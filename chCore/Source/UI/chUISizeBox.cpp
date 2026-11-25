/************************************************************************/
/**
 * @file chUISizeBox.cpp
 * @author Swampertor
 * @date 2026/11/24
 * @brief Layout wrapper that keeps one child at a fixed size, as UMG's USizeBox.
 */
/************************************************************************/
#include "chUISizeBox.h"

#include "chMath.h"

namespace chEngineSDK {

UISizeBox::UISizeBox()
{
  // A size box is a layout construct: it neither paints nor takes the pointer.
  setVisibility(UIVisibility::SelfHitTestInvisible);
}

UISizeBox::~UISizeBox() = default;

/*
 */
Vector2
UISizeBox::getDesiredSize() const
{
  Vector2 size = Vector2::ZERO;
  if (const UIWidget* content = getContent()) {
    size = content->getDesiredSize();
  }
  if (m_widthOverride > 0.0f) {
    size.x = m_widthOverride;
  }
  if (m_heightOverride > 0.0f) {
    size.y = m_heightOverride;
  }
  return size;
}

/*
 */
void
UISizeBox::arrangeChildren()
{
  UIWidget* content = getContent();
  if (content == nullptr) {
    return;
  }

  UIGeometry childGeometry = getGeometry();
  if (m_widthOverride > 0.0f) {
    childGeometry.size.x = Math::min(childGeometry.size.x, m_widthOverride);
  }
  if (m_heightOverride > 0.0f) {
    childGeometry.size.y = Math::min(childGeometry.size.y, m_heightOverride);
  }

  UIGeometry arranged = childGeometry;
  if (UIPanelSlot* slot = getContentSlot()) {
    slot->arrange(childGeometry, arranged);
  }
  content->arrange(arranged);
}

} // namespace chEngineSDK
