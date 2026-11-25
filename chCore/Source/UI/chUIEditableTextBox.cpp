/************************************************************************/
/**
 * @file chUIEditableTextBox.cpp
 * @author Swampertor
 * @date 2026/11/24
 * @brief Single-line text field, as UMG's UEditableTextBox.
 */
/************************************************************************/
#include "chUIEditableTextBox.h"

#include "chFont.h"
#include "chMath.h"
#include "chUIDrawList.h"
#include "chUnicode.h"

namespace chEngineSDK {

namespace {

constexpr float kCaretBlinkSeconds = 0.5f;
constexpr float kCaretWidth = 1.5f;
constexpr float kMinWidth = 80.0f;
constexpr float kMinHeight = 24.0f;

NODISCARD SIZE_T
previousCodepointStart(StringView text, SIZE_T index) noexcept
{
  if (index == 0) {
    return 0;
  }

  SIZE_T position = index - 1;
  while (position > 0 && (static_cast<uint8>(text[position]) & 0xC0) == 0x80) {
    --position;
  }
  return position;
}

NODISCARD SIZE_T
nextCodepointEnd(StringView text, SIZE_T index) noexcept
{
  SIZE_T position = index + 1;
  while (position < text.size() && (static_cast<uint8>(text[position]) & 0xC0) == 0x80) {
    ++position;
  }
  return position;
}

NODISCARD SIZE_T
countCodepoints(StringView text) noexcept
{
  SIZE_T count = 0;
  SIZE_T position = 0;
  while (position < text.size()) {
    (void)UTF8::decodeNext(text, position);
    ++count;
  }
  return count;
}

NODISCARD SIZE_T
truncateToCodepoints(StringView text, SIZE_T maximum) noexcept
{
  SIZE_T count = 0;
  SIZE_T position = 0;
  while (position < text.size() && count < maximum) {
    (void)UTF8::decodeNext(text, position);
    ++count;
  }
  return position;
}

/**
 * Width in pixels of the first end bytes of text, matching TextLayout's scale, advances
 * and kerning with no wrapping. Password fields measure a '*' per character.
 */
float
measureWidth(Font& font, StringView text, SIZE_T end, bool bPassword, float fontSize)
{
  if (!font.isValid()) {
    return 0.0f;
  }

  const float fontHeight = font.getAscent() - font.getDescent();
  const float scale = fontHeight > 0.0f ? fontSize / fontHeight : 0.0f;
  float pen = 0.0f;
  uint32 previousSlot = Font::INVALID_SLOT;
  SIZE_T position = 0;
  while (position < end && position < text.size()) {
    const WCHAR32 character = UTF8::decodeNext(text, position);
    const uint32 slot = font.findGlyph(bPassword ? '*' : character);
    if (previousSlot != Font::INVALID_SLOT) {
      pen += font.getKerning(previousSlot, slot) * scale;
    }
    pen += font.getGlyph(slot).advance * scale;
    previousSlot = slot;
  }
  return pen;
}

} // namespace

UIEditableTextBox::UIEditableTextBox()
{
  setFocusable(true);
}

UIEditableTextBox::~UIEditableTextBox() = default;

/*
 */
void
UIEditableTextBox::setText(StringView text)
{
  m_text = text;
  m_caretIndex = m_text.size();
  invalidateLayout();
  resetBlink();
}

/*
 */
void
UIEditableTextBox::setHintText(StringView text)
{
  m_hintText = text;
  invalidateLayout();
}

/*
 */
Font*
UIEditableTextBox::getFont() const
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
UIEditableTextBox::setFont(Font* font) noexcept
{
  m_font = font;
  invalidateLayout();
}

/*
 */
void
UIEditableTextBox::setFontSize(float size) noexcept
{
  m_fontSize = size;
  invalidateLayout();
}

/*
 */
void
UIEditableTextBox::setTextColor(const LinearColor& color)
{
  m_textColor = color;
  invalidateLayout();
}

/*
 */
void
UIEditableTextBox::setHintColor(const LinearColor& color)
{
  m_hintColor = color;
  invalidateLayout();
}

/*
 */
void
UIEditableTextBox::setCaretIndex(SIZE_T index)
{
  if (index > m_text.size()) {
    index = m_text.size();
  }
  // Never leave the caret inside a UTF-8 sequence.
  while (index > 0 && index < m_text.size() &&
         (static_cast<uint8>(m_text[index]) & 0xC0) == 0x80) {
    --index;
  }
  if (index != m_caretIndex) {
    m_caretIndex = index;
    resetBlink();
  }
}

/*
 */
Vector2
UIEditableTextBox::getDesiredSize() const
{
  ensureLayout();
  const TextLayout& layout = m_text.empty() ? m_hintLayout : m_layout;
  Vector2 size(layout.getSize().x, Math::max(layout.getSize().y, m_fontSize));
  size.x += m_padding.getHorizontal();
  size.y += m_padding.getVertical();
  size.x = Math::max(size.x, kMinWidth);
  size.y = Math::max(size.y, kMinHeight);
  return size;
}

/*
 */
void
UIEditableTextBox::paint(UIDrawList& drawList) const
{
  const UIRect rect = getRect();
  if (m_backgroundColor.a > 0.0f) {
    drawList.addRect(rect, m_backgroundColor);
  }

  Font* font = getFont();
  if (font == nullptr || !font->isValid()) {
    return;
  }

  ensureLayout();
  const Vector2 textPosition = rect.position + Vector2(m_padding.left, m_padding.top);

  if (!m_text.empty()) {
    if (!m_layout.getGlyphs().empty()) {
      drawList.addText(*font, m_layout, textPosition);
    }
  }
  else if (!m_hintText.empty() && !m_hintLayout.getGlyphs().empty()) {
    drawList.addText(*font, m_hintLayout, textPosition);
  }

  if (!isFocused() || !m_bCaretVisible || m_bReadOnly) {
    return;
  }

  const float availableWidth =
      Math::max(0.0f, rect.size.x - m_padding.getHorizontal());
  const float caretOffset = Math::min(measureCaretOffset(), availableWidth);
  const float caretHeight =
      Math::max(m_fontSize, Math::min(m_fontSize, rect.size.y - m_padding.getVertical()));
  const Vector2 caretPosition(rect.position.x + m_padding.left + caretOffset,
                              rect.position.y + m_padding.top);
  drawList.addRect({caretPosition, {kCaretWidth, caretHeight}}, m_caretColor);
}

/*
 */
void
UIEditableTextBox::onTick(float deltaTime)
{
  if (!isFocused()) {
    return;
  }

  m_blinkTime += deltaTime;
  if (m_blinkTime >= kCaretBlinkSeconds) {
    m_blinkTime = 0.0f;
    m_bCaretVisible = !m_bCaretVisible;
  }
}

/*
 */
void
UIEditableTextBox::onPointerDown(const Vector2& localPosition)
{
  if (!isEnabled()) {
    return;
  }
  setCaretIndex(caretIndexFromX(localPosition.x - m_padding.left));
}

/*
 */
void
UIEditableTextBox::onFocusReceived()
{
  m_caretIndex = m_text.size();
  m_bCaretVisible = true;
  m_blinkTime = 0.0f;
}

/*
 */
void
UIEditableTextBox::onFocusLost()
{
  m_bCaretVisible = false;
  commit();
}

/*
 */
void
UIEditableTextBox::onSubmit()
{
  commit();
  resetBlink();
}

/*
 */
void
UIEditableTextBox::onCancel()
{
  commit();
  resetBlink();
}

/*
 */
void
UIEditableTextBox::onTextInput(StringView text)
{
  if (!isEnabled() || m_bReadOnly) {
    return;
  }

  // Single line: line breaks and carriage returns never reach the field.
  String filtered;
  for (const ANSICHAR character : text) {
    if (character != '\r' && character != '\n') {
      filtered += character;
    }
  }
  if (filtered.empty()) {
    return;
  }

  if (m_maxLength > 0) {
    const SIZE_T current = countCodepoints(m_text);
    if (current >= static_cast<SIZE_T>(m_maxLength)) {
      return;
    }
    const SIZE_T room = static_cast<SIZE_T>(m_maxLength) - current;
    const SIZE_T length = truncateToCodepoints(filtered, room);
    filtered.resize(length);
  }

  m_text.insert(m_caretIndex, filtered);
  m_caretIndex += filtered.size();
  invalidateLayout();
  resetBlink();
  notifyTextChanged();
}

/*
 */
bool
UIEditableTextBox::onTextEditingKey(InputKey key, bool bControl)
{
  CH_PARAMETER_UNUSED(bControl);
  if (!isEnabled() || m_bReadOnly) {
    return false;
  }

  bool bChanged = false;
  switch (key) {
    case InputKey::Backspace:
      if (m_caretIndex > 0) {
        const SIZE_T start = previousCodepointStart(m_text, m_caretIndex);
        m_text.erase(start, m_caretIndex - start);
        m_caretIndex = start;
        bChanged = true;
      }
      break;

    case InputKey::Delete:
      if (m_caretIndex < m_text.size()) {
        const SIZE_T end = nextCodepointEnd(m_text, m_caretIndex);
        m_text.erase(m_caretIndex, end - m_caretIndex);
        bChanged = true;
      }
      break;

    case InputKey::Left:
      if (m_caretIndex > 0) {
        m_caretIndex = previousCodepointStart(m_text, m_caretIndex);
        bChanged = true;
      }
      break;

    case InputKey::Right:
      if (m_caretIndex < m_text.size()) {
        m_caretIndex = nextCodepointEnd(m_text, m_caretIndex);
        bChanged = true;
      }
      break;

    case InputKey::Home:
      if (m_caretIndex != 0) {
        m_caretIndex = 0;
        bChanged = true;
      }
      break;

    case InputKey::End:
      if (m_caretIndex != m_text.size()) {
        m_caretIndex = m_text.size();
        bChanged = true;
      }
      break;

    default: return false;
  }

  if (!bChanged) {
    return false;
  }

  invalidateLayout();
  resetBlink();
  notifyTextChanged();
  return true;
}

/*
 */
bool
UIEditableTextBox::onNavigation(UINavigation direction)
{
  switch (direction) {
    case UINavigation::Up:
    case UINavigation::Down:
    case UINavigation::Left:
    case UINavigation::Right:
      // Consumed, so the viewport does not move focus away; processTextEditingKeys moves
      // the caret for the horizontal ones.
      return true;
    case UINavigation::Next:
    case UINavigation::Previous:
    case UINavigation::COUNT:
      break;
  }
  return false;
}

/*
 */
void
UIEditableTextBox::ensureLayout() const
{
  if (!m_bLayoutDirty) {
    return;
  }

  Font* font = getFont();
  if (font == nullptr || !font->isValid()) {
    m_layout.clear();
    m_hintLayout.clear();
    m_bLayoutDirty = false;
    return;
  }

  TextLayoutDesc desc;
  desc.size = m_fontSize;
  desc.wrapWidth = 0.0f;
  desc.align = TextAlign::Left;
  desc.pivot = {0.0f, 1.0f};
  // Typed brackets are text, not color tags.
  desc.bRichText = false;
  desc.color = m_textColor;

  const String display = getDisplayText();
  m_layout.build(*font, display, desc);

  if (m_text.empty() && !m_hintText.empty()) {
    desc.color = m_hintColor;
    m_hintLayout.build(*font, m_hintText, desc);
  }
  else {
    m_hintLayout.clear();
  }

  m_bLayoutDirty = false;
}

/*
 */
String
UIEditableTextBox::getDisplayText() const
{
  if (!m_bPassword) {
    return m_text;
  }

  String display;
  display.reserve(m_text.size());
  SIZE_T position = 0;
  while (position < m_text.size()) {
    (void)UTF8::decodeNext(m_text, position);
    display += '*';
  }
  return display;
}

/*
 */
float
UIEditableTextBox::measureCaretOffset() const
{
  Font* font = getFont();
  if (font == nullptr || !font->isValid()) {
    return 0.0f;
  }
  return measureWidth(*font, m_text, m_caretIndex, m_bPassword, m_fontSize);
}

/*
 */
SIZE_T
UIEditableTextBox::caretIndexFromX(float localX) const
{
  Font* font = getFont();
  if (font == nullptr || !font->isValid()) {
    return localX <= 0.0f ? 0 : m_text.size();
  }

  const float fontHeight = font->getAscent() - font->getDescent();
  const float scale = fontHeight > 0.0f ? m_fontSize / fontHeight : 0.0f;
  float pen = 0.0f;
  uint32 previousSlot = Font::INVALID_SLOT;

  SIZE_T position = 0;
  while (position < m_text.size()) {
    const SIZE_T start = position;
    const WCHAR32 character = UTF8::decodeNext(m_text, position);
    const uint32 slot = font->findGlyph(m_bPassword ? '*' : character);
    float advance = 0.0f;
    if (previousSlot != Font::INVALID_SLOT) {
      advance += font->getKerning(previousSlot, slot) * scale;
    }
    advance += font->getGlyph(slot).advance * scale;

    if (localX < pen + advance * 0.5f) {
      return start;
    }
    pen += advance;
    previousSlot = slot;
  }
  return m_text.size();
}

/*
 */
void
UIEditableTextBox::notifyTextChanged()
{
  if (m_onTextChanged) {
    m_onTextChanged(m_text);
  }
}

/*
 */
void
UIEditableTextBox::commit()
{
  if (m_onTextCommitted) {
    m_onTextCommitted(m_text);
  }
}

/*
 */
void
UIEditableTextBox::resetBlink() noexcept
{
  m_blinkTime = 0.0f;
  m_bCaretVisible = true;
}

} // namespace chEngineSDK
