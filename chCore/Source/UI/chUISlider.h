/************************************************************************/
/**
 * @file chUISlider.h
 * @author Swampertor
 * @date 2026/11/24
 * @brief Control that picks a number in a range, as UMG's USlider.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chLinearColor.h"
#include "chUITypes.h"
#include "chUIWidget.h"

namespace chEngineSDK {

/**
 * A focusable track and thumb. Pressing or dragging on it sets the value from the pointer,
 * the wheel and the arrow navigation keys step it, and onValueChanged fires whenever the
 * value moves.
 *
 * Horizontal sliders grow to the right; vertical ones grow towards the top.
 */
class CH_CORE_EXPORT UISlider : public UIWidget
{
 public:
  UISlider();
  ~UISlider() override;

  NODISCARD const ANSICHAR*
  getTypeName() const noexcept override
  {
    return "UISlider";
  }

  // -- Range ------------------------------------------------------------------

  NODISCARD FORCEINLINE float
  getValue() const noexcept
  {
    return m_value;
  }

  /**
   * Sets the value, snapped to the step and clamped to the range; the callback fires only
   * when the value changes, unless bNotify is false.
   */
  void
  setValue(float value, bool bNotify = true);

  NODISCARD FORCEINLINE float
  getMinValue() const noexcept
  {
    return m_minValue;
  }

  FORCEINLINE void
  setMinValue(float value) noexcept
  {
    m_minValue = value;
  }

  NODISCARD FORCEINLINE float
  getMaxValue() const noexcept
  {
    return m_maxValue;
  }

  FORCEINLINE void
  setMaxValue(float value) noexcept
  {
    m_maxValue = value;
  }

  NODISCARD FORCEINLINE float
  getStep() const noexcept
  {
    return m_step;
  }

  FORCEINLINE void
  setStep(float step) noexcept
  {
    m_step = step;
  }

  /**
   * Value as 0 to 1, for painting and for callers that only want the position.
   */
  NODISCARD float
  getNormalizedValue() const noexcept;

  FORCEINLINE UIOrientation
  getOrientation() const noexcept
  {
    return m_orientation;
  }

  FORCEINLINE void
  setOrientation(UIOrientation orientation) noexcept
  {
    m_orientation = orientation;
  }

  NODISCARD FORCEINLINE bool
  isDragging() const noexcept
  {
    return m_bDragging;
  }

  // -- Callback ---------------------------------------------------------------

  void
  setOnValueChanged(Function<void(float)> callback)
  {
    m_onValueChanged = std::move(callback);
  }

  NODISCARD FORCEINLINE const Function<void(float)>&
  getOnValueChanged() const noexcept
  {
    return m_onValueChanged;
  }

  // -- Colors -----------------------------------------------------------------

  NODISCARD FORCEINLINE const LinearColor&
  getTrackColor() const noexcept
  {
    return m_trackColor;
  }

  FORCEINLINE void
  setTrackColor(const LinearColor& color) noexcept
  {
    m_trackColor = color;
  }

  NODISCARD FORCEINLINE const LinearColor&
  getFillColor() const noexcept
  {
    return m_fillColor;
  }

  FORCEINLINE void
  setFillColor(const LinearColor& color) noexcept
  {
    m_fillColor = color;
  }

  NODISCARD FORCEINLINE const LinearColor&
  getThumbColor() const noexcept
  {
    return m_thumbColor;
  }

  FORCEINLINE void
  setThumbColor(const LinearColor& color) noexcept
  {
    m_thumbColor = color;
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

  NODISCARD Vector2
  getDesiredSize() const override;

  void
  paint(UIDrawList& drawList) const override;

  void
  onPointerDown(const Vector2& localPosition) override;

  void
  onPointerMove(const Vector2& localPosition) override;

  void
  onPointerUp(const Vector2& localPosition) override;

  void
  onPointerClick(const Vector2& localPosition) override;

  bool
  onScroll(float delta) override;

  bool
  onNavigation(UINavigation direction) override;

  void
  onFocusLost() override;

 private:
  /**
   * Maps a local pointer position to the value, using the track the thumb can travel.
   */
  void
  updateValueFromPointer(const Vector2& localPosition);

  NODISCARD float
  snapValue(float value) const noexcept;

  Function<void(float)> m_onValueChanged;
  LinearColor m_trackColor{0.22f, 0.22f, 0.25f, 1.0f};
  LinearColor m_fillColor{0.35f, 0.62f, 0.95f, 1.0f};
  LinearColor m_thumbColor{0.92f, 0.92f, 0.95f, 1.0f};
  LinearColor m_disabledColor{0.12f, 0.12f, 0.12f, 0.5f};
  float m_value = 0.0f;
  float m_minValue = 0.0f;
  float m_maxValue = 1.0f;
  float m_step = 0.01f;
  UIOrientation m_orientation = UIOrientation::Horizontal;
  bool m_bDragging = false;
};

} // namespace chEngineSDK
