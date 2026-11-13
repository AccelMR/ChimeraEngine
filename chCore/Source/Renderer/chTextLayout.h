/************************************************************************/
/**
 * @file chTextLayout.h
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Places the glyphs of a text: lines, wrapping, alignment, kerning and color tags.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chLinearColor.h"
#include "chVector2.h"

namespace chEngineSDK {
class Font;

/**
 * Where the lines of a text stand inside its block. Saved by text components, so the
 * values are fixed.
 */
enum class TextAlign : uint8
{
  Left = 0,
  Center = 1,
  Right = 2,
  COUNT
};

struct TextLayoutDesc
{
  // Height from the lowest to the highest point of the font, in world units.
  float size = 1.0f;
  // Lines longer than this break at their last space (or inside a word that is longer by
  // itself); 0 never breaks.
  float wrapWidth = 0.0f;
  TextAlign align = TextAlign::Left;
  // Times the font's own distance between lines.
  float lineSpacing = 1.0f;
  // Added after every glyph, in world units.
  float letterSpacing = 0.0f;
  // Point of the block that stands at the origin: (0, 0) bottom left, (1, 1) top right.
  Vector2 pivot{0.5f, 0.5f};
  // Every digit takes the width of the widest, so numbers that change do not move.
  bool bMonospaceDigits = false;
  // Reads <color=#rrggbb> or <color=#rrggbbaa> ... </color>, which can nest; << writes a
  // '<'. Anything else that starts with '<' is plain text.
  bool bRichText = true;
  LinearColor color = LinearColor::WHITE;
};

/**
 * A placed glyph, in world units from the pivot, x right and y up.
 */
struct TextGlyph
{
  uint32 slot = 0;
  // Bottom left corner of the glyph's image.
  Vector2 position = Vector2::ZERO;
  Vector2 size = Vector2::ZERO;
  // Halves, as GpuSpriteInstance keeps them.
  uint32 colorRG = 0;
  uint32 colorBA = 0;
};

/**
 * Exists so text is laid out once and drawn many times: build() places every glyph that has
 * an image (spaces only move the pen), and whoever draws it (sprites now, game UI later)
 * only moves the glyphs. The vectors keep their memory, so building texts of the same size
 * again does not allocate; new characters are drawn into the font's atlas on the way.
 * Colors in tags are sRGB, as image editors write them, and turned into linear values.
 */
class CH_CORE_EXPORT TextLayout
{
 public:
  void
  build(Font& font, StringView text, const TextLayoutDesc& desc);

  void
  clear() noexcept;

  NODISCARD FORCEINLINE Span<const TextGlyph>
  getGlyphs() const noexcept
  {
    return m_glyphs;
  }

  /**
   * Width and height of the block in world units: the widest line (or the wrap width) and
   * every line from the top of the first to the bottom of the last.
   */
  NODISCARD FORCEINLINE const Vector2&
  getSize() const noexcept
  {
    return m_size;
  }

  NODISCARD FORCEINLINE uint32
  getLineCount() const noexcept
  {
    return static_cast<uint32>(m_lines.size());
  }

  /**
   * Distance from the pivot that holds every glyph, for culling.
   */
  NODISCARD FORCEINLINE float
  getRadius() const noexcept
  {
    return m_radius;
  }

  static constexpr uint32 MAX_COLOR_DEPTH = 8;

 private:
  struct Line
  {
    uint32 firstGlyph = 0;
    uint32 glyphCount = 0;
    float width = 0.0f;
  };

  void
  finishLine(uint32 glyphEnd, float width);

  Vector<TextGlyph> m_glyphs;
  Vector<Line> m_lines;
  Vector2 m_size = Vector2::ZERO;
  float m_radius = 0.0f;
};

} // namespace chEngineSDK
