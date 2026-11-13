/************************************************************************/
/**
 * @file chTextLayout.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Places the glyphs of a text: lines, wrapping, alignment, kerning and color tags.
 */
/************************************************************************/
#include "chTextLayout.h"

#include "chFont.h"
#include "chMath.h"
#include "chSpriteDraw.h"
#include "chUnicode.h"

namespace chEngineSDK {

namespace {
constexpr StringView kColorTag = "color=#";
constexpr StringView kColorEndTag = "/color>";
constexpr float kTabSpaces = 4.0f;

NODISCARD int32
hexValue(ANSICHAR character) noexcept
{
  if (character >= '0' && character <= '9') {
    return character - '0';
  }
  if (character >= 'a' && character <= 'f') {
    return character - 'a' + 10;
  }
  if (character >= 'A' && character <= 'F') {
    return character - 'A' + 10;
  }
  return -1;
}

NODISCARD float
srgbToLinear(float value) noexcept
{
  return value <= 0.04045f ? value / 12.92f : Math::pow((value + 0.055f) / 1.055f, 2.4f);
}

/**
 * Reads #rrggbb or #rrggbbaa followed by '>' at position. On success position ends after
 * the '>'.
 */
NODISCARD bool
readHexColor(StringView text, SIZE_T& position, LinearColor& outColor) noexcept
{
  float channels[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  uint32 count = 0;
  SIZE_T cursor = position;
  while (count < 4 && cursor + 1 < text.size()) {
    const int32 high = hexValue(text[cursor]);
    const int32 low = hexValue(text[cursor + 1]);
    if (high < 0 || low < 0) {
      break;
    }
    channels[count++] = static_cast<float>(high * 16 + low) / 255.0f;
    cursor += 2;
  }
  if ((count != 3 && count != 4) || cursor >= text.size() || text[cursor] != '>') {
    return false;
  }
  outColor = LinearColor(srgbToLinear(channels[0]),
                         srgbToLinear(channels[1]),
                         srgbToLinear(channels[2]),
                         channels[3]);
  position = cursor + 1;
  return true;
}

NODISCARD bool
startsWithAt(StringView text, SIZE_T position, StringView prefix) noexcept
{
  return text.size() - position >= prefix.size() &&
         text.substr(position, prefix.size()) == prefix;
}
} // namespace

/*
 */
void
TextLayout::build(Font& font, StringView text, const TextLayoutDesc& desc)
{
  clear();
  if (!font.isValid()) {
    return;
  }

  const float fontHeight = font.getAscent() - font.getDescent();
  const float scale = fontHeight > 0.0f ? desc.size / fontHeight : 0.0f;
  const float lineHeight = font.getLineHeight() * scale * desc.lineSpacing;
  const float digitAdvance = desc.bMonospaceDigits ? font.getDigitAdvance() * scale : 0.0f;

  Array<LinearColor, MAX_COLOR_DEPTH> colors;
  colors[0] = desc.color;
  uint32 colorDepth = 0;
  uint32 colorRG = 0;
  uint32 colorBA = 0;
  SpriteDraw::packColor(desc.color, colorRG, colorBA);

  float pen = 0.0f;
  float contentWidth = 0.0f;
  bool bHasBreak = false;
  uint32 breakGlyph = 0;
  float breakPen = 0.0f;
  float breakWidth = 0.0f;
  uint32 previousSlot = Font::INVALID_SLOT;

  SIZE_T position = 0;
  while (position < text.size()) {
    const WCHAR32 character = UTF8::decodeNext(text, position);

    if (character == '<' && desc.bRichText) {
      if (position < text.size() && text[position] == '<') {
        ++position;
      }
      else if (startsWithAt(text, position, kColorTag)) {
        SIZE_T cursor = position + kColorTag.size();
        LinearColor tagColor;
        if (readHexColor(text, cursor, tagColor)) {
          position = cursor;
          // Deeper tags than the stack holds replace the last color, and still pop.
          ++colorDepth;
          LinearColor& color = colors[Math::min(colorDepth, MAX_COLOR_DEPTH - 1)];
          color = tagColor;
          color.a *= desc.color.a;
          SpriteDraw::packColor(color, colorRG, colorBA);
          continue;
        }
      }
      else if (startsWithAt(text, position, kColorEndTag) && colorDepth > 0) {
        position += kColorEndTag.size();
        --colorDepth;
        SpriteDraw::packColor(colors[Math::min(colorDepth, MAX_COLOR_DEPTH - 1)],
                              colorRG,
                              colorBA);
        continue;
      }
    }
    if (character == '\r') {
      continue;
    }
    if (character == '\n') {
      finishLine(static_cast<uint32>(m_glyphs.size()), contentWidth);
      pen = 0.0f;
      contentWidth = 0.0f;
      bHasBreak = false;
      previousSlot = Font::INVALID_SLOT;
      continue;
    }

    const bool bTab = character == '\t';
    const uint32 slot = font.findGlyph(bTab ? ' ' : character);
    const FontGlyph& glyph = font.getGlyph(slot);
    const bool bFixedDigit = desc.bMonospaceDigits && character >= '0' && character <= '9';
    float advance = bFixedDigit ? digitAdvance : glyph.advance * scale;
    if (bTab) {
      advance *= kTabSpaces;
    }
    if (!bFixedDigit && previousSlot != Font::INVALID_SLOT) {
      pen += font.getKerning(previousSlot, slot) * scale;
    }
    previousSlot = slot;

    if (character == ' ' || bTab) {
      pen += advance + desc.letterSpacing;
      bHasBreak = true;
      breakGlyph = static_cast<uint32>(m_glyphs.size());
      breakPen = pen;
      breakWidth = contentWidth;
      continue;
    }

    if (desc.wrapWidth > 0.0f && pen + advance > desc.wrapWidth && contentWidth > 0.0f) {
      if (bHasBreak) {
        // The word that started after the last space goes to the next line whole.
        finishLine(breakGlyph, breakWidth);
        for (SIZE_T i = breakGlyph; i < m_glyphs.size(); ++i) {
          m_glyphs[i].position.x -= breakPen;
        }
        pen -= breakPen;
        contentWidth -= breakPen;
      }
      else {
        finishLine(static_cast<uint32>(m_glyphs.size()), contentWidth);
        pen = 0.0f;
        contentWidth = 0.0f;
      }
      bHasBreak = false;
    }

    if (glyph.width > 0 && glyph.height > 0) {
      const float centering = bFixedDigit ? (digitAdvance - glyph.advance * scale) * 0.5f : 0.0f;
      const float height = static_cast<float>(glyph.height) * scale;
      m_glyphs.push_back({.slot = slot,
                          .position = Vector2(pen + centering + glyph.offsetX * scale,
                                              glyph.offsetY * scale - height),
                          .size = Vector2(static_cast<float>(glyph.width) * scale, height),
                          .colorRG = colorRG,
                          .colorBA = colorBA});
    }
    pen += advance;
    contentWidth = pen;
    pen += desc.letterSpacing;
  }
  finishLine(static_cast<uint32>(m_glyphs.size()), contentWidth);

  // Lines were placed on their own baselines from x = 0; now they go into the block.
  float blockWidth = desc.wrapWidth;
  if (blockWidth <= 0.0f) {
    for (const Line& line : m_lines) {
      blockWidth = Math::max(blockWidth, line.width);
    }
  }
  const float ascent = font.getAscent() * scale;
  const float height =
      static_cast<float>(m_lines.size() - 1) * lineHeight + fontHeight * scale;
  const Vector2 pivot(desc.pivot.x * blockWidth, (desc.pivot.y - 1.0f) * height);
  const float alignFactor = desc.align == TextAlign::Center  ? 0.5f
                            : desc.align == TextAlign::Right ? 1.0f
                                                             : 0.0f;
  float radiusSquared = 0.0f;
  for (SIZE_T lineIndex = 0; lineIndex < m_lines.size(); ++lineIndex) {
    const Line& line = m_lines[lineIndex];
    const Vector2 offset((blockWidth - line.width) * alignFactor - pivot.x,
                         -ascent - static_cast<float>(lineIndex) * lineHeight - pivot.y);
    for (uint32 i = 0; i < line.glyphCount; ++i) {
      TextGlyph& glyph = m_glyphs[line.firstGlyph + i];
      glyph.position += offset;
      const float x = Math::max(Math::abs(glyph.position.x),
                                Math::abs(glyph.position.x + glyph.size.x));
      const float y = Math::max(Math::abs(glyph.position.y),
                                Math::abs(glyph.position.y + glyph.size.y));
      radiusSquared = Math::max(radiusSquared, x * x + y * y);
    }
  }
  m_size = Vector2(blockWidth, height);
  m_radius = Math::sqrt(radiusSquared);
}

/*
 */
void
TextLayout::clear() noexcept
{
  m_glyphs.clear();
  m_lines.clear();
  m_size = Vector2::ZERO;
  m_radius = 0.0f;
}

/*
 */
void
TextLayout::finishLine(uint32 glyphEnd, float width)
{
  const uint32 firstGlyph =
      m_lines.empty() ? 0 : m_lines.back().firstGlyph + m_lines.back().glyphCount;
  m_lines.push_back({.firstGlyph = firstGlyph, .glyphCount = glyphEnd - firstGlyph, .width = width});
}

} // namespace chEngineSDK
