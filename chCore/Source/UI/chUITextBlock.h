/************************************************************************/
/**
 * @file chUITextBlock.h
 * @author Swampertor
 * @date 2026/11/19
 * @brief Widget that draws a laid-out text, as UMG's UTextBlock.
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
 * Draws a string inside its rectangle, optionally wrapping it at the widget's width. The
 * glyphs are laid out once and cached; the layout is rebuilt when the text, the font, the
 * settings or the wrapping width change.
 *
 * The font is not owned: a document keeps its font assets alive, as it does for textures.
 * Without one, the text uses Font::getDefault().
 *
 * The text starts at the widget's top-left plus its margin. The render transform is not
 * applied to text yet.
 *
 * Labels default to SelfHitTestInvisible, so text used as button content does not take the
 * pointer away from the button. Set Visible explicitly when a label should block input.
 */
class CH_CORE_EXPORT UITextBlock : public UIWidget
{
 public:
  UITextBlock();
  ~UITextBlock() override;

  NODISCARD const ANSICHAR*
  getTypeName() const noexcept override
  {
    return "UITextBlock";
  }

  // -- Text -------------------------------------------------------------------

  NODISCARD FORCEINLINE const String&
  getText() const noexcept
  {
    return m_text;
  }

  void
  setText(StringView text);

  NODISCARD Font*
  getFont() const;

  void
  setFont(Font* font) noexcept;

  /**
   * Height from the lowest to the highest point of the font, in pixels.
   */
  NODISCARD FORCEINLINE float
  getFontSize() const noexcept
  {
    return m_fontSize;
  }

  void
  setFontSize(float size) noexcept;

  NODISCARD FORCEINLINE TextAlign
  getAlign() const noexcept
  {
    return m_align;
  }

  void
  setAlign(TextAlign align) noexcept;

  /**
   * Whether lines longer than the widget's width break at a space.
   */
  NODISCARD FORCEINLINE bool
  getWrapText() const noexcept
  {
    return m_bWrapText;
  }

  void
  setWrapText(bool wrap) noexcept;

  NODISCARD FORCEINLINE const UIMargin&
  getMargin() const noexcept
  {
    return m_margin;
  }

  void
  setMargin(const UIMargin& margin) noexcept;

  NODISCARD FORCEINLINE float
  getLineSpacing() const noexcept
  {
    return m_lineSpacing;
  }

  void
  setLineSpacing(float spacing) noexcept;

  NODISCARD FORCEINLINE float
  getLetterSpacing() const noexcept
  {
    return m_letterSpacing;
  }

  void
  setLetterSpacing(float spacing) noexcept;

  NODISCARD FORCEINLINE bool
  getMonospaceDigits() const noexcept
  {
    return m_bMonospaceDigits;
  }

  void
  setMonospaceDigits(bool monospace) noexcept;

  NODISCARD FORCEINLINE bool
  getRichText() const noexcept
  {
    return m_bRichText;
  }

  void
  setRichText(bool richText) noexcept;

  /**
   * The text color; setTextColor rebuilds the layout, which carries the glyph colors.
   */
  NODISCARD FORCEINLINE const LinearColor&
  getTextColor() const noexcept
  {
    return getColor();
  }

  void
  setTextColor(const LinearColor& color);

  /**
   * Width and height the text needs with the current settings, margin included.
   */
  NODISCARD Vector2
  getDesiredSize() const override;

  // -- Widget -----------------------------------------------------------------

  void
  arrange(const UIGeometry& geometry) override;

  void
  paint(UIDrawList& drawList) const override;

 private:
  void
  invalidateLayout() noexcept
  {
    m_bLayoutDirty = true;
  }

  void
  ensureLayout() const;

  String m_text;
  Font* m_font = nullptr;
  float m_fontSize = 16.0f;
  TextAlign m_align = TextAlign::Left;
  float m_lineSpacing = 1.0f;
  float m_letterSpacing = 0.0f;
  bool m_bWrapText = false;
  bool m_bMonospaceDigits = false;
  bool m_bRichText = true;
  UIMargin m_margin;

  mutable TextLayout m_layout;
  mutable bool m_bLayoutDirty = true;
  mutable float m_layoutWrapWidth = -1.0f;
};

} // namespace chEngineSDK
