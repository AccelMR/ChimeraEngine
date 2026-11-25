/************************************************************************/
/**
 * @file chUICheckBox.h
 * @author Swampertor
 * @date 2026/11/24
 * @brief Control with a check box and one label, as UMG's UCheckBox.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chLinearColor.h"
#include "chUIContentWidget.h"

namespace chEngineSDK {

/**
 * A focusable toggle. The check box is drawn at the left and its content fills the rest,
 * so a UITextBlock used as content is the label. Clicking it or submitting while it has
 * focus toggles the state and calls onCheckStateChanged.
 */
class CH_CORE_EXPORT UICheckBox : public UIContentWidget
{
 public:
  UICheckBox();
  ~UICheckBox() override;

  NODISCARD const ANSICHAR*
  getTypeName() const noexcept override
  {
    return "UICheckBox";
  }

  // -- State ------------------------------------------------------------------

  NODISCARD FORCEINLINE bool
  isChecked() const noexcept
  {
    return m_bChecked;
  }

  /**
   * Sets the state. The callback fires only when it changes, unless bNotify is false.
   */
  void
  setIsChecked(bool checked, bool bNotify = true);

  NODISCARD FORCEINLINE bool
  isHovered() const noexcept
  {
    return m_bHovered;
  }

  NODISCARD FORCEINLINE bool
  isPressed() const noexcept
  {
    return m_bPressed;
  }

  // -- Callback ---------------------------------------------------------------

  void
  setOnCheckStateChanged(Function<void(bool)> callback)
  {
    m_onCheckStateChanged = std::move(callback);
  }

  NODISCARD FORCEINLINE const Function<void(bool)>&
  getOnCheckStateChanged() const noexcept
  {
    return m_onCheckStateChanged;
  }

  // -- Colors -----------------------------------------------------------------

  NODISCARD FORCEINLINE const LinearColor&
  getNormalColor() const noexcept
  {
    return m_normalColor;
  }

  FORCEINLINE void
  setNormalColor(const LinearColor& color) noexcept
  {
    m_normalColor = color;
  }

  NODISCARD FORCEINLINE const LinearColor&
  getHoveredColor() const noexcept
  {
    return m_hoveredColor;
  }

  FORCEINLINE void
  setHoveredColor(const LinearColor& color) noexcept
  {
    m_hoveredColor = color;
  }

  NODISCARD FORCEINLINE const LinearColor&
  getPressedColor() const noexcept
  {
    return m_pressedColor;
  }

  FORCEINLINE void
  setPressedColor(const LinearColor& color) noexcept
  {
    m_pressedColor = color;
  }

  NODISCARD FORCEINLINE const LinearColor&
  getDisabledColor() const noexcept
  {
    return m_disabledColor;
  }

  FORCEINLINE void
  setDisabledColor(const LinearColor& color) noexcept
  {
    m_disabledColor = color;
  }

  NODISCARD FORCEINLINE const LinearColor&
  getCheckMarkColor() const noexcept
  {
    return m_checkMarkColor;
  }

  FORCEINLINE void
  setCheckMarkColor(const LinearColor& color) noexcept
  {
    m_checkMarkColor = color;
  }

  // -- Size -------------------------------------------------------------------

  NODISCARD FORCEINLINE float
  getCheckBoxSize() const noexcept
  {
    return m_checkBoxSize;
  }

  FORCEINLINE void
  setCheckBoxSize(float size) noexcept
  {
    m_checkBoxSize = size;
  }

  NODISCARD FORCEINLINE float
  getSpacing() const noexcept
  {
    return m_spacing;
  }

  FORCEINLINE void
  setSpacing(float spacing) noexcept
  {
    m_spacing = spacing;
  }

  // -- Widget -----------------------------------------------------------------

  NODISCARD Vector2
  getDesiredSize() const override;

  void
  paint(UIDrawList& drawList) const override;

  void
  onPointerEnter(const Vector2& localPosition) override;

  void
  onPointerExit(const Vector2& localPosition) override;

  void
  onPointerDown(const Vector2& localPosition) override;

  void
  onPointerUp(const Vector2& localPosition) override;

  void
  onPointerClick(const Vector2& localPosition) override;

  void
  onFocusLost() override;

  void
  onSubmit() override;

 protected:
  void
  arrangeChildren() override;

 private:
  void
  toggle();

  Function<void(bool)> m_onCheckStateChanged;
  LinearColor m_normalColor{0.16f, 0.16f, 0.18f, 1.0f};
  LinearColor m_hoveredColor{0.24f, 0.24f, 0.28f, 1.0f};
  LinearColor m_pressedColor{0.10f, 0.10f, 0.12f, 1.0f};
  LinearColor m_disabledColor{0.10f, 0.10f, 0.10f, 0.5f};
  LinearColor m_checkMarkColor{0.95f, 0.95f, 0.95f, 1.0f};
  float m_checkBoxSize = 18.0f;
  float m_spacing = 8.0f;
  bool m_bChecked = false;
  bool m_bHovered = false;
  bool m_bPressed = false;
};

} // namespace chEngineSDK
