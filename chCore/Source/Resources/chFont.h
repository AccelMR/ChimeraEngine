/************************************************************************/
/**
 * @file chFont.h
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  A TrueType or OpenType font with the atlas of the glyphs it has drawn so far.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

namespace chEngineSDK {
class IBuffer;
class ITexture;

/**
 * How a font's glyphs are drawn into its atlas. Saved in font assets, so the values are
 * fixed.
 */
enum class FontRenderMode : uint8
{
  // Signed distance fields: sharp at any size, with outlines and shadows.
  Sdf = 0,
  // Plain coverage at one size, read with the point filter, for pixel-art fonts.
  Pixel = 1,
  COUNT
};

struct FontSettings
{
  FontRenderMode mode = FontRenderMode::Sdf;
  // Height in pixels the glyphs are drawn at: a pixel font's own size, the detail of an SDF
  // font.
  uint32 rasterSize = 48;
  // SDF only: pixels of distance kept around each glyph, so the widest outline or shadow
  // edge it can draw.
  uint32 sdfSpread = 8;
  // Drawn when the font is made, so the first frame that shows them does not have to.
  bool bPreloadAscii = true;
  bool bPreloadLatin1 = true;
  // More characters to draw when the font is made, as UTF-8.
  String preloadCharacters{};

  // Kept in range by Font, so a broken asset cannot ask for a huge atlas.
  static constexpr uint32 MIN_RASTER_SIZE = 4;
  static constexpr uint32 MAX_RASTER_SIZE = 256;
  static constexpr uint32 MAX_SDF_SPREAD = 32;
};

/**
 * One glyph of the atlas. Sizes are pixels at FontSettings::rasterSize; y goes up.
 */
struct FontGlyph
{
  // Rectangle of the glyph's image in the atlas, from its top left corner.
  uint16 atlasX = 0;
  uint16 atlasY = 0;
  uint16 width = 0;
  uint16 height = 0;
  // From the pen on the baseline to the left and top edges of the image.
  float offsetX = 0.0f;
  float offsetY = 0.0f;
  // How far the pen moves after the glyph.
  float advance = 0.0f;
};

/**
 * One glyph as glyphrun.hlsl reads it from the font's glyph table, 32 bytes. Sizes are in
 * font heights (ascent to descent), y up from the baseline.
 */
struct GpuGlyph
{
  // Left, top, width and height of the image in the atlas, in 0..1.
  float uvRect[4];
  // Left and bottom edges of the image from the pen, then its width and height.
  float box[4];
};
static_assert(sizeof(GpuGlyph) == 32, "GpuGlyph must match glyphrun.hlsl");

/**
 * Exists so text can show any character without preparing the atlas by hand: glyphs are
 * drawn into a one-channel atlas the first time they are asked for, and stay there. The
 * atlas grows (up to MAX_ATLAS_SIZE on each side) when it is full, keeping where every
 * glyph is, so only their texture coordinates change; getRevision() tells callers that
 * cached them. The GPU copy is made and updated by prepareGpuAtlas(), on the main thread.
 *
 * Drawing a glyph costs a little (an SDF glyph about the time of a small image), so text
 * shown every frame only pays it once per character. Asking for a glyph that is already
 * there is one table read for Latin-1 and one hash lookup above it.
 */
class CH_CORE_EXPORT Font
{
 public:
  /**
   * fontData holds the whole TrueType or OpenType file. isValid() is false when it cannot
   * be read.
   */
  Font(Vector<uint8> fontData, const FontSettings& settings);

  ~Font();

  Font(const Font&) = delete;

  Font&
  operator=(const Font&) = delete;

  NODISCARD bool
  isValid() const noexcept;

  NODISCARD FORCEINLINE const FontSettings&
  getSettings() const noexcept
  {
    return m_settings;
  }

  NODISCARD FORCEINLINE Span<const uint8>
  getFontData() const noexcept
  {
    return m_fontData;
  }

  /**
   * Height above the baseline the font reaches, in pixels at the raster size.
   */
  NODISCARD FORCEINLINE float
  getAscent() const noexcept
  {
    return m_ascent;
  }

  /**
   * Negative: how far below the baseline the font reaches.
   */
  NODISCARD FORCEINLINE float
  getDescent() const noexcept
  {
    return m_descent;
  }

  /**
   * From the lowest to the highest point, the unit glyph runs and TextLayout sizes use.
   */
  NODISCARD FORCEINLINE float
  getHeight() const noexcept
  {
    return m_ascent - m_descent;
  }

  /**
   * From one baseline to the next.
   */
  NODISCARD FORCEINLINE float
  getLineHeight() const noexcept
  {
    return m_lineHeight;
  }

  /**
   * The slot of a character, drawing it into the atlas the first time. A character the font
   * does not have, or one that no longer fits, gives the slot of U+FFFD (or '?').
   */
  NODISCARD uint32
  findGlyph(WCHAR32 character);

  NODISCARD FORCEINLINE const FontGlyph&
  getGlyph(uint32 slot) const noexcept
  {
    return m_glyphs[slot];
  }

  NODISCARD FORCEINLINE uint32
  getGlyphCount() const noexcept
  {
    return static_cast<uint32>(m_glyphs.size());
  }

  /**
   * Added to the pen between two glyphs, in pixels at the raster size.
   */
  NODISCARD float
  getKerning(uint32 leftSlot, uint32 rightSlot);

  /**
   * The widest advance of the digits 0 to 9, for numbers whose digits keep their place.
   */
  NODISCARD float
  getDigitAdvance();

  NODISCARD FORCEINLINE uint32
  getAtlasWidth() const noexcept
  {
    return m_atlasWidth;
  }

  NODISCARD FORCEINLINE uint32
  getAtlasHeight() const noexcept
  {
    return m_atlasHeight;
  }

  /**
   * One byte per pixel, row by row.
   */
  NODISCARD FORCEINLINE Span<const uint8>
  getAtlasPixels() const noexcept
  {
    return m_atlasPixels;
  }

  /**
   * Goes up when the atlas changes size, or gets a new GPU texture, which changes the
   * texture coordinates or the bindless index of glyphs drawn before.
   */
  NODISCARD FORCEINLINE uint32
  getRevision() const noexcept
  {
    return m_revision;
  }

  /**
   * Sends the glyphs drawn since the last call to the GPU and gives the bindless index of
   * the atlas, or GpuSpriteInstance::NO_TEXTURE without a graphics API. Main thread only;
   * a frame that drew new glyphs uploads the whole atlas once.
   */
  NODISCARD uint32
  prepareGpuAtlas();

  NODISCARD FORCEINLINE const SPtr<ITexture>&
  getAtlasTexture() const noexcept
  {
    return m_atlasTexture;
  }

  /**
   * Sends the glyph table (one GpuGlyph per slot) to the GPU when glyphs were added or the
   * atlas changed size, and gives the bindless index of its storage buffer, 0 without a
   * graphics API. Main thread only. A table made bigger is a new buffer, so runs built
   * earlier in the frame keep reading the old one, which has every glyph they use.
   */
  NODISCARD uint32
  prepareGpuGlyphTable();

  /**
   * The table prepareGpuGlyphTable uploads, in slot order.
   */
  NODISCARD Span<const GpuGlyph>
  getGpuGlyphs();

  /**
   * Drops the GPU copy; the next prepareGpuAtlas() makes it again.
   */
  void
  releaseGpuData();

  /**
   * The font texts use when they have none: Inter, read from /Engine/Fonts on first use.
   * Null when the file cannot be read.
   */
  NODISCARD static const SPtr<Font>&
  getDefault();

  /**
   * Called before the graphics API shuts down, since the default font keeps a texture.
   */
  static void
  releaseDefault();

  static constexpr uint32 INITIAL_ATLAS_SIZE = 512;
  static constexpr uint32 MAX_ATLAS_SIZE = 4096;
  // Empty pixels between glyphs, so filtering never reads a neighbor.
  static constexpr uint32 ATLAS_PADDING = 1;
  static constexpr uint32 INVALID_SLOT = 0xFFFFFFFFu;

 private:
  struct Impl;

  NODISCARD uint32
  addGlyph(int32 glyphIndex);

  NODISCARD bool
  placeGlyph(uint32 width, uint32 height, uint32& outX, uint32& outY);

  NODISCARD bool
  growAtlas();

  void
  preload();

  void
  rebuildGpuGlyphs();

  UniquePtr<Impl> m_impl;
  FontSettings m_settings;
  Vector<uint8> m_fontData;
  float m_ascent = 0.0f;
  float m_descent = 0.0f;
  float m_lineHeight = 0.0f;

  Vector<FontGlyph> m_glyphs;
  // Font glyph index of each slot, for kerning.
  Vector<int32> m_glyphIndices;
  Array<uint32, 256> m_latin1Slots;
  UnorderedMap<WCHAR32, uint32> m_otherSlots;
  UnorderedMap<int32, uint32> m_slotsByGlyphIndex;
  UnorderedMap<uint64, float> m_kerning;
  uint32 m_fallbackSlot = INVALID_SLOT;
  float m_digitAdvance = -1.0f;

  Vector<uint8> m_atlasPixels;
  uint32 m_atlasWidth = 0;
  uint32 m_atlasHeight = 0;
  uint32 m_shelfX = 0;
  uint32 m_shelfY = 0;
  uint32 m_shelfHeight = 0;
  bool m_bAtlasFull = false;
  uint32 m_revision = 0;

  Vector<GpuGlyph> m_gpuGlyphs;
  uint32 m_gpuGlyphsRevision = INVALID_SLOT;
  SPtr<IBuffer> m_glyphTable;
  uint32 m_glyphTableCapacity = 0;
  uint32 m_glyphTableCount = 0;
  uint32 m_glyphTableRevision = INVALID_SLOT;

  SPtr<ITexture> m_atlasTexture;
  uint32 m_atlasBindlessIndex = 0;
  bool m_bGpuDirty = true;
  bool m_bGpuSizeChanged = true;
};

} // namespace chEngineSDK
