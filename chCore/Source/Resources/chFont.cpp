/************************************************************************/
/**
 * @file chFont.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  A TrueType or OpenType font with the atlas of the glyphs it has drawn so far.
 */
/************************************************************************/
#include "chFont.h"

#include "chFileSystem.h"
#include "chIBuffer.h"
#include "chIGraphicsAPI.h"
#include "chITexture.h"
#include "chLogger.h"
#include "chMath.h"
#include "chSpriteItem.h"
#include "chUnicode.h"

// Static, so the functions never meet the copy ImGui builds into the editor.
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace chEngineSDK {

CH_LOG_DECLARE_STATIC(FontLog, All);

namespace {
constexpr WCHAR32 kLatin1End = 256;
constexpr WCHAR32 kAsciiFirst = 0x20;
constexpr WCHAR32 kAsciiEnd = 0x7F;
// Latin-1 starts again after its control characters.
constexpr WCHAR32 kLatin1First = 0xA0;
// stb writes the edge of an SDF glyph at this value.
constexpr uint8 kSdfEdgeValue = 128;
// Most fonts fit, so the table is rarely made again.
constexpr uint32 kInitialGlyphTableCapacity = 512;
constexpr StringView kDefaultFontPath = "/Engine/Fonts/Inter-Regular.ttf";

NODISCARD uint64
makeKerningKey(uint32 leftSlot, uint32 rightSlot) noexcept
{
  return (static_cast<uint64>(leftSlot) << 32) | rightSlot;
}

NODISCARD uint32
readBigEndian32(const uint8* bytes) noexcept
{
  return (static_cast<uint32>(bytes[0]) << 24) | (static_cast<uint32>(bytes[1]) << 16) |
         (static_cast<uint32>(bytes[2]) << 8) | bytes[3];
}

/**
 * stb_truetype trusts the file, so the table directory is checked first: a file cut short
 * or that is not a font would make it read past the end. The offset of the first font is
 * written to outOffset.
 */
NODISCARD bool
hasValidTableDirectory(Span<const uint8> data, int32& outOffset) noexcept
{
  constexpr SIZE_T kHeaderSize = 12;
  constexpr SIZE_T kTableRecordSize = 16;
  constexpr SIZE_T kCollectionHeaderSize = 16;
  if (data.size() < kHeaderSize) {
    return false;
  }
  SIZE_T offset = 0;
  if (data[0] == 't' && data[1] == 't' && data[2] == 'c' && data[3] == 'f') {
    if (data.size() < kCollectionHeaderSize) {
      return false;
    }
    offset = readBigEndian32(data.data() + 12);
  }
  if (offset + kHeaderSize > data.size()) {
    return false;
  }
  const SIZE_T tableCount =
      (static_cast<SIZE_T>(data[offset + 4]) << 8) | data[offset + 5];
  if (offset + kHeaderSize + tableCount * kTableRecordSize > data.size()) {
    return false;
  }
  for (SIZE_T table = 0; table < tableCount; ++table) {
    const uint8* record = data.data() + offset + kHeaderSize + table * kTableRecordSize;
    const uint64 tableEnd =
        static_cast<uint64>(readBigEndian32(record + 8)) + readBigEndian32(record + 12);
    if (tableEnd > data.size()) {
      return false;
    }
  }
  outOffset = static_cast<int32>(offset);
  return true;
}

NODISCARD SPtr<Font>&
getDefaultStorage()
{
  static SPtr<Font> s_font;
  return s_font;
}

NODISCARD bool&
getDefaultTried()
{
  static bool s_bTried = false;
  return s_bTried;
}
} // namespace

struct Font::Impl
{
  stbtt_fontinfo info{};
  float scale = 0.0f;
  bool bHasKerning = false;
};

/*
 */
Font::Font(Vector<uint8> fontData, const FontSettings& settings)
  : m_impl(chMakeUnique<Impl>()),
    m_settings(settings),
    m_fontData(std::move(fontData))
{
  m_settings.rasterSize = Math::clamp(m_settings.rasterSize,
                                      FontSettings::MIN_RASTER_SIZE,
                                      FontSettings::MAX_RASTER_SIZE);
  m_settings.sdfSpread = Math::clamp(m_settings.sdfSpread, 1u, FontSettings::MAX_SDF_SPREAD);
  m_latin1Slots.fill(INVALID_SLOT);

  int32 offset = 0;
  if (!hasValidTableDirectory(m_fontData, offset) ||
      stbtt_InitFont(&m_impl->info, m_fontData.data(), offset) == 0) {
    CH_LOG_ERROR(FontLog, "The font data could not be read as TrueType or OpenType.");
    m_impl.reset();
    return;
  }

  m_impl->scale =
      stbtt_ScaleForPixelHeight(&m_impl->info, static_cast<float>(m_settings.rasterSize));
  m_impl->bHasKerning = m_impl->info.kern != 0 || m_impl->info.gpos != 0;
  int32 ascent = 0;
  int32 descent = 0;
  int32 lineGap = 0;
  stbtt_GetFontVMetrics(&m_impl->info, &ascent, &descent, &lineGap);
  m_ascent = static_cast<float>(ascent) * m_impl->scale;
  m_descent = static_cast<float>(descent) * m_impl->scale;
  m_lineHeight = static_cast<float>(ascent - descent + lineGap) * m_impl->scale;

  m_atlasWidth = INITIAL_ATLAS_SIZE;
  m_atlasHeight = INITIAL_ATLAS_SIZE;
  m_atlasPixels.assign(static_cast<SIZE_T>(m_atlasWidth) * m_atlasHeight, 0);

  // The fallback first, so every character that is missing or does not fit has a slot.
  int32 fallbackGlyph = stbtt_FindGlyphIndex(&m_impl->info, UTF8::REPLACEMENT_CHAR);
  if (fallbackGlyph == 0) {
    fallbackGlyph = stbtt_FindGlyphIndex(&m_impl->info, '?');
  }
  m_fallbackSlot = addGlyph(fallbackGlyph);
  preload();
}

/*
 */
Font::~Font() = default;

/*
 */
bool
Font::isValid() const noexcept
{
  return m_impl != nullptr;
}

/*
 */
uint32
Font::findGlyph(WCHAR32 character)
{
  if (!m_impl) {
    return 0;
  }
  uint32* slot = nullptr;
  if (character < kLatin1End) {
    slot = &m_latin1Slots[character];
    if (*slot != INVALID_SLOT) {
      return *slot;
    }
  }
  else {
    const auto found = m_otherSlots.find(character);
    if (found != m_otherSlots.end()) {
      return found->second;
    }
  }

  const int32 glyphIndex = stbtt_FindGlyphIndex(&m_impl->info, static_cast<int32>(character));
  uint32 newSlot = m_fallbackSlot;
  if (glyphIndex != 0) {
    const auto existing = m_slotsByGlyphIndex.find(glyphIndex);
    newSlot = existing != m_slotsByGlyphIndex.end() ? existing->second : addGlyph(glyphIndex);
  }
  if (slot != nullptr) {
    *slot = newSlot;
  }
  else {
    m_otherSlots.emplace(character, newSlot);
  }
  return newSlot;
}

/*
 */
float
Font::getKerning(uint32 leftSlot, uint32 rightSlot)
{
  if (!m_impl || !m_impl->bHasKerning) {
    return 0.0f;
  }
  const uint64 key = makeKerningKey(leftSlot, rightSlot);
  const auto found = m_kerning.find(key);
  if (found != m_kerning.end()) {
    return found->second;
  }
  const float kerning = static_cast<float>(stbtt_GetGlyphKernAdvance(
                            &m_impl->info, m_glyphIndices[leftSlot], m_glyphIndices[rightSlot])) *
                        m_impl->scale;
  m_kerning.emplace(key, kerning);
  return kerning;
}

/*
 */
float
Font::getDigitAdvance()
{
  if (m_digitAdvance < 0.0f) {
    m_digitAdvance = 0.0f;
    for (WCHAR32 digit = '0'; digit <= '9'; ++digit) {
      m_digitAdvance = Math::max(m_digitAdvance, m_glyphs[findGlyph(digit)].advance);
    }
  }
  return m_digitAdvance;
}

/*
 */
uint32
Font::prepareGpuAtlas()
{
  if (!m_impl || !IGraphicsAPI::isStarted()) {
    return GpuSpriteInstance::NO_TEXTURE;
  }
  if (m_bGpuSizeChanged || !m_atlasTexture) {
    m_atlasTexture = IGraphicsAPI::instance().createTexture(
        {.format = Format::R8_UNORM,
         .width = m_atlasWidth,
         .height = m_atlasHeight,
         .usage = TextureUsage::Sampled | TextureUsage::TransferDst,
         .initialData = m_atlasPixels.data(),
         .initialDataSize = m_atlasPixels.size()});
    m_atlasBindlessIndex = m_atlasTexture->getBindlessIndex();
    m_bGpuSizeChanged = false;
    m_bGpuDirty = false;
    ++m_revision;
  }
  else if (m_bGpuDirty) {
    m_atlasTexture->uploadData(m_atlasPixels.data(), m_atlasPixels.size());
    m_bGpuDirty = false;
  }
  return m_atlasBindlessIndex;
}

/*
 */
uint32
Font::prepareGpuGlyphTable()
{
  if (!m_impl || !IGraphicsAPI::isStarted()) {
    return 0;
  }
  rebuildGpuGlyphs();
  const uint32 count = static_cast<uint32>(m_gpuGlyphs.size());
  if (m_glyphTable && count == m_glyphTableCount && m_revision == m_glyphTableRevision) {
    return m_glyphTable->getBindlessIndex();
  }

  uint32 firstToUpload = m_revision == m_glyphTableRevision ? m_glyphTableCount : 0;
  if (count > m_glyphTableCapacity || !m_glyphTable) {
    m_glyphTableCapacity =
        Math::max(count, Math::max(m_glyphTableCapacity * 2, kInitialGlyphTableCapacity));
    m_glyphTable = IGraphicsAPI::instance().createBuffer(
        {.size = static_cast<uint32>(m_glyphTableCapacity * sizeof(GpuGlyph)),
         .usage = BufferUsage::StorageBuffer,
         .memoryUsage = MemoryUsage::GpuOnly});
    firstToUpload = 0;
  }
  if (count > firstToUpload) {
    m_glyphTable->update(m_gpuGlyphs.data() + firstToUpload,
                         (count - firstToUpload) * sizeof(GpuGlyph),
                         static_cast<uint32>(firstToUpload * sizeof(GpuGlyph)));
  }
  m_glyphTableCount = count;
  m_glyphTableRevision = m_revision;
  return m_glyphTable->getBindlessIndex();
}

/*
 */
Span<const GpuGlyph>
Font::getGpuGlyphs()
{
  rebuildGpuGlyphs();
  return m_gpuGlyphs;
}

/*
 */
void
Font::releaseGpuData()
{
  m_atlasTexture.reset();
  m_glyphTable.reset();
  m_glyphTableCapacity = 0;
  m_bGpuSizeChanged = true;
}

/*
 */
const SPtr<Font>&
Font::getDefault()
{
  SPtr<Font>& font = getDefaultStorage();
  bool& bTried = getDefaultTried();
  if (!font && !bTried) {
    bTried = true;
    Vector<uint8> data = FileSystem::fastRead(Path(String(kDefaultFontPath)));
    if (data.empty()) {
      CH_LOG_ERROR(FontLog, "The default font {0} could not be read.", kDefaultFontPath);
    }
    else {
      font = chMakeShared<Font>(std::move(data), FontSettings{});
      if (!font->isValid()) {
        font.reset();
      }
    }
  }
  return font;
}

/*
 */
void
Font::setDefault(const SPtr<Font>& font)
{
  getDefaultStorage() = font;
  getDefaultTried() = font != nullptr;
}

/*
 */
void
Font::releaseDefault()
{
  getDefaultStorage().reset();
  getDefaultTried() = false;
}

/*
 */
uint32
Font::addGlyph(int32 glyphIndex)
{
  const float scale = m_impl->scale;
  int32 advance = 0;
  int32 leftBearing = 0;
  stbtt_GetGlyphHMetrics(&m_impl->info, glyphIndex, &advance, &leftBearing);

  FontGlyph glyph{.advance = static_cast<float>(advance) * scale};
  int32 width = 0;
  int32 height = 0;
  int32 offsetX = 0;
  int32 offsetY = 0;
  uint8* sdf = nullptr;
  if (m_settings.mode == FontRenderMode::Sdf) {
    const int32 spread = static_cast<int32>(m_settings.sdfSpread);
    sdf = stbtt_GetGlyphSDF(&m_impl->info,
                            scale,
                            glyphIndex,
                            spread,
                            kSdfEdgeValue,
                            static_cast<float>(kSdfEdgeValue) / static_cast<float>(spread),
                            &width,
                            &height,
                            &offsetX,
                            &offsetY);
  }
  else {
    int32 x1 = 0;
    int32 y1 = 0;
    stbtt_GetGlyphBitmapBox(&m_impl->info, glyphIndex, scale, scale, &offsetX, &offsetY, &x1, &y1);
    width = x1 - offsetX;
    height = y1 - offsetY;
  }

  uint32 atlasX = 0;
  uint32 atlasY = 0;
  const bool bHasImage = width > 0 && height > 0;
  if (bHasImage && !placeGlyph(static_cast<uint32>(width), static_cast<uint32>(height), atlasX, atlasY)) {
    if (sdf != nullptr) {
      stbtt_FreeSDF(sdf, nullptr);
    }
    // The fallback itself always gets a slot, even without its image.
    if (m_fallbackSlot != INVALID_SLOT) {
      return m_fallbackSlot;
    }
    width = 0;
    height = 0;
  }

  if (bHasImage && width > 0) {
    uint8* target = m_atlasPixels.data() + static_cast<SIZE_T>(atlasY) * m_atlasWidth + atlasX;
    if (sdf != nullptr) {
      for (int32 row = 0; row < height; ++row) {
        const uint8* source = sdf + static_cast<SIZE_T>(row) * width;
        for (int32 column = 0; column < width; ++column) {
          target[static_cast<SIZE_T>(row) * m_atlasWidth + column] = source[column];
        }
      }
    }
    else {
      stbtt_MakeGlyphBitmap(&m_impl->info,
                            target,
                            width,
                            height,
                            static_cast<int32>(m_atlasWidth),
                            scale,
                            scale,
                            glyphIndex);
    }
    glyph.atlasX = static_cast<uint16>(atlasX);
    glyph.atlasY = static_cast<uint16>(atlasY);
    glyph.width = static_cast<uint16>(width);
    glyph.height = static_cast<uint16>(height);
    glyph.offsetX = static_cast<float>(offsetX);
    glyph.offsetY = -static_cast<float>(offsetY);
    m_bGpuDirty = true;
  }
  if (sdf != nullptr) {
    stbtt_FreeSDF(sdf, nullptr);
  }

  const uint32 slot = static_cast<uint32>(m_glyphs.size());
  m_glyphs.push_back(glyph);
  m_glyphIndices.push_back(glyphIndex);
  m_slotsByGlyphIndex.emplace(glyphIndex, slot);
  return slot;
}

/*
 */
bool
Font::placeGlyph(uint32 width, uint32 height, uint32& outX, uint32& outY)
{
  const uint32 paddedWidth = width + ATLAS_PADDING;
  const uint32 paddedHeight = height + ATLAS_PADDING;
  if (paddedWidth > MAX_ATLAS_SIZE || paddedHeight > MAX_ATLAS_SIZE) {
    return false;
  }
  for (;;) {
    if (m_shelfX + paddedWidth > m_atlasWidth) {
      m_shelfY += m_shelfHeight;
      m_shelfX = 0;
      m_shelfHeight = 0;
    }
    if (m_shelfX + paddedWidth <= m_atlasWidth &&
        m_shelfY + Math::max(m_shelfHeight, paddedHeight) <= m_atlasHeight) {
      break;
    }
    if (!growAtlas()) {
      return false;
    }
  }
  outX = m_shelfX;
  outY = m_shelfY;
  m_shelfX += paddedWidth;
  m_shelfHeight = Math::max(m_shelfHeight, paddedHeight);
  return true;
}

/*
 */
bool
Font::growAtlas()
{
  // Taller first: rows keep their place, so the pixels already drawn are only copied.
  const uint32 newWidth = m_atlasHeight > m_atlasWidth ? m_atlasWidth * 2 : m_atlasWidth;
  const uint32 newHeight = m_atlasHeight > m_atlasWidth ? m_atlasHeight : m_atlasHeight * 2;
  if (newWidth > MAX_ATLAS_SIZE || newHeight > MAX_ATLAS_SIZE) {
    if (!m_bAtlasFull) {
      m_bAtlasFull = true;
      CH_LOG_WARNING(FontLog,
                     "The font atlas is full at {0}x{0}; new characters show the fallback.",
                     MAX_ATLAS_SIZE);
    }
    return false;
  }

  Vector<uint8> pixels(static_cast<SIZE_T>(newWidth) * newHeight, 0);
  for (uint32 row = 0; row < m_atlasHeight; ++row) {
    const uint8* source = m_atlasPixels.data() + static_cast<SIZE_T>(row) * m_atlasWidth;
    uint8* target = pixels.data() + static_cast<SIZE_T>(row) * newWidth;
    for (uint32 column = 0; column < m_atlasWidth; ++column) {
      target[column] = source[column];
    }
  }
  m_atlasPixels = std::move(pixels);
  m_atlasWidth = newWidth;
  m_atlasHeight = newHeight;
  m_bGpuSizeChanged = true;
  ++m_revision;
  return true;
}

/*
 */
void
Font::rebuildGpuGlyphs()
{
  // Texture coordinates depend on the atlas size, so a new revision computes them all.
  SIZE_T first = m_gpuGlyphs.size();
  if (m_gpuGlyphsRevision != m_revision) {
    m_gpuGlyphs.clear();
    first = 0;
  }
  if (first == m_glyphs.size()) {
    m_gpuGlyphsRevision = m_revision;
    return;
  }
  const float inverseWidth = 1.0f / static_cast<float>(m_atlasWidth);
  const float inverseHeight = 1.0f / static_cast<float>(m_atlasHeight);
  const float height = getHeight();
  const float inverseFontHeight = height > 0.0f ? 1.0f / height : 0.0f;
  m_gpuGlyphs.resize(m_glyphs.size());
  for (SIZE_T slot = first; slot < m_glyphs.size(); ++slot) {
    const FontGlyph& glyph = m_glyphs[slot];
    GpuGlyph& gpu = m_gpuGlyphs[slot];
    const float width = static_cast<float>(glyph.width);
    const float glyphHeight = static_cast<float>(glyph.height);
    gpu.uvRect[0] = static_cast<float>(glyph.atlasX) * inverseWidth;
    gpu.uvRect[1] = static_cast<float>(glyph.atlasY) * inverseHeight;
    gpu.uvRect[2] = width * inverseWidth;
    gpu.uvRect[3] = glyphHeight * inverseHeight;
    gpu.box[0] = glyph.offsetX * inverseFontHeight;
    gpu.box[1] = (glyph.offsetY - glyphHeight) * inverseFontHeight;
    gpu.box[2] = width * inverseFontHeight;
    gpu.box[3] = glyphHeight * inverseFontHeight;
  }
  m_gpuGlyphsRevision = m_revision;
}

/*
 */
void
Font::preload()
{
  if (m_settings.bPreloadAscii) {
    for (WCHAR32 character = kAsciiFirst; character < kAsciiEnd; ++character) {
      CH_PARAMETER_UNUSED(findGlyph(character));
    }
  }
  if (m_settings.bPreloadLatin1) {
    for (WCHAR32 character = kLatin1First; character < kLatin1End; ++character) {
      CH_PARAMETER_UNUSED(findGlyph(character));
    }
  }
  const StringView text = m_settings.preloadCharacters;
  SIZE_T position = 0;
  while (position < text.size()) {
    CH_PARAMETER_UNUSED(findGlyph(UTF8::decodeNext(text, position)));
  }
}

} // namespace chEngineSDK
