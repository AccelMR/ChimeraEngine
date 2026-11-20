/************************************************************************/
/**
 * @file chUITextBlock.cpp
 * @author Swampertor
 * @date 2026/11/19
 * @brief Widget that draws a laid-out text, as UMG's UTextBlock.
 */
/************************************************************************/
#include "chUITextBlock.h"

#include "chFont.h"
#include "chMath.h"
#include "chUIDrawList.h"

namespace chEngineSDK {

UITextBlock::UITextBlock()
{
  // A label is not a control: navigation lands on the widgets that act. The default
  // visibility also lets a label inside a button pass hover and clicks through to it.
  setFocusable(false);
  setVisibility(UIVisibility::SelfHitTestInvisible);
}

UITextBlock::~UITextBlock() = default;

/*
 */
void
UITextBlock::setText(StringView text)
{
  m_text = text;
  invalidateLayout();
}

/*
 */
Font*
UITextBlock::getFont() const
{
  if (m_font != nullptr) {
    return m_font;
  }
  const SPtr<Font>& defaultFont = Font::getDefault();
  return defaultFont.get();
}

/*
 */
void
UITextBlock::setFont(Font* font) noexcept
{
  m_font = font;
  invalidateLayout();
}

/*
 */
void
UITextBlock::setFontSize(float size) noexcept
{
  m_fontSize = size;
  invalidateLayout();
}

/*
 */
void
UITextBlock::setAlign(TextAlign align) noexcept
{
  m_align = align;
  invalidateLayout();
}

/*
 */
void
UITextBlock::setWrapText(bool wrap) noexcept
{
  m_bWrapText = wrap;
  invalidateLayout();
}

/*
 */
void
UITextBlock::setMargin(const UIMargin& margin) noexcept
{
  m_margin = margin;
  invalidateLayout();
}

/*
 */
void
UITextBlock::setLineSpacing(float spacing) noexcept
{
  m_lineSpacing = spacing;
  invalidateLayout();
}

/*
 */
void
UITextBlock::setLetterSpacing(float spacing) noexcept
{
  m_letterSpacing = spacing;
  invalidateLayout();
}

/*
 */
void
UITextBlock::setMonospaceDigits(bool monospace) noexcept
{
  m_bMonospaceDigits = monospace;
  invalidateLayout();
}

/*
 */
void
UITextBlock::setRichText(bool richText) noexcept
{
  m_bRichText = richText;
  invalidateLayout();
}

/*
 */
void
UITextBlock::setTextColor(const LinearColor& color)
{
  setColor(color);
  invalidateLayout();
}

/*
 */
Vector2
UITextBlock::getDesiredSize() const
{
  ensureLayout();
  return m_layout.getSize() + Vector2(m_margin.getHorizontal(), m_margin.getVertical());
}

/*
 */
void
UITextBlock::arrange(const UIGeometry& geometry)
{
  // Wrapping depends on the width, so a new one may change the layout.
  if (geometry.size.x != getSize().x) {
    invalidateLayout();
  }
  UIWidget::arrange(geometry);
}

/*
 */
void
UITextBlock::ensureLayout() const
{
  Font* font = getFont();
  if (font == nullptr || !font->isValid()) {
    m_layout.clear();
    m_bLayoutDirty = false;
    return;
  }

  const float wrapWidth =
      m_bWrapText ? Math::max(0.0f, getSize().x - m_margin.getHorizontal()) : 0.0f;
  if (!m_bLayoutDirty && wrapWidth == m_layoutWrapWidth) {
    return;
  }

  TextLayoutDesc desc;
  desc.size = m_fontSize;
  desc.wrapWidth = wrapWidth;
  desc.align = m_align;
  desc.lineSpacing = m_lineSpacing;
  desc.letterSpacing = m_letterSpacing;
  // Top-left of the block, so the glyphs extend right and down from where paint puts it.
  desc.pivot = {0.0f, 1.0f};
  desc.bMonospaceDigits = m_bMonospaceDigits;
  desc.bRichText = m_bRichText;
  desc.color = getColor();
  m_layout.build(*font, m_text, desc);

  m_bLayoutDirty = false;
  m_layoutWrapWidth = wrapWidth;
}

/*
 */
void
UITextBlock::paint(UIDrawList& drawList) const
{
  Font* font = getFont();
  if (font == nullptr || !font->isValid()) {
    return;
  }

  ensureLayout();
  if (m_layout.getGlyphs().empty()) {
    return;
  }

  const Vector2 position = getPosition() + Vector2(m_margin.left, m_margin.top);
  drawList.addText(*font, m_layout, position);
}

} // namespace chEngineSDK
