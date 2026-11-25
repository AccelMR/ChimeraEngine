/************************************************************************/
/**
 * @file chUIHorizontalBox.cpp
 * @author Swampertor
 * @date 2026/11/24
 * @brief Panel that places its children left to right, as UMG's UHorizontalBox.
 */
/************************************************************************/
#include "chUIHorizontalBox.h"

#include "chMath.h"

namespace chEngineSDK {

namespace {

NODISCARD const UIBoxSlot*
getHorizontalSlot(const UIPanelSlot* slot) noexcept
{
  if (slot == nullptr || StringView(slot->getTypeName()) != "UIHorizontalBoxSlot") {
    return nullptr;
  }
  return static_cast<const UIHorizontalBoxSlot*>(slot);
}

} // namespace

/*
 */
UIWidget*
UIHorizontalBox::addChild(UniquePtr<UIWidget> child)
{
  return UIPanelWidget::addChild(std::move(child), chMakeUnique<UIHorizontalBoxSlot>());
}

/*
 */
Vector2
UIHorizontalBox::getDesiredSize() const
{
  Vector2 size = Vector2::ZERO;
  for (SIZE_T index = 0; index < getChildCount(); ++index) {
    const UIWidget* child = getChildAt(index);
    if (child == nullptr || child->getVisibility() == UIVisibility::Collapsed) {
      continue;
    }

    const UIBoxSlot* box = getHorizontalSlot(getSlotAt(index));
    const UIMargin padding = box ? box->getPadding() : UIMargin();
    const Vector2 desired = child->getDesiredSize();
    size.x += desired.x + padding.getHorizontal();
    size.y = Math::max(size.y, desired.y + padding.getVertical());
  }
  return size;
}

/*
 */
void
UIHorizontalBox::arrangeChildren()
{
  const UIGeometry& parent = getGeometry();

  float totalAuto = 0.0f;
  float totalWeight = 0.0f;
  for (SIZE_T index = 0; index < getChildCount(); ++index) {
    const UIWidget* child = getChildAt(index);
    if (child == nullptr || child->getVisibility() == UIVisibility::Collapsed) {
      continue;
    }

    const UIBoxSlot* box = getHorizontalSlot(getSlotAt(index));
    const UIMargin padding = box ? box->getPadding() : UIMargin();
    if (box == nullptr || box->getSizeRule() == UISizeRule::Auto) {
      totalAuto += child->getDesiredSize().x + padding.getHorizontal();
    }
    else {
      totalWeight += Math::max(0.0f, box->getFillWeight());
    }
  }

  const float freeWidth = Math::max(0.0f, parent.size.x - totalAuto);
  float cursor = parent.position.x;

  for (SIZE_T index = 0; index < getChildCount(); ++index) {
    UIWidget* child = getChildAt(index);
    if (child == nullptr) {
      continue;
    }

    const UIBoxSlot* box = getHorizontalSlot(getSlotAt(index));
    const UIMargin padding = box ? box->getPadding() : UIMargin();

    if (child->getVisibility() == UIVisibility::Collapsed) {
      UIGeometry collapsed = parent;
      collapsed.position = {cursor, parent.position.y};
      collapsed.size = Vector2::ZERO;
      child->arrange(collapsed);
      continue;
    }

    const Vector2 desired = child->getDesiredSize();
    const bool bFill = box != nullptr && box->getSizeRule() == UISizeRule::Fill;
    float columnWidth = 0.0f;
    if (bFill) {
      const float weight = Math::max(0.0f, box->getFillWeight());
      columnWidth = totalWeight > 0.0f ? freeWidth * weight / totalWeight : 0.0f;
    }
    else {
      columnWidth = desired.x + padding.getHorizontal();
    }

    const float innerHeight = Math::max(0.0f, parent.size.y - padding.getVertical());
    UIVerticalAlignment alignment = box ? box->getVerticalAlignment()
                                        : UIVerticalAlignment::Fill;
    float height = innerHeight;
    float y = parent.position.y + padding.top;
    if (alignment != UIVerticalAlignment::Fill) {
      height = Math::min(innerHeight, Math::max(0.0f, desired.y));
      if (alignment == UIVerticalAlignment::Center) {
        y += (innerHeight - height) * 0.5f;
      }
      else if (alignment == UIVerticalAlignment::Bottom) {
        y += innerHeight - height;
      }
    }

    UIGeometry childGeometry = parent;
    childGeometry.position = {cursor + padding.left, y};
    childGeometry.size = {Math::max(0.0f, columnWidth - padding.getHorizontal()), height};
    child->arrange(childGeometry);

    cursor += columnWidth;
  }
}

} // namespace chEngineSDK
