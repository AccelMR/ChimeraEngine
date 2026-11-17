/************************************************************************/
/**
 * @file chParticleSystemComponent.cpp
 * @author AccelMR
 * @date 2026/11/16
 * @brief
 *  Particles written once when they are born and moved, colored and sized by the GPU.
 */
/************************************************************************/
#include "chParticleSystemComponent.h"

#include <bit>

#include "chAlgorithm.h"
#include "chAngle.h"
#include "chAssetFile.h"
#include "chAssetManager.h"
#include "chFileStream.h"
#include "chGameObject.h"
#include "chIBuffer.h"
#include "chIGraphicsAPI.h"
#include "chLogger.h"
#include "chMath.h"
#include "chMatrix4.h"
#include "chScene.h"
#include "chSpriteAtlasAsset.h"
#include "chSpriteDraw.h"
#include "chSpriteShader.h"
#include "chTextureAsset.h"

namespace chEngineSDK {

CH_LOG_DECLARE_STATIC(ParticlesLog, All);

namespace {
constexpr uint32 kMaxKeys = GpuParticleLook::MAX_KEYS;
constexpr uint32 kMaxFrames = GpuParticleLook::MAX_FRAMES;

struct SystemFileData
{
  UUID texture;
  UUID atlas;
  float uvRect[4];
  float size[2];
  float minScale;
  float maxScale;
  float color[4];
  float color2[4];
  float rotation;
  float rotationVariance;
  float spin;
  float spinVariance;
  float depth;
  int16 sortingLayer;
  int16 order;
  uint8 blendMode;
  uint8 filter;
  uint8 rotationMode;
  uint8 shape;
  uint8 bLoop;
  uint8 bPlayOnStart;
  uint8 bLocalSpace;
  uint8 padding;
  float rate;
  float duration;
  float shapeSize[2];
  uint32 firstFrame;
  uint32 frameCount;
  float framesPerSecond;
  uint32 capacity;
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
  uint8 burstCount;
  uint8 colorKeyCount;
  uint8 sizeKeyCount;
  uint8 padding2;
};
static_assert(sizeof(SystemFileData) == 208, "SystemFileData must have no padding");

struct BurstFileData
{
  float time;
  uint32 count;
  uint32 cycles;
  float interval;
};
static_assert(sizeof(BurstFileData) == 16, "BurstFileData must have no padding");

struct ColorKeyFileData
{
  float time;
  float color[4];
};
static_assert(sizeof(ColorKeyFileData) == 20, "ColorKeyFileData must have no padding");

struct SizeKeyFileData
{
  float time;
  float size;
};
static_assert(sizeof(SizeKeyFileData) == 8, "SizeKeyFileData must have no padding");

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

NODISCARD float
toRadians(float degrees)
{
  return Radian(Degree(degrees)).valueRadian();
}

template<typename Asset>
NODISCARD SPtr<Asset>
findLoaded(const UUID& id)
{
  if (id.isNull() || !AssetManager::isStarted()) {
    return nullptr;
  }
  AssetManager& assetManager = AssetManager::instance();
  const SPtr<IAsset> asset = assetManager.getAsset(id);
  SPtr<Asset> typed = asset ? asset->as<Asset>() : nullptr;
  if (typed && !typed->isLoaded() && !assetManager.syncLoadAsset(typed)) {
    return nullptr;
  }
  return typed;
}
} // namespace

/*
 */
ParticleSystemComponent::ParticleSystemComponent()
{
  setUpdateFlags(ComponentUpdate::Frame);
  // So Test Emit and Play show in the editor.
  setUpdatesWhileEditing(true);
}

/*
 */
ParticleSystemComponent::~ParticleSystemComponent() = default;

/*
 */
uint32
ParticleSystemComponent::emit(uint32 count)
{
  if (!isRegistered() || m_ring.getCapacity() == 0) {
    return 0;
  }
  const EmitterPlace place = getEmitterPlace();
  for (uint32 i = 0; i < count; ++i) {
    writeParticle(pickShapePoint() * place.scale, place, LinearColor::WHITE);
  }
  return count;
}

/*
 */
uint32
ParticleSystemComponent::emitAt(uint32 count,
                                const Vector2& position,
                                const LinearColor& tint)
{
  if (!isRegistered() || m_ring.getCapacity() == 0) {
    return 0;
  }
  const EmitterPlace place = getEmitterPlace();
  // writeParticle takes a point from the emitter, before its turn.
  const Vector2 offset = position - place.position;
  const Vector2 local(offset.x * place.rotation.x + offset.y * place.rotation.y,
                      offset.y * place.rotation.x - offset.x * place.rotation.y);
  for (uint32 i = 0; i < count; ++i) {
    writeParticle(local, place, tint);
  }
  return count;
}

/*
 */
void
ParticleSystemComponent::play()
{
  m_bPlaying = true;
  m_cycleTime = 0.0f;
  m_rateDebt = 0.0f;
}

/*
 */
void
ParticleSystemComponent::stop(bool bClear)
{
  m_bPlaying = false;
  if (bClear) {
    clear();
  }
}

/*
 */
void
ParticleSystemComponent::clear()
{
  m_ring.clear();
}

/*
 */
void
ParticleSystemComponent::setCapacity(uint32 capacity)
{
  m_capacity = Math::clamp(capacity, 1u, MAX_CAPACITY);
  if (isRegistered()) {
    m_ring.create(m_capacity);
  }
}

/*
 */
void
ParticleSystemComponent::setEmission(const ParticleEmission& emission)
{
  m_emission = emission;
  m_emission.rate = Math::max(emission.rate, 0.0f);
  m_emission.duration = Math::max(emission.duration, AgeMotion::MIN_LIFETIME);
  m_emission.shapeSize = Vector2(Math::max(emission.shapeSize.x, 0.0f),
                                 Math::max(emission.shapeSize.y, 0.0f));
}

/*
 */
void
ParticleSystemComponent::setBursts(Span<const ParticleBurst> bursts)
{
  m_bursts.assign(bursts.begin(),
                  bursts.begin() + Math::min(bursts.size(), static_cast<SIZE_T>(MAX_BURSTS)));
  for (ParticleBurst& burst : m_bursts) {
    burst.time = Math::max(burst.time, 0.0f);
    burst.cycles = Math::max(burst.cycles, 1u);
    burst.interval = Math::max(burst.interval, 0.0f);
  }
}

/*
 */
void
ParticleSystemComponent::setMotion(const AgeMotion& motion)
{
  m_motion = motion.getClamped();
  m_bLookDirty = true;
}

/*
 */
void
ParticleSystemComponent::setLook(const ParticleLook& look)
{
  const bool bTextureChanged = look.texture != m_look.texture;
  m_look = look;
  m_look.size = Vector2(Math::max(look.size.x, 0.0f), Math::max(look.size.y, 0.0f));
  m_look.minScale = Math::max(look.minScale, 0.0f);
  m_look.maxScale = Math::max(look.maxScale, 0.0f);
  if (bTextureChanged) {
    resolveAssets();
  }
  m_bLookDirty = true;
}

/*
 */
void
ParticleSystemComponent::setTextureId(const UUID& texture)
{
  ParticleLook look = m_look;
  look.texture = texture;
  setLook(look);
}

/*
 */
void
ParticleSystemComponent::setFlipbook(const ParticleFlipbook& flipbook)
{
  m_flipbook = flipbook;
  m_flipbook.framesPerSecond = Math::max(flipbook.framesPerSecond, 0.0f);
  resolveAssets();
  m_bLookDirty = true;
}

/*
 */
void
ParticleSystemComponent::setColorGradient(Span<const GradientKey> keys)
{
  m_colorKeys.assign(keys.begin(),
                     keys.begin() + Math::min(keys.size(), static_cast<SIZE_T>(kMaxKeys)));
  Algorithm::sort(m_colorKeys, [](const GradientKey& a, const GradientKey& b) {
    return a.time < b.time;
  });
  m_bLookDirty = true;
}

/*
 */
void
ParticleSystemComponent::setSizeCurve(Span<const CurveKey> keys)
{
  m_sizeKeys.assign(keys.begin(),
                    keys.begin() + Math::min(keys.size(), static_cast<SIZE_T>(kMaxKeys)));
  Algorithm::sort(m_sizeKeys, [](const CurveKey& a, const CurveKey& b) {
    return a.time < b.time;
  });
  m_bLookDirty = true;
}

#if USING(CH_EDITOR)
/*
 */
void
ParticleSystemComponent::testEmit()
{
  CH_PARAMETER_UNUSED(emit(TEST_EMIT_COUNT));
}

/*
 */
void
ParticleSystemComponent::stopAndClear()
{
  stop(true);
}
#endif // USING(CH_EDITOR)

/*
 */
void
ParticleSystemComponent::update(float deltaTime)
{
  if (!isRegistered()) {
    return;
  }
  if (!m_bStartChecked && getOwner()->getScene()->isPlaying()) {
    m_bStartChecked = true;
    if (m_emission.bPlayOnStart) {
      play();
    }
  }

  // Particles die in about the order they were born, so the live ones are one part of the
  // ring.
  CH_PARAMETER_UNUSED(m_ring.advance(deltaTime));
  if (m_bPlaying) {
    emitOverTime(deltaTime);
  }
  if (m_ring.getLiveCount() == 0) {
    return;
  }

  // A texture's bindless index can come after the asset loaded.
  const TextureAsset* texture =
      m_atlas && m_atlas->getTexture() ? m_atlas->getTexture().get() : m_texture.get();
  if (SpriteDraw::makeTextureFlags(texture, m_look.filter) != m_textureAndFlags) {
    m_bLookDirty = true;
  }
  if (m_bLookDirty) {
    rebuildGpuLook();
  }
  m_ring.upload();
  const EmitterPlace place = getEmitterPlace();
  m_ring.forEachLiveRange(
      [this, &place](uint32 first, uint32 count) { addBatch(first, count, place); });
}

/*
 */
void
ParticleSystemComponent::serialize(DataStream& stream) const
{
  const uint32 version = SERIALIZATION_VERSION;
  SystemFileData data{};
  data.texture = m_look.texture;
  data.atlas = m_flipbook.atlas;
  for (uint32 i = 0; i < 4; ++i) {
    data.uvRect[i] = m_look.uvRect[i];
  }
  data.size[0] = m_look.size.x;
  data.size[1] = m_look.size.y;
  data.minScale = m_look.minScale;
  data.maxScale = m_look.maxScale;
  copyColor(m_look.color, data.color);
  copyColor(m_look.color2, data.color2);
  data.rotation = m_look.rotation;
  data.rotationVariance = m_look.rotationVariance;
  data.spin = m_look.spin;
  data.spinVariance = m_look.spinVariance;
  data.depth = m_look.depth;
  data.sortingLayer = m_look.sortingLayer;
  data.order = m_look.order;
  data.blendMode = static_cast<uint8>(m_look.blendMode);
  data.filter = static_cast<uint8>(m_look.filter);
  data.rotationMode = static_cast<uint8>(m_look.rotationMode);
  data.shape = static_cast<uint8>(m_emission.shape);
  data.bLoop = m_emission.bLoop ? 1 : 0;
  data.bPlayOnStart = m_emission.bPlayOnStart ? 1 : 0;
  data.bLocalSpace = m_emission.bLocalSpace ? 1 : 0;
  data.rate = m_emission.rate;
  data.duration = m_emission.duration;
  data.shapeSize[0] = m_emission.shapeSize.x;
  data.shapeSize[1] = m_emission.shapeSize.y;
  data.firstFrame = m_flipbook.firstFrame;
  data.frameCount = m_flipbook.frameCount;
  data.framesPerSecond = m_flipbook.framesPerSecond;
  data.capacity = m_capacity;
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
  data.burstCount = static_cast<uint8>(m_bursts.size());
  data.colorKeyCount = static_cast<uint8>(m_colorKeys.size());
  data.sizeKeyCount = static_cast<uint8>(m_sizeKeys.size());
  stream.write(&version, sizeof(version));
  stream.write(&data, sizeof(data));
  for (const ParticleBurst& burst : m_bursts) {
    const BurstFileData burstData{.time = burst.time,
                                  .count = burst.count,
                                  .cycles = burst.cycles,
                                  .interval = burst.interval};
    stream.write(&burstData, sizeof(burstData));
  }
  for (const GradientKey& key : m_colorKeys) {
    ColorKeyFileData keyData{};
    keyData.time = key.time;
    copyColor(key.color, keyData.color);
    stream.write(&keyData, sizeof(keyData));
  }
  for (const CurveKey& key : m_sizeKeys) {
    const SizeKeyFileData keyData{.time = key.time, .size = key.value};
    stream.write(&keyData, sizeof(keyData));
  }
}

/*
 */
bool
ParticleSystemComponent::deserialize(DataStream& stream)
{
  uint32 version = 0;
  SystemFileData data;
  if (!AssetFile::readValue(stream, version) || version != SERIALIZATION_VERSION ||
      !AssetFile::readValue(stream, data) ||
      data.blendMode >= static_cast<uint8>(SpriteBlendMode::COUNT) ||
      data.filter >= static_cast<uint8>(SpriteFilter::COUNT) ||
      data.rotationMode >= static_cast<uint8>(ParticleRotation::COUNT) ||
      data.shape >= static_cast<uint8>(ParticleShape::COUNT) || data.burstCount > MAX_BURSTS ||
      data.colorKeyCount > kMaxKeys || data.sizeKeyCount > kMaxKeys) {
    return false;
  }
  Vector<ParticleBurst> bursts(data.burstCount);
  for (ParticleBurst& burst : bursts) {
    BurstFileData burstData;
    if (!AssetFile::readValue(stream, burstData)) {
      return false;
    }
    burst = {.time = burstData.time,
             .count = burstData.count,
             .cycles = burstData.cycles,
             .interval = burstData.interval};
  }
  Vector<GradientKey> colorKeys(data.colorKeyCount);
  for (GradientKey& key : colorKeys) {
    ColorKeyFileData keyData;
    if (!AssetFile::readValue(stream, keyData)) {
      return false;
    }
    key = {.time = keyData.time, .color = toColor(keyData.color)};
  }
  Vector<CurveKey> sizeKeys(data.sizeKeyCount);
  for (CurveKey& key : sizeKeys) {
    SizeKeyFileData keyData;
    if (!AssetFile::readValue(stream, keyData)) {
      return false;
    }
    key = {.time = keyData.time, .value = keyData.size};
  }

  m_capacity = Math::clamp(data.capacity, 1u, MAX_CAPACITY);
  ParticleLook look;
  look.texture = data.texture;
  for (uint32 i = 0; i < 4; ++i) {
    look.uvRect[i] = data.uvRect[i];
  }
  look.size = Vector2(data.size[0], data.size[1]);
  look.minScale = data.minScale;
  look.maxScale = data.maxScale;
  look.color = toColor(data.color);
  look.color2 = toColor(data.color2);
  look.rotationMode = static_cast<ParticleRotation>(data.rotationMode);
  look.rotation = data.rotation;
  look.rotationVariance = data.rotationVariance;
  look.spin = data.spin;
  look.spinVariance = data.spinVariance;
  look.depth = data.depth;
  look.sortingLayer = data.sortingLayer;
  look.order = data.order;
  look.blendMode = static_cast<SpriteBlendMode>(data.blendMode);
  look.filter = static_cast<SpriteFilter>(data.filter);
  m_look = {};
  setLook(look);
  setEmission({.rate = data.rate,
               .duration = data.duration,
               .bLoop = data.bLoop != 0,
               .bPlayOnStart = data.bPlayOnStart != 0,
               .bLocalSpace = data.bLocalSpace != 0,
               .shape = static_cast<ParticleShape>(data.shape),
               .shapeSize = Vector2(data.shapeSize[0], data.shapeSize[1])});
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
  setFlipbook({.atlas = data.atlas,
               .firstFrame = data.firstFrame,
               .frameCount = data.frameCount,
               .framesPerSecond = data.framesPerSecond});
  setBursts(bursts);
  setColorGradient(colorKeys);
  setSizeCurve(sizeKeys);
  if (!m_texture && !data.texture.isNull()) {
    CH_LOG_WARNING(ParticlesLog, "Particles lost their texture {0}.", data.texture);
  }
  if (!m_atlas && !data.atlas.isNull()) {
    CH_LOG_WARNING(ParticlesLog, "Particles lost their flipbook atlas {0}.", data.atlas);
  }
  return true;
}

/*
 */
void
ParticleSystemComponent::collectReferences(Vector<UUID>& outReferences) const
{
  if (!m_look.texture.isNull()) {
    outReferences.push_back(m_look.texture);
  }
  if (!m_flipbook.atlas.isNull()) {
    outReferences.push_back(m_flipbook.atlas);
  }
}

/*
 */
void
ParticleSystemComponent::onRegister()
{
  // Every buffer is sized once, so emitting never allocates.
  m_ring.create(m_capacity);
  m_bStartChecked = false;
  m_bLookDirty = true;
  if (IGraphicsAPI::isStarted()) {
    m_lookBuffer = IGraphicsAPI::instance().createBuffer(
        {.size = sizeof(GpuParticleLook),
         .usage = BufferUsage::StorageBuffer,
         .memoryUsage = MemoryUsage::GpuOnly});
  }
}

/*
 */
void
ParticleSystemComponent::onUnregister()
{
  m_lookBuffer.reset();
  m_ring.release();
  m_bPlaying = false;
}

/*
 */
ParticleSystemComponent::EmitterPlace
ParticleSystemComponent::getEmitterPlace() const
{
  const Matrix4 world = getOwner()->getCurrentWorldMatrix();
  // The right axis is the image's right in the YZ plane; its turn and length are the
  // emitter's turn and scale.
  const Vector2 right(world.at(1, 1), world.at(1, 2));
  const float scale = right.magnitude();
  EmitterPlace place;
  place.position = Vector2(world.at(3, 1), world.at(3, 2));
  place.rotation = scale > 0.0f ? right * (1.0f / scale) : Vector2(1.0f, 0.0f);
  place.scale = scale;
  return place;
}

/*
 */
Vector2
ParticleSystemComponent::pickShapePoint()
{
  const Vector2& size = m_emission.shapeSize;
  switch (m_emission.shape) {
  case ParticleShape::Circle:
  case ParticleShape::CircleEdge: {
    const float angle = m_random.getRandom01() * Math::TWO_PI;
    // The square root spreads points evenly over the area.
    const float distance = m_emission.shape == ParticleShape::Circle
                               ? Math::sqrt(m_random.getRandom01()) * size.x
                               : size.x;
    float sine = 0.0f;
    float cosine = 1.0f;
    Math::sinCos(angle, sine, cosine);
    return Vector2(cosine * distance, sine * distance);
  }
  case ParticleShape::Rectangle:
    return Vector2((m_random.getRandom01() * 2.0f - 1.0f) * size.x,
                   (m_random.getRandom01() * 2.0f - 1.0f) * size.y);
  case ParticleShape::Line:
    return Vector2((m_random.getRandom01() * 2.0f - 1.0f) * size.x, 0.0f);
  default:
    return Vector2::ZERO;
  }
}

/*
 */
void
ParticleSystemComponent::writeParticle(const Vector2& position,
                                       const EmitterPlace& place,
                                       const LinearColor& tint)
{
  AgeBirth birth = m_motion.makeBirth(m_random);
  Vector2 start = position;
  if (!m_emission.bLocalSpace) {
    // In world space the emitter's turn and place are applied now, once.
    const Vector2& rotation = place.rotation;
    start = place.position + Vector2(position.x * rotation.x - position.y * rotation.y,
                                     position.x * rotation.y + position.y * rotation.x);
    birth.velocity = Vector2(birth.velocity.x * rotation.x - birth.velocity.y * rotation.y,
                             birth.velocity.x * rotation.y + birth.velocity.y * rotation.x);
  }

  GpuParticle particle;
  particle.position[0] = start.x;
  particle.position[1] = start.y;
  particle.birthTime = m_ring.getTime();
  particle.lifetime = birth.lifetime;
  particle.velocity = birth.getPackedVelocity();
  particle.seed = m_random.getPseudoRandom();
  SpriteDraw::packColor(tint, particle.tintRG, particle.tintBA);
  m_ring.write(particle, birth.lifetime);
}

/*
 */
void
ParticleSystemComponent::emitOverTime(float deltaTime)
{
  const float from = m_cycleTime;
  m_cycleTime += deltaTime;

  if (m_cycleTime < m_emission.duration) {
    emitBursts(from, m_cycleTime);
  }
  else {
    emitBursts(from, m_emission.duration);
    if (!m_emission.bLoop) {
      // The part of the frame after the end makes no particles.
      m_rateDebt += m_emission.rate * (deltaTime - (m_cycleTime - m_emission.duration));
      CH_PARAMETER_UNUSED(emit(static_cast<uint32>(m_rateDebt)));
      m_bPlaying = false;
      return;
    }
    m_cycleTime = Math::fmod(m_cycleTime, m_emission.duration);
    emitBursts(0.0f, m_cycleTime);
  }

  m_rateDebt += m_emission.rate * deltaTime;
  const uint32 count = static_cast<uint32>(m_rateDebt);
  m_rateDebt -= static_cast<float>(count);
  CH_PARAMETER_UNUSED(emit(count));
}

/*
 */
void
ParticleSystemComponent::emitBursts(float from, float to)
{
  for (const ParticleBurst& burst : m_bursts) {
    for (uint32 cycle = 0; cycle < burst.cycles; ++cycle) {
      const float time = burst.time + static_cast<float>(cycle) * burst.interval;
      if (time >= from && time < to) {
        CH_PARAMETER_UNUSED(emit(burst.count));
      }
    }
  }
}

/*
 */
void
ParticleSystemComponent::resolveAssets()
{
  m_texture = findLoaded<TextureAsset>(m_look.texture);
  m_atlas = findLoaded<SpriteAtlasAsset>(m_flipbook.atlas);
  m_bLookDirty = true;
}

/*
 */
void
ParticleSystemComponent::rebuildGpuLook()
{
  m_bLookDirty = false;
  GpuParticleLook& look = m_gpuLook;
  look = {};
  look.motion = m_motion.toGpu();

  const TextureAsset* texture =
      m_atlas && m_atlas->getTexture() ? m_atlas->getTexture().get() : m_texture.get();
  m_textureAndFlags = SpriteDraw::makeTextureFlags(texture, m_look.filter);
  look.textureAndFlags = m_textureAndFlags;
  look.framesPerSecond = m_flipbook.framesPerSecond;
  look.flags = m_look.rotationMode == ParticleRotation::FaceVelocity
                   ? GpuParticleLook::FACE_VELOCITY_BIT
                   : 0u;
  look.size[0] = m_look.size.x;
  look.size[1] = m_look.size.y;
  look.minScale = m_look.minScale;
  look.maxScale = m_look.maxScale;
  look.rotation = toRadians(m_look.rotation);
  look.rotationVariance = toRadians(m_look.rotationVariance);
  look.spin = toRadians(m_look.spin);
  look.spinVariance = toRadians(m_look.spinVariance);
  copyColor(m_look.color, look.color);
  copyColor(m_look.color2, look.color2);
  look.depth = m_look.depth;

  look.colorKeyCount = static_cast<uint32>(m_colorKeys.size());
  for (uint32 i = 0; i < look.colorKeyCount; ++i) {
    look.colorTimes[i] = m_colorKeys[i].time;
    copyColor(m_colorKeys[i].color, look.colorKeys[i]);
  }
  look.sizeKeyCount = static_cast<uint32>(m_sizeKeys.size());
  for (uint32 i = 0; i < look.sizeKeyCount; ++i) {
    look.sizeTimes[i] = m_sizeKeys[i].time;
    look.sizeKeys[i] = m_sizeKeys[i].value;
  }

  look.frameCount = 0;
  if (m_atlas) {
    const Vector<SpriteAtlasFrame>& frames = m_atlas->getAtlas().getFrames();
    const uint32 first = Math::min(m_flipbook.firstFrame, static_cast<uint32>(frames.size()));
    const uint32 available = static_cast<uint32>(frames.size()) - first;
    const uint32 count = m_flipbook.frameCount > 0
                             ? Math::min(m_flipbook.frameCount, available)
                             : available;
    look.frameCount = Math::min(count, kMaxFrames);
    for (uint32 i = 0; i < look.frameCount; ++i) {
      const SpriteAtlasFrame& frame = frames[first + i];
      SpriteDraw::makeUvRect(texture, frame.position, frame.size, false, false, look.frames[i]);
    }
  }
  if (look.frameCount == 0) {
    look.frameCount = 1;
    for (uint32 i = 0; i < 4; ++i) {
      look.frames[0][i] = m_look.uvRect[i];
    }
  }

  if (m_lookBuffer) {
    m_lookBuffer->update(&look, sizeof(look));
  }
}

/*
 */
void
ParticleSystemComponent::addBatch(uint32 first, uint32 count, const EmitterPlace& place)
{
  const IBuffer* particleBuffer = m_ring.getBuffer();
  if (!particleBuffer || !m_lookBuffer || count == 0) {
    return;
  }
  CustomSpriteBatch batch{.shader = &ParticleGpu::getShader(),
                          .vertexCount = 6,
                          .instanceCount = count,
                          .sortingLayer = m_look.sortingLayer,
                          .order = m_look.order,
                          .blendMode = m_look.blendMode};
  ParticleBatchParameters parameters{.particleBufferIndex = particleBuffer->getBindlessIndex(),
                                     .firstParticle = first,
                                     .lookIndex = m_lookBuffer->getBindlessIndex(),
                                     .time = m_ring.getTime(),
                                     .emitterPosition = {0.0f, 0.0f},
                                     .emitterRotation = {1.0f, 0.0f}};
  if (m_emission.bLocalSpace) {
    parameters.emitterPosition[0] = place.position.x;
    parameters.emitterPosition[1] = place.position.y;
    parameters.emitterRotation[0] = place.rotation.x;
    parameters.emitterRotation[1] = place.rotation.y;
  }
  batch.setParameters(parameters);
  getOwner()->getScene()->getSpriteDraw().customBatch(batch);
}

} // namespace chEngineSDK
