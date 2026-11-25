/************************************************************************/
/**
 * @file chUISlider.cpp
 * @author Swampertor
 * @date 2026/11/24
 * @brief Control that picks a number in a range, as UMG's USlider.
 */
/************************************************************************/
#include "chUISlider.h"

#include "chMath.h"
#include "chUIDrawList.h"

namespace chEngineSDK {

namespace {
constexpr float kTrackThickness = 4.0f;
constexpr float kThumbLength = 16.0f;
constexpr float kThumbThickness = 10.0f;
} // namespace

UISlider::UISlider()
{
  setFocusable(true);
}

UISlider::~UISlider() = default;

/*
 */
void
UISlider::setValue(float value, bool bNotify)
{
  const float snapped = snapValue(value);
  if (Math::abs(snapped - m_value) <= 1.0e-6f) {
    return;
  }

  m_value = snapped;
  if (bNotify && m_onValueChanged) {
    m_onValueChanged(m_value);
  }
}

/*
 */
float
UISlider::getNormalizedValue() const noexcept
{
  const float range = m_maxValue - m_minValue;
  if (range <= 0.0f) {
    return 0.0f;
  }
  return Math::clamp((m_value - m_minValue) / range, 0.0f, 1.0f);
}

/*
 */
Vector2
UISlider::getDesiredSize() const
{
  return m_orientation == UIOrientation::Horizontal ? Vector2(160.0f, 20.0f)
                                                    : Vector2(20.0f, 160.0f);
}

/*
 */
void
UISlider::paint(UIDrawList& drawList) const
{
  const Vector2 position = getPosition();
  const Vector2 size = getSize();
  const float alpha = isEnabled() ? 1.0f : 0.5f;
  LinearColor track = m_trackColor;
  LinearColor fill = m_fillColor;
  LinearColor thumb = m_thumbColor;
  if (!isEnabled()) {
    track = m_disabledColor;
    fill = m_disabledColor;
    thumb = m_disabledColor;
  }
  track.a *= alpha;
  fill.a *= alpha;
  thumb.a *= alpha;

  const float ratio = getNormalizedValue();

  if (m_orientation == UIOrientation::Horizontal) {
    const float centerY = position.y + size.y * 0.5f;
    const float trackY = centerY - kTrackThickness * 0.5f;
    const float travel = Math::max(0.0f, size.x - kThumbThickness);
    drawList.addRect({{position.x, trackY}, {size.x, kTrackThickness}}, track);
    drawList.addRect({{position.x, trackY},
                      {kThumbThickness * 0.5f + travel * ratio, kTrackThickness}},
                     fill);
    const float thumbX = position.x + travel * ratio;
    drawList.addRect({{thumbX, centerY - kThumbLength * 0.5f},
                      {kThumbThickness, kThumbLength}},
                     thumb);
    return;
  }

  const float centerX = position.x + size.x * 0.5f;
  const float trackX = centerX - kTrackThickness * 0.5f;
  const float travel = Math::max(0.0f, size.y - kThumbThickness);
  drawList.addRect({{trackX, position.y}, {kTrackThickness, size.y}}, track);
  // Vertical sliders grow towards the top, so the fill starts at the bottom.
  const float fillHeight = kThumbThickness * 0.5f + travel * ratio;
  drawList.addRect({{trackX, position.y + size.y - fillHeight},
                    {kTrackThickness, fillHeight}},
                   fill);
  const float thumbY = position.y + size.y - kThumbThickness * 0.5f - travel * ratio;
  drawList.addRect({{centerX - kThumbLength * 0.5f, thumbY},
                    {kThumbLength, kThumbThickness}},
                   thumb);
}

/*
 */
void
UISlider::onPointerDown(const Vector2& localPosition)
{
  if (!isEnabled()) {
    return;
  }
  m_bDragging = true;
  updateValueFromPointer(localPosition);
}

/*
 */
void
UISlider::onPointerMove(const Vector2& localPosition)
{
  if (m_bDragging && isEnabled()) {
    updateValueFromPointer(localPosition);
  }
}

/*
 */
void
UISlider::onPointerUp(const Vector2& localPosition)
{
  CH_PARAMETER_UNUSED(localPosition);
  m_bDragging = false;
}

/*
 */
void
UISlider::onPointerClick(const Vector2& localPosition)
{
  if (isEnabled() && !m_bDragging) {
    updateValueFromPointer(localPosition);
  }
}

/*
 */
bool
UISlider::onScroll(float delta)
{
  if (!isEnabled() || delta == 0.0f) {
    return false;
  }

  setValue(m_value + delta * m_step);
  return true;
}

/*
 */
bool
UISlider::onNavigation(UINavigation direction)
{
  if (!isEnabled()) {
    return false;
  }

  switch (direction) {
    case UINavigation::Up:
    case UINavigation::Right:
      setValue(m_value + m_step);
      return true;
    case UINavigation::Down:
    case UINavigation::Left:
      setValue(m_value - m_step);
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
UISlider::onFocusLost()
{
  m_bDragging = false;
}

/*
 */
void
UISlider::updateValueFromPointer(const Vector2& localPosition)
{
  const Vector2 size = getSize();
  const float travel = m_orientation == UIOrientation::Horizontal
                           ? Math::max(0.0f, size.x - kThumbThickness)
                           : Math::max(0.0f, size.y - kThumbThickness);
  if (travel <= 0.0f) {
    return;
  }

  float ratio = 0.0f;
  if (m_orientation == UIOrientation::Horizontal) {
    ratio = (localPosition.x - kThumbThickness * 0.5f) / travel;
  }
  else {
    ratio = (size.y - kThumbThickness * 0.5f - localPosition.y) / travel;
  }
  ratio = Math::clamp(ratio, 0.0f, 1.0f);
  setValue(m_minValue + (m_maxValue - m_minValue) * ratio);
}

/*
 */
float
UISlider::snapValue(float value) const noexcept
{
  const float low = Math::min(m_minValue, m_maxValue);
  const float high = Math::max(m_minValue, m_maxValue);
  float result = Math::clamp(value, low, high);

  if (m_step > 0.0f) {
    const float steps = Math::floor((result - low) / m_step + 0.5f);
    result = Math::clamp(low + steps * m_step, low, high);
  }
  return result;
}

} // namespace chEngineSDK
