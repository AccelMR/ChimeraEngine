/************************************************************************/
/**
 * @file chUIVerticalBox.cpp
 * @author Swampertor
 * @date 2026/11/24
 * @brief Panel that stacks its children top to bottom, as UMG's UVerticalBox.
 */
/************************************************************************/
#include "chUIVerticalBox.h"

#include "chMath.h"

namespace chEngineSDK {

namespace {

/*
 * A child may have been added with another slot type through the base panel; such a child
 * is laid out like a Fill row with no padding.
 */
NODISCARD const UIBoxSlot*
getVerticalSlot(const UIPanelSlot* slot) noexcept
{
  if (slot == nullptr || StringView(slot->getTypeName()) != "UIVerticalBoxSlot") {
    return nullptr;
  }
  return static_cast<const UIVerticalBoxSlot*>(slot);
}

} // namespace

/*
 */
UIWidget*
UIVerticalBox::addChild(UniquePtr<UIWidget> child)
{
  return UIPanelWidget::addChild(std::move(child), chMakeUnique<UIVerticalBoxSlot>());
}

/*
 */
Vector2
UIVerticalBox::getDesiredSize() const
{
  Vector2 size = Vector2::ZERO;
  for (SIZE_T index = 0; index < getChildCount(); ++index) {
    const UIWidget* child = getChildAt(index);
    if (child == nullptr || child->getVisibility() == UIVisibility::Collapsed) {
      continue;
    }

    const UIBoxSlot* box = getVerticalSlot(getSlotAt(index));
    const UIMargin padding = box ? box->getPadding() : UIMargin();
    const Vector2 desired = child->getDesiredSize();
    size.x = Math::max(size.x, desired.x + padding.getHorizontal());
    size.y += desired.y + padding.getVertical();
  }
  return size;
}

/*
 */
void
UIVerticalBox::arrangeChildren()
{
  const UIGeometry& parent = getGeometry();

  float totalAuto = 0.0f;
  float totalWeight = 0.0f;
  for (SIZE_T index = 0; index < getChildCount(); ++index) {
    const UIWidget* child = getChildAt(index);
    if (child == nullptr || child->getVisibility() == UIVisibility::Collapsed) {
      continue;
    }

    const UIBoxSlot* box = getVerticalSlot(getSlotAt(index));
    const UIMargin padding = box ? box->getPadding() : UIMargin();
    if (box == nullptr || box->getSizeRule() == UISizeRule::Auto) {
      totalAuto += child->getDesiredSize().y + padding.getVertical();
    }
    else {
      totalWeight += Math::max(0.0f, box->getFillWeight());
    }
  }

  const float freeHeight = Math::max(0.0f, parent.size.y - totalAuto);
  float cursor = parent.position.y;

  for (SIZE_T index = 0; index < getChildCount(); ++index) {
    UIWidget* child = getChildAt(index);
    if (child == nullptr) {
      continue;
    }

    const UIBoxSlot* box = getVerticalSlot(getSlotAt(index));
    const UIMargin padding = box ? box->getPadding() : UIMargin();

    if (child->getVisibility() == UIVisibility::Collapsed) {
      UIGeometry collapsed = parent;
      collapsed.position = {parent.position.x, cursor};
      collapsed.size = Vector2::ZERO;
      child->arrange(collapsed);
      continue;
    }

    const Vector2 desired = child->getDesiredSize();
    const bool bFill = box != nullptr && box->getSizeRule() == UISizeRule::Fill;
    float rowHeight = 0.0f;
    if (bFill) {
      const float weight = Math::max(0.0f, box->getFillWeight());
      rowHeight = totalWeight > 0.0f ? freeHeight * weight / totalWeight : 0.0f;
    }
    else {
      rowHeight = desired.y + padding.getVertical();
    }

    const float innerWidth = Math::max(0.0f, parent.size.x - padding.getHorizontal());
    UIHorizontalAlignment alignment = box ? box->getHorizontalAlignment()
                                          : UIHorizontalAlignment::Fill;
    float width = innerWidth;
    float x = parent.position.x + padding.left;
    if (alignment != UIHorizontalAlignment::Fill) {
      width = Math::min(innerWidth, Math::max(0.0f, desired.x));
      if (alignment == UIHorizontalAlignment::Center) {
        x += (innerWidth - width) * 0.5f;
      }
      else if (alignment == UIHorizontalAlignment::Right) {
        x += innerWidth - width;
      }
    }

    UIGeometry childGeometry = parent;
    childGeometry.position = {x, cursor + padding.top};
    childGeometry.size = {width, Math::max(0.0f, rowHeight - padding.getVertical())};
    child->arrange(childGeometry);

    cursor += rowHeight;
  }
}

} // namespace chEngineSDK
