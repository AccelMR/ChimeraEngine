/************************************************************************/
/**
 * @file chUICheckBox.cpp
 * @author Swampertor
 * @date 2026/11/24
 * @brief Control with a check box and one label, as UMG's UCheckBox.
 */
/************************************************************************/
#include "chUICheckBox.h"

#include "chMath.h"
#include "chUIDrawList.h"

namespace chEngineSDK {

UICheckBox::UICheckBox()
{
  // The base content widget is a layout construct; a check box is a control.
  setFocusable(true);
}

UICheckBox::~UICheckBox() = default;

/*
 */
void
UICheckBox::setIsChecked(bool checked, bool bNotify)
{
  if (m_bChecked == checked) {
    return;
  }

  m_bChecked = checked;
  if (bNotify && m_onCheckStateChanged) {
    m_onCheckStateChanged(m_bChecked);
  }
}

/*
 */
Vector2
UICheckBox::getDesiredSize() const
{
  const float boxWidth = m_checkBoxSize + m_spacing;
  Vector2 size(boxWidth, m_checkBoxSize);
  if (const UIWidget* content = getContent()) {
    const Vector2 desired = content->getDesiredSize();
    size.x += desired.x;
    size.y = Math::max(size.y, desired.y);
  }
  return size;
}

/*
 */
void
UICheckBox::paint(UIDrawList& drawList) const
{
  const LinearColor& color = !isEnabled()                  ? m_disabledColor
                             : m_bPressed                   ? m_pressedColor
                             : (m_bHovered || isFocused()) ? m_hoveredColor
                                                           : m_normalColor;

  const Vector2 position = getPosition();
  const Vector2 size = getSize();
  const float boxY = position.y + (size.y - m_checkBoxSize) * 0.5f;
  const UIRect box{{position.x, boxY}, {m_checkBoxSize, m_checkBoxSize}};
  drawList.addRect(box, color);

  if (!m_bChecked) {
    return;
  }

  // Two bars meeting at their ends, the long one rising to the right.
  const float thickness = Math::max(2.0f, m_checkBoxSize * 0.12f);
  const UIRect shortBar{{box.position.x + m_checkBoxSize * 0.18f,
                         box.position.y + m_checkBoxSize * 0.52f},
                        {m_checkBoxSize * 0.32f, thickness}};
  drawList.addRect(shortBar, m_checkMarkColor, Radian(-0.75f), {0.5f, 0.5f});

  const UIRect longBar{{box.position.x + m_checkBoxSize * 0.42f,
                        box.position.y + m_checkBoxSize * 0.62f},
                       {m_checkBoxSize * 0.56f, thickness}};
  drawList.addRect(longBar, m_checkMarkColor, Radian(0.75f), {0.5f, 0.5f});
}

/*
 */
void
UICheckBox::arrangeChildren()
{
  UIWidget* content = getContent();
  if (content == nullptr) {
    return;
  }

  const UIGeometry& parent = getGeometry();
  UIGeometry contentGeometry = parent;
  contentGeometry.position.x += m_checkBoxSize + m_spacing;
  contentGeometry.size.x =
      Math::max(0.0f, parent.size.x - m_checkBoxSize - m_spacing);
  contentGeometry.position.y += 2.0f;
  contentGeometry.size.y = Math::max(0.0f, parent.size.y - 4.0f);

  UIGeometry arranged = contentGeometry;
  if (UIPanelSlot* slot = getContentSlot()) {
    slot->arrange(contentGeometry, arranged);
  }
  content->arrange(arranged);
}

/*
 */
void
UICheckBox::onPointerEnter(const Vector2& localPosition)
{
  CH_PARAMETER_UNUSED(localPosition);
  if (isEnabled()) {
    m_bHovered = true;
  }
}

/*
 */
void
UICheckBox::onPointerExit(const Vector2& localPosition)
{
  CH_PARAMETER_UNUSED(localPosition);
  m_bHovered = false;
  m_bPressed = false;
}

/*
 */
void
UICheckBox::onPointerDown(const Vector2& localPosition)
{
  CH_PARAMETER_UNUSED(localPosition);
  if (isEnabled()) {
    m_bPressed = true;
  }
}

/*
 */
void
UICheckBox::onPointerUp(const Vector2& localPosition)
{
  CH_PARAMETER_UNUSED(localPosition);
  m_bPressed = false;
}

/*
 */
void
UICheckBox::onPointerClick(const Vector2& localPosition)
{
  CH_PARAMETER_UNUSED(localPosition);
  if (isEnabled()) {
    toggle();
  }
}

/*
 */
void
UICheckBox::onFocusLost()
{
  m_bPressed = false;
}

/*
 */
void
UICheckBox::onSubmit()
{
  if (isEnabled()) {
    toggle();
  }
}

/*
 */
void
UICheckBox::toggle()
{
  setIsChecked(!m_bChecked, true);
}

} // namespace chEngineSDK
