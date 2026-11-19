/************************************************************************/
/**
 * @file chUIButton.cpp
 * @author Swampertor
 * @date 2026/11/19
 * @brief Clickable widget with one piece of content, as UMG's UButton.
 */
/************************************************************************/
#include "chUIButton.h"

#include "chUIDrawList.h"

namespace chEngineSDK {

UIButton::UIButton()
{
  // The base content widget is a layout construct; a button is a control.
  setFocusable(true);
}

UIButton::~UIButton() = default;

/*
 */
void
UIButton::paint(UIDrawList& drawList) const
{
  const LinearColor& color = !isEnabled()                  ? m_disabledColor
                             : m_bPressed                  ? m_pressedColor
                             : (m_bHovered || isFocused()) ? m_hoveredColor
                                                           : m_normalColor;
  drawList.addRect(getRect(), color);
}

/*
 */
void
UIButton::onPointerEnter(const Vector2& localPosition)
{
  CH_PARAMETER_UNUSED(localPosition);
  if (isEnabled()) {
    m_bHovered = true;
  }
}

/*
 */
void
UIButton::onPointerExit(const Vector2& localPosition)
{
  CH_PARAMETER_UNUSED(localPosition);
  m_bHovered = false;
  m_bPressed = false;
}

/*
 */
void
UIButton::onPointerDown(const Vector2& localPosition)
{
  CH_PARAMETER_UNUSED(localPosition);
  if (isEnabled()) {
    m_bPressed = true;
  }
}

/*
 */
void
UIButton::onPointerUp(const Vector2& localPosition)
{
  CH_PARAMETER_UNUSED(localPosition);
  m_bPressed = false;
}

/*
 */
void
UIButton::onPointerClick(const Vector2& localPosition)
{
  CH_PARAMETER_UNUSED(localPosition);
  if (isEnabled()) {
    click();
  }
}

/*
 */
void
UIButton::onFocusLost()
{
  m_bPressed = false;
}

/*
 */
void
UIButton::onSubmit()
{
  if (isEnabled()) {
    click();
  }
}

/*
 */
void
UIButton::click()
{
  if (m_onClicked) {
    m_onClicked();
  }
}

} // namespace chEngineSDK
