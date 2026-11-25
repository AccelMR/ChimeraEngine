/************************************************************************/
/**
 * @file chUIScrollBox.cpp
 * @author Swampertor
 * @date 2026/11/24
 * @brief Panel that stacks its children and scrolls over them, as UMG's UScrollBox.
 */
/************************************************************************/
#include "chUIScrollBox.h"

#include "chMath.h"

namespace chEngineSDK {

namespace {

NODISCARD const UIScrollBoxSlot*
getScrollBoxSlot(const UIPanelSlot* slot) noexcept
{
  if (slot == nullptr || StringView(slot->getTypeName()) != "UIScrollBoxSlot") {
    return nullptr;
  }
  return static_cast<const UIScrollBoxSlot*>(slot);
}

} // namespace

UIScrollBox::UIScrollBox() = default;

UIScrollBox::~UIScrollBox() = default;

/*
 */
UIWidget*
UIScrollBox::addChild(UniquePtr<UIWidget> child)
{
  return UIPanelWidget::addChild(std::move(child), chMakeUnique<UIScrollBoxSlot>());
}

/*
 */
Vector2
UIScrollBox::getDesiredSize() const
{
  Vector2 size = Vector2::ZERO;
  for (SIZE_T index = 0; index < getChildCount(); ++index) {
    const UIWidget* child = getChildAt(index);
    if (child == nullptr || child->getVisibility() == UIVisibility::Collapsed) {
      continue;
    }

    const UIScrollBoxSlot* slot = getScrollBoxSlot(getSlotAt(index));
    const UIMargin padding = slot ? slot->getPadding() : UIMargin();
    const Vector2 desired = child->getDesiredSize();
    size.x = Math::max(size.x, desired.x + padding.getHorizontal());
    size.y += desired.y + padding.getVertical();
  }
  return size;
}

/*
 */
void
UIScrollBox::setScrollOffset(float offset)
{
  m_scrollOffset = Math::clamp(offset, 0.0f, m_maxScrollOffset);
}

/*
 */
bool
UIScrollBox::onScroll(float delta)
{
  if (delta == 0.0f || m_maxScrollOffset <= 0.0f) {
    return false;
  }

  // A notch away from the user shows earlier content, so the offset moves the other way.
  setScrollOffset(m_scrollOffset - delta * m_scrollStep);
  return true;
}

/*
 */
void
UIScrollBox::updateScrollRange(float viewHeight)
{
  m_maxScrollOffset = Math::max(0.0f, m_contentHeight - viewHeight);
  m_scrollOffset = Math::clamp(m_scrollOffset, 0.0f, m_maxScrollOffset);
}

/*
 */
void
UIScrollBox::arrangeChildren()
{
  const UIGeometry& parent = getGeometry();

  // The view is the box's own rectangle, narrowed by a clip inherited from an ancestor
  // scroll box or canvas. Children carry it so hit-testing and painting can skip the parts
  // that scroll out of sight.
  UIRect viewClip = parent.toRect();
  if (parent.bClipped) {
    viewClip = viewClip.intersection(parent.clipRect);
  }

  m_contentHeight = 0.0f;
  for (SIZE_T index = 0; index < getChildCount(); ++index) {
    const UIWidget* child = getChildAt(index);
    if (child == nullptr || child->getVisibility() == UIVisibility::Collapsed) {
      continue;
    }

    const UIScrollBoxSlot* slot = getScrollBoxSlot(getSlotAt(index));
    const UIMargin padding = slot ? slot->getPadding() : UIMargin();
    m_contentHeight += child->getDesiredSize().y + padding.getVertical();
  }

  updateScrollRange(parent.size.y);

  float cursor = parent.position.y - m_scrollOffset;
  for (SIZE_T index = 0; index < getChildCount(); ++index) {
    UIWidget* child = getChildAt(index);
    if (child == nullptr) {
      continue;
    }

    const UIScrollBoxSlot* slot = getScrollBoxSlot(getSlotAt(index));
    const UIMargin padding = slot ? slot->getPadding() : UIMargin();

    if (child->getVisibility() == UIVisibility::Collapsed) {
      UIGeometry collapsed = parent;
      collapsed.position = {parent.position.x, cursor};
      collapsed.size = Vector2::ZERO;
      collapsed.bClipped = true;
      collapsed.clipRect = viewClip;
      child->arrange(collapsed);
      continue;
    }

    const Vector2 desired = child->getDesiredSize();
    const float rowHeight = desired.y + padding.getVertical();

    const float innerWidth = Math::max(0.0f, parent.size.x - padding.getHorizontal());
    UIHorizontalAlignment alignment = slot ? slot->getHorizontalAlignment()
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
    childGeometry.size = {width, Math::max(0.0f, desired.y)};
    childGeometry.bClipped = true;
    childGeometry.clipRect = viewClip;
    child->arrange(childGeometry);

    cursor += rowHeight;
  }
}

} // namespace chEngineSDK
