/************************************************************************/
/**
 * @file chUIButton.h
 * @author Swampertor
 * @date 2026/11/19
 * @brief Clickable widget with one piece of content, as UMG's UButton.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chLinearColor.h"
#include "chUIContentWidget.h"

namespace chEngineSDK {

/**
 * A focusable control that paints a state-colored background and calls a callback when it
 * is clicked or submitted. Its content is arranged by the base content widget, filling the
 * button unless its slot says otherwise.
 *
 * The pointer hooks are driven by UIViewport in point 5; until then a game or a test can
 * call them directly.
 */
class CH_CORE_EXPORT UIButton : public UIContentWidget
{
 public:
  UIButton();
  ~UIButton() override;

  NODISCARD const ANSICHAR*
  getTypeName() const noexcept override
  {
    return "UIButton";
  }

  // -- State ------------------------------------------------------------------

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
  setOnClicked(Function<void()> callback)
  {
    m_onClicked = std::move(callback);
  }

  NODISCARD FORCEINLINE const Function<void()>&
  getOnClicked() const noexcept
  {
    return m_onClicked;
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

  // -- Widget -----------------------------------------------------------------

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

 private:
  void
  click();

  Function<void()> m_onClicked;
  LinearColor m_normalColor{0.16f, 0.16f, 0.18f, 1.0f};
  LinearColor m_hoveredColor{0.24f, 0.24f, 0.28f, 1.0f};
  LinearColor m_pressedColor{0.10f, 0.10f, 0.12f, 1.0f};
  LinearColor m_disabledColor{0.10f, 0.10f, 0.10f, 0.5f};
  bool m_bHovered = false;
  bool m_bPressed = false;
};

} // namespace chEngineSDK
