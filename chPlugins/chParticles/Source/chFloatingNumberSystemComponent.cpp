/************************************************************************/
/**
 * @file chFloatingNumberSystemComponent.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Damage numbers and other short texts that pop up, fly and fade on the GPU.
 */
/************************************************************************/
#include "chFloatingNumberSystemComponent.h"

#include <bit>

#include "chAlgorithm.h"
#include "chAssetFile.h"
#include "chAssetManager.h"
#include "chFileStream.h"
#include "chFloatingNumberGpu.h"
#include "chFont.h"
#include "chFontAsset.h"
#include "chGameObject.h"
#include "chIBuffer.h"
#include "chIGraphicsAPI.h"
#include "chLogger.h"
#include "chMath.h"
#include "chMatrix4.h"
#include "chScene.h"
#include "chSpriteDraw.h"
#include "chSpriteShader.h"

namespace chEngineSDK {

CH_LOG_DECLARE_STATIC(FloatingNumbersLog, All);

namespace {
struct SystemFileData
{
  UUID font;
  float size;
  uint32 capacity;
  int16 sortingLayer;
  int16 order;
  uint8 blendMode;
  uint8 bMonospaceDigits;
  uint8 notation;
  uint8 bTrimZeros;
  int8 decimals;
  int8 plainDecimals;
  uint8 tierCount;
  uint8 padding[5];
  double plainBelow;
  float angle;
  float spread;
  float speed;
  float speedVariance;
  float gravity[2];
  float drag;
  float lifetime;
  float lifetimeVariance;
  float fadeIn;
  float fadeOut;
  uint32 padding2;
};
static_assert(sizeof(SystemFileData) == 96, "SystemFileData must have no padding");

struct TierFileData
{
  double minValue;
  float color[4];
  float flashColor[4];
  float outlineColor[4];
  float outlineWidth;
  float scale;
  float popScale;
  float popDuration;
  float shakeAmplitude;
  float shakeFrequency;
  float shakeDuration;
  float flashDuration;
  float flashRate;
  uint32 padding;
};
static_assert(sizeof(TierFileData) == 96, "TierFileData must have no padding");

NODISCARD SPtr<FontAsset>
findFont(const UUID& id)
{
  if (id.isNull() || !AssetManager::isStarted()) {
    return nullptr;
  }
  const SPtr<IAsset> asset = AssetManager::instance().getAsset(id);
  SPtr<FontAsset> font = asset ? asset->as<FontAsset>() : nullptr;
  if (font && !font->isLoaded() && !AssetManager::instance().syncLoadAsset(font)) {
    return nullptr;
  }
  return font;
}

void
copyColor(const LinearColor& color, float (&out)[4])
{
  out[0] = color.r;
  out[1] = color.g;
  out[2] = color.b;
  out[3] = color.a;
}

NODISCARD LinearColor
toColor(const float (&values)[4])
{
  return LinearColor(values[0], values[1], values[2], values[3]);
}

} // namespace

/*
 */
FloatingNumberSystemComponent::FloatingNumberSystemComponent()
{
  setUpdateFlags(ComponentUpdate::Frame);
  // So Test Spawn shows in the editor.
  setUpdatesWhileEditing(true);
  m_tiers.push_back(FloatingNumberTier{});
}

/*
 */
FloatingNumberSystemComponent::~FloatingNumberSystemComponent() = default;

/*
 */
bool
FloatingNumberSystemComponent::spawn(double value,
                                     const Vector2& position,
                                     const FloatingNumberSpawn& options)
{
  ANSICHAR buffer[NumberFormat::MAX_CHARS];
  const StringView text = NumberFormat::format(buffer, value, m_numberFormat);
  const uint32 tier = options.tier >= 0
                          ? Math::min(static_cast<uint32>(options.tier),
                                      static_cast<uint32>(m_tiers.size()) - 1)
                          : findTier(value);
  return spawnRuns(text, position, tier, options);
}

/*
 */
bool
FloatingNumberSystemComponent::spawnText(StringView text,
                                         const Vector2& position,
                                         const FloatingNumberSpawn& options)
{
  const uint32 tier = Math::min(static_cast<uint32>(Math::max(options.tier, 0)),
                                static_cast<uint32>(m_tiers.size()) - 1);
  return spawnRuns(text, position, tier, options);
}

/*
 */
void
FloatingNumberSystemComponent::clear()
{
  m_ring.clear();
  m_maxGlyphs = 1;
}

/*
 */
uint32
FloatingNumberSystemComponent::findTier(double value) const noexcept
{
  uint32 tier = 0;
  for (uint32 i = 1; i < m_tiers.size(); ++i) {
    if (value >= m_tiers[i].minValue) {
      tier = i;
    }
  }
  return tier;
}

/*
 */
uint32
FloatingNumberSystemComponent::getLiveCount() const noexcept
{
  return m_ring.getLiveCount();
}

/*
 */
void
FloatingNumberSystemComponent::setCapacity(uint32 capacity)
{
  m_capacity = Math::clamp(capacity, 1u, MAX_CAPACITY);
  clear();
  if (isRegistered()) {
    onUnregister();
    onRegister();
  }
}

/*
 */
void
FloatingNumberSystemComponent::setFont(const SPtr<FontAsset>& font)
{
  m_font = font;
  m_fontId = font ? font->getUUID() : UUID::null();
  // Runs keep glyph slots of the font they were made with.
  clear();
}

/*
 */
void
FloatingNumberSystemComponent::setFontId(const UUID& fontId)
{
  setFont(findFont(fontId));
  m_fontId = fontId;
}

/*
 */
void
FloatingNumberSystemComponent::setSize(float size)
{
  m_size = size;
}

/*
 */
void
FloatingNumberSystemComponent::setSortingLayer(int16 sortingLayer)
{
  m_sortingLayer = sortingLayer;
}

/*
 */
void
FloatingNumberSystemComponent::setOrder(int16 order)
{
  m_order = order;
}

/*
 */
void
FloatingNumberSystemComponent::setBlendMode(SpriteBlendMode blendMode)
{
  m_blendMode = blendMode;
}

/*
 */
void
FloatingNumberSystemComponent::setMonospaceDigits(bool bMonospace)
{
  m_bMonospaceDigits = bMonospace;
}

/*
 */
void
FloatingNumberSystemComponent::setMotion(const AgeMotion& motion)
{
  m_motion = motion.getClamped();
  m_bMotionDirty = true;
}

/*
 */
void
FloatingNumberSystemComponent::setTiers(Span<const FloatingNumberTier> tiers)
{
  constexpr SIZE_T kMaxTiers = GpuFloatingNumberMotion::MAX_TIERS;
  m_tiers.assign(tiers.begin(), tiers.begin() + Math::min(tiers.size(), kMaxTiers));
  if (m_tiers.empty()) {
    m_tiers.push_back(FloatingNumberTier{});
  }
  Algorithm::sort(m_tiers, [](const FloatingNumberTier& a, const FloatingNumberTier& b) {
    return a.minValue < b.minValue;
  });
  m_bMotionDirty = true;
}

/*
 */
void
FloatingNumberSystemComponent::setNumberFormat(const NumberFormatDesc& format)
{
  m_numberFormat = format;
}

#if USING(CH_EDITOR)
/*
 */
void
FloatingNumberSystemComponent::testSpawn()
{
  if (!isRegistered()) {
    return;
  }
  const Matrix4& world = getOwner()->getTransform().getWorldMatrix();
  const Vector2 origin(world.at(3, 1), world.at(3, 2));
  // Values from the first tier's to past the last one's, so every tier shows.
  const double low = Math::max(m_tiers.front().minValue, 1.0);
  const double high = Math::max(m_tiers.back().minValue * 3.0, low * 10.0);
  for (uint32 i = 0; i < TEST_SPAWN_COUNT; ++i) {
    const double fraction = static_cast<double>(i) / (TEST_SPAWN_COUNT - 1);
    const double value = low * Math::pow(static_cast<float>(high / low), static_cast<float>(fraction));
    const Vector2 jitter((m_random.getRandom01() * 2.0f - 1.0f) * 1.5f,
                         (m_random.getRandom01() * 2.0f - 1.0f) * 0.5f);
    CH_PARAMETER_UNUSED(spawn(value, origin + jitter));
  }
}
#endif // USING(CH_EDITOR)

/*
 */
void
FloatingNumberSystemComponent::update(float deltaTime)
{
  if (!isRegistered()) {
    return;
  }
  // Runs die in about the order they were made, so the live ones are one part of the ring.
  if (m_ring.advance(deltaTime) == 0) {
    m_maxGlyphs = 1;
    return;
  }

  Font* font = getShownFont();
  if (font && font == m_writtenFont && font->getRevision() != m_writtenRevision) {
    refreshFontIndices(*font);
  }
  if (m_bMotionDirty) {
    rebuildGpuMotion();
  }
  m_ring.upload();
  m_ring.forEachLiveRange([this](uint32 first, uint32 count) { addBatch(first, count); });
}

/*
 */
void
FloatingNumberSystemComponent::serialize(DataStream& stream) const
{
  const uint32 version = SERIALIZATION_VERSION;
  SystemFileData data{};
  data.font = m_fontId;
  data.size = m_size;
  data.capacity = m_capacity;
  data.sortingLayer = m_sortingLayer;
  data.order = m_order;
  data.blendMode = static_cast<uint8>(m_blendMode);
  data.bMonospaceDigits = m_bMonospaceDigits ? 1 : 0;
  data.notation = static_cast<uint8>(m_numberFormat.notation);
  data.bTrimZeros = m_numberFormat.bTrimZeros ? 1 : 0;
  data.decimals = static_cast<int8>(Math::clamp(m_numberFormat.decimals, 0, NumberFormat::MAX_DECIMALS));
  data.plainDecimals =
      static_cast<int8>(Math::clamp(m_numberFormat.plainDecimals, 0, NumberFormat::MAX_DECIMALS));
  data.tierCount = static_cast<uint8>(m_tiers.size());
  data.plainBelow = m_numberFormat.plainBelow;
  data.angle = m_motion.angle;
  data.spread = m_motion.spread;
  data.speed = m_motion.speed;
  data.speedVariance = m_motion.speedVariance;
  data.gravity[0] = m_motion.gravity.x;
  data.gravity[1] = m_motion.gravity.y;
  data.drag = m_motion.drag;
  data.lifetime = m_motion.lifetime;
  data.lifetimeVariance = m_motion.lifetimeVariance;
  data.fadeIn = m_motion.fadeIn;
  data.fadeOut = m_motion.fadeOut;
  stream.write(&version, sizeof(version));
  stream.write(&data, sizeof(data));
  for (const FloatingNumberTier& tier : m_tiers) {
    TierFileData tierData{};
    tierData.minValue = tier.minValue;
    copyColor(tier.color, tierData.color);
    copyColor(tier.flashColor, tierData.flashColor);
    copyColor(tier.outlineColor, tierData.outlineColor);
    tierData.outlineWidth = tier.outlineWidth;
    tierData.scale = tier.scale;
    tierData.popScale = tier.popScale;
    tierData.popDuration = tier.popDuration;
    tierData.shakeAmplitude = tier.shakeAmplitude;
    tierData.shakeFrequency = tier.shakeFrequency;
    tierData.shakeDuration = tier.shakeDuration;
    tierData.flashDuration = tier.flashDuration;
    tierData.flashRate = tier.flashRate;
    stream.write(&tierData, sizeof(tierData));
  }
}

/*
 */
bool
FloatingNumberSystemComponent::deserialize(DataStream& stream)
{
  uint32 version = 0;
  SystemFileData data;
  if (!AssetFile::readValue(stream, version) || version != SERIALIZATION_VERSION ||
      !AssetFile::readValue(stream, data) ||
      data.blendMode >= static_cast<uint8>(SpriteBlendMode::COUNT) ||
      data.notation >= static_cast<uint8>(NumberNotation::COUNT) || data.tierCount == 0 ||
      data.tierCount > GpuFloatingNumberMotion::MAX_TIERS || !Math::isFinite(data.size)) {
    return false;
  }
  Vector<FloatingNumberTier> tiers(data.tierCount);
  for (FloatingNumberTier& tier : tiers) {
    TierFileData tierData;
    if (!AssetFile::readValue(stream, tierData)) {
      return false;
    }
    tier = {.minValue = tierData.minValue,
            .color = toColor(tierData.color),
            .flashColor = toColor(tierData.flashColor),
            .outlineColor = toColor(tierData.outlineColor),
            .outlineWidth = tierData.outlineWidth,
            .scale = tierData.scale,
            .popScale = tierData.popScale,
            .popDuration = tierData.popDuration,
            .shakeAmplitude = tierData.shakeAmplitude,
            .shakeFrequency = tierData.shakeFrequency,
            .shakeDuration = tierData.shakeDuration,
            .flashDuration = tierData.flashDuration,
            .flashRate = tierData.flashRate};
  }

  m_size = data.size;
  m_capacity = Math::clamp(data.capacity, 1u, MAX_CAPACITY);
  m_sortingLayer = data.sortingLayer;
  m_order = data.order;
  m_blendMode = static_cast<SpriteBlendMode>(data.blendMode);
  m_bMonospaceDigits = data.bMonospaceDigits != 0;
  m_numberFormat = {.notation = static_cast<NumberNotation>(data.notation),
                    .decimals = data.decimals,
                    .bTrimZeros = data.bTrimZeros != 0,
                    .plainBelow = data.plainBelow,
                    .plainDecimals = data.plainDecimals};
  setMotion({.angle = data.angle,
             .spread = data.spread,
             .speed = data.speed,
             .speedVariance = data.speedVariance,
             .gravity = Vector2(data.gravity[0], data.gravity[1]),
             .drag = data.drag,
             .lifetime = data.lifetime,
             .lifetimeVariance = data.lifetimeVariance,
             .fadeIn = data.fadeIn,
             .fadeOut = data.fadeOut});
  setTiers(tiers);
  m_font = findFont(data.font);
  m_fontId = data.font;
  if (!m_font && !data.font.isNull()) {
    CH_LOG_WARNING(FloatingNumbersLog,
                   "Floating numbers lost their font {0}; they use the default one.",
                   data.font);
  }
  clear();
  return true;
}

/*
 */
void
FloatingNumberSystemComponent::collectReferences(Vector<UUID>& outReferences) const
{
  if (!m_fontId.isNull()) {
    outReferences.push_back(m_fontId);
  }
}

/*
 */
void
FloatingNumberSystemComponent::onRegister()
{
  // Every buffer is sized once, so spawning never allocates.
  m_ring.create(m_capacity);
  m_maxGlyphs = 1;
  m_bMotionDirty = true;
  if (IGraphicsAPI::isStarted()) {
    m_motionBuffer = IGraphicsAPI::instance().createBuffer(
        {.size = sizeof(GpuFloatingNumberMotion),
         .usage = BufferUsage::StorageBuffer,
         .memoryUsage = MemoryUsage::GpuOnly});
  }
}

/*
 */
void
FloatingNumberSystemComponent::onUnregister()
{
  m_motionBuffer.reset();
  m_ring.release();
  m_maxGlyphs = 1;
}

/*
 */
Font*
FloatingNumberSystemComponent::getShownFont() const
{
  if (m_font && m_font->getFont()) {
    return m_font->getFont().get();
  }
  return Font::getDefault().get();
}

/*
 */
bool
FloatingNumberSystemComponent::spawnRuns(StringView text,
                                         const Vector2& position,
                                         uint32 tier,
                                         const FloatingNumberSpawn& options)
{
  Font* font = getShownFont();
  if (!font || !isRegistered()) {
    return false;
  }
  if (font != m_writtenFont) {
    // Runs keep glyph slots of the font they were made with.
    clear();
    m_writtenFont = font;
  }

  GlyphRunItem item;
  const GlyphRunDesc desc{.position = position,
                          .size = m_size * options.scale,
                          .color = options.tint,
                          .bMonospaceDigits = m_bMonospaceDigits};
  if (!SpriteDraw::makeGlyphRun(*font, text, desc, item)) {
    return false;
  }
  m_writtenRevision = font->getRevision();

  const AgeBirth birth = m_motion.makeBirth(m_random);

  GpuGlyphRun& run = item.run;
  run.userData[0] = std::bit_cast<uint32>(m_ring.getTime());
  run.userData[1] = birth.getPackedVelocity();
  run.userData[2] = std::bit_cast<uint32>(birth.lifetime);
  run.userData[3] = (tier & FloatingNumberGpu::TIER_MASK) |
                    (birth.seed << FloatingNumberGpu::SEED_SHIFT);

  // Written first, so the fill draws over it.
  const uint32 edge = SpriteDraw::makeOutlineEdge(*font, m_tiers[tier].outlineWidth);
  if (edge > 0) {
    GpuGlyphRun outline = run;
    outline.textureAndFlags =
        (run.textureAndFlags & ~(GpuSpriteInstance::EDGE_MASK << GpuSpriteInstance::EDGE_SHIFT)) |
        (edge << GpuSpriteInstance::EDGE_SHIFT);
    outline.userData[3] |= FloatingNumberGpu::OUTLINE_BIT;
    writeRun(outline, birth.lifetime);
  }
  writeRun(run, birth.lifetime);
  return true;
}

/*
 */
void
FloatingNumberSystemComponent::writeRun(const GpuGlyphRun& run, float lifetime)
{
  m_ring.write(run, lifetime);
  m_maxGlyphs = Math::max(m_maxGlyphs, run.glyphCount);
}

/*
 */
void
FloatingNumberSystemComponent::rebuildGpuMotion()
{
  m_bMotionDirty = false;
  GpuFloatingNumberMotion& motion = m_gpuMotion;
  motion = {};
  motion.motion = m_motion.toGpu();
  motion.tierCount = static_cast<uint32>(m_tiers.size());
  for (SIZE_T i = 0; i < m_tiers.size(); ++i) {
    const FloatingNumberTier& tier = m_tiers[i];
    GpuFloatingNumberTier& gpu = motion.tiers[i];
    copyColor(tier.color, gpu.color);
    copyColor(tier.flashColor, gpu.flashColor);
    copyColor(tier.outlineColor, gpu.outlineColor);
    gpu.scale = tier.scale;
    gpu.popScale = Math::max(tier.popScale, 0.0f);
    gpu.popDuration = Math::max(tier.popDuration, 0.0f);
    gpu.shakeAmplitude = tier.shakeAmplitude;
    gpu.shakeFrequency = tier.shakeFrequency;
    gpu.shakeDuration = Math::max(tier.shakeDuration, 0.0f);
    gpu.flashDuration = Math::max(tier.flashDuration, 0.0f);
    gpu.flashRate = tier.flashRate;
  }
  if (m_motionBuffer) {
    m_motionBuffer->update(&motion, sizeof(motion));
  }
}

/*
 */
void
FloatingNumberSystemComponent::refreshFontIndices(Font& font)
{
  const uint32 atlasIndex = font.prepareGpuAtlas();
  const uint32 tableIndex = font.prepareGpuGlyphTable();
  constexpr uint32 kIndexMask = GpuSpriteInstance::TEXTURE_INDEX_MASK;
  for (uint32 i = 0; i < m_ring.getLiveCount(); ++i) {
    GpuGlyphRun& run = m_ring.getLiveEntry(i);
    run.textureAndFlags = (run.textureAndFlags & ~kIndexMask) | (atlasIndex & kIndexMask);
    run.glyphTableIndex = tableIndex;
  }
  m_writtenRevision = font.getRevision();
  // Every live run changed, so they all go up again.
  m_ring.markLivePending();
}

/*
 */
void
FloatingNumberSystemComponent::addBatch(uint32 first, uint32 count)
{
  const IBuffer* runBuffer = m_ring.getBuffer();
  if (!runBuffer || !m_motionBuffer || count == 0) {
    return;
  }
  const uint32 glyphs = Math::clamp(m_maxGlyphs, 1u, GpuGlyphRun::MAX_GLYPHS);
  CustomSpriteBatch batch{.shader = &FloatingNumberGpu::getShader(),
                          .vertexCount = 6 * glyphs,
                          .instanceCount = count,
                          .sortingLayer = m_sortingLayer,
                          .order = m_order,
                          .blendMode = m_blendMode};
  batch.setParameters(FloatingNumberBatchParameters{
      .runBufferIndex = runBuffer->getBindlessIndex(),
      .firstRun = first,
      .motionIndex = m_motionBuffer->getBindlessIndex(),
      .time = m_ring.getTime()});
  getOwner()->getScene()->getSpriteDraw().customBatch(batch);
}

} // namespace chEngineSDK
