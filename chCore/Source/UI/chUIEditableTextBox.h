/************************************************************************/
/**
 * @file chUIEditableTextBox.h
 * @author Swampertor
 * @date 2026/11/24
 * @brief Single-line text field, as UMG's UEditableTextBox.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chLinearColor.h"
#include "chTextLayout.h"
#include "chUIRect.h"
#include "chUIWidget.h"

namespace chEngineSDK {
class Font;

/**
 * A focusable control that edits one line of UTF-8 text. While it has focus it takes the
 * keyboard: the viewport routes typed characters and the editing keys to it and reports
 * bKeyboardCaptured, so gameplay does not read them.
 *
 * The caret is a byte offset into the text, always on a character boundary. Selection,
 * clipboard and multi-line editing are not implemented yet; this is the single-line field
 * the sample and the first documents need.
 */
class CH_CORE_EXPORT UIEditableTextBox : public UIWidget
{
 public:
  UIEditableTextBox();
  ~UIEditableTextBox() override;

  NODISCARD const ANSICHAR*
  getTypeName() const noexcept override
  {
    return "UIEditableTextBox";
  }

  // -- Text -------------------------------------------------------------------

  NODISCARD FORCEINLINE const String&
  getText() const noexcept
  {
    return m_text;
  }

  /**
   * Replaces the text and puts the caret at its end. Does not call onTextChanged.
   */
  void
  setText(StringView text);

  NODISCARD FORCEINLINE const String&
  getHintText() const noexcept
  {
    return m_hintText;
  }

  void
  setHintText(StringView text);

  NODISCARD Font*
  getFont() const;

  void
  setFont(Font* font) noexcept;

  NODISCARD FORCEINLINE float
  getFontSize() const noexcept
  {
    return m_fontSize;
  }

  void
  setFontSize(float size) noexcept;

  NODISCARD FORCEINLINE const UIMargin&
  getPadding() const noexcept
  {
    return m_padding;
  }

  FORCEINLINE void
  setPadding(const UIMargin& padding) noexcept
  {
    m_padding = padding;
  }

  /**
   * Run of text entered by the player, in characters; 0 accepts every character.
   */
  NODISCARD FORCEINLINE int32
  getMaxLength() const noexcept
  {
    return m_maxLength;
  }

  FORCEINLINE void
  setMaxLength(int32 length) noexcept
  {
    m_maxLength = length;
  }

  NODISCARD FORCEINLINE bool
  isReadOnly() const noexcept
  {
    return m_bReadOnly;
  }

  FORCEINLINE void
  setReadOnly(bool readOnly) noexcept
  {
    m_bReadOnly = readOnly;
  }

  /**
   * Paints a '*' per character instead of the text.
   */
  NODISCARD FORCEINLINE bool
  isPassword() const noexcept
  {
    return m_bPassword;
  }

  FORCEINLINE void
  setPassword(bool password) noexcept
  {
    m_bPassword = password;
  }

  // -- Caret ------------------------------------------------------------------

  NODISCARD FORCEINLINE SIZE_T
  getCaretIndex() const noexcept
  {
    return m_caretIndex;
  }

  /**
   * Moves the caret, clamped into the text and onto a character boundary.
   */
  void
  setCaretIndex(SIZE_T index);

  // -- Colors -----------------------------------------------------------------

  NODISCARD FORCEINLINE const LinearColor&
  getTextColor() const noexcept
  {
    return m_textColor;
  }

  void
  setTextColor(const LinearColor& color);

  NODISCARD FORCEINLINE const LinearColor&
  getHintColor() const noexcept
  {
    return m_hintColor;
  }

  void
  setHintColor(const LinearColor& color);

  NODISCARD FORCEINLINE const LinearColor&
  getBackgroundColor() const noexcept
  {
    return m_backgroundColor;
  }

  FORCEINLINE void
  setBackgroundColor(const LinearColor& color) noexcept
  {
    m_backgroundColor = color;
  }

  NODISCARD FORCEINLINE const LinearColor&
  getCaretColor() const noexcept
  {
    return m_caretColor;
  }

  FORCEINLINE void
  setCaretColor(const LinearColor& color) noexcept
  {
    m_caretColor = color;
  }

  // -- Callbacks --------------------------------------------------------------

  void
  setOnTextChanged(Function<void(const String&)> callback)
  {
    m_onTextChanged = std::move(callback);
  }

  void
  setOnTextCommitted(Function<void(const String&)> callback)
  {
    m_onTextCommitted = std::move(callback);
  }

  // -- Widget -----------------------------------------------------------------

  NODISCARD Vector2
  getDesiredSize() const override;

  void
  paint(UIDrawList& drawList) const override;

  void
  onTick(float deltaTime) override;

  void
  onPointerDown(const Vector2& localPosition) override;

  void
  onFocusReceived() override;

  void
  onFocusLost() override;

  void
  onSubmit() override;

  void
  onCancel() override;

  NODISCARD bool
  isTextEditor() const noexcept override
  {
    return true;
  }

  void
  onTextInput(StringView text) override;

  bool
  onTextEditingKey(InputKey key, bool bControl) override;

  /**
   * A text field keeps the arrow keys: they move the caret instead of moving focus.
   */
  bool
  onNavigation(UINavigation direction) override;

 private:
  void
  invalidateLayout() noexcept
  {
    m_bLayoutDirty = true;
  }

  void
  ensureLayout() const;

  NODISCARD String
  getDisplayText() const;

  /**
   * Width in pixels of the part of the text before the caret.
   */
  NODISCARD float
  measureCaretOffset() const;

  /**
   * Byte offset of the character the local x lands on.
   */
  NODISCARD SIZE_T
  caretIndexFromX(float localX) const;

  void
  notifyTextChanged();

  void
  commit();

  void
  resetBlink() noexcept;

  String m_text;
  String m_hintText;
  Font* m_font = nullptr;
  float m_fontSize = 16.0f;
  UIMargin m_padding{6.0f, 4.0f, 6.0f, 4.0f};
  int32 m_maxLength = 0;
  bool m_bReadOnly = false;
  bool m_bPassword = false;

  LinearColor m_textColor = LinearColor::WHITE;
  LinearColor m_hintColor{0.55f, 0.57f, 0.60f, 1.0f};
  LinearColor m_backgroundColor{0.06f, 0.06f, 0.08f, 1.0f};
  LinearColor m_caretColor{0.95f, 0.95f, 0.95f, 1.0f};

  Function<void(const String&)> m_onTextChanged;
  Function<void(const String&)> m_onTextCommitted;

  SIZE_T m_caretIndex = 0;
  float m_blinkTime = 0.0f;
  bool m_bCaretVisible = true;

  mutable TextLayout m_layout;
  mutable TextLayout m_hintLayout;
  mutable bool m_bLayoutDirty = true;
};

} // namespace chEngineSDK
