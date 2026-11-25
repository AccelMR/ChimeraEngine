/************************************************************************/
/**
 * @file chUIOverlay.cpp
 * @author Swampertor
 * @date 2026/11/24
 * @brief Panel that stacks its children on one rectangle, as UMG's UOverlay.
 */
/************************************************************************/
#include "chUIOverlay.h"

#include "chMath.h"

namespace chEngineSDK {

namespace {

NODISCARD const UIOverlaySlot*
getOverlaySlot(const UIPanelSlot* slot) noexcept
{
  if (slot == nullptr || StringView(slot->getTypeName()) != "UIOverlaySlot") {
    return nullptr;
  }
  return static_cast<const UIOverlaySlot*>(slot);
}

} // namespace

/*
 */
UIWidget*
UIOverlay::addChild(UniquePtr<UIWidget> child)
{
  return UIPanelWidget::addChild(std::move(child), chMakeUnique<UIOverlaySlot>());
}

/*
 */
Vector2
UIOverlay::getDesiredSize() const
{
  Vector2 size = Vector2::ZERO;
  for (SIZE_T index = 0; index < getChildCount(); ++index) {
    const UIWidget* child = getChildAt(index);
    if (child == nullptr || child->getVisibility() == UIVisibility::Collapsed) {
      continue;
    }

    const UIOverlaySlot* slot = getOverlaySlot(getSlotAt(index));
    const UIMargin padding = slot ? slot->getPadding() : UIMargin();
    const Vector2 desired = child->getDesiredSize();
    size.x = Math::max(size.x, desired.x + padding.getHorizontal());
    size.y = Math::max(size.y, desired.y + padding.getVertical());
  }
  return size;
}

} // namespace chEngineSDK
