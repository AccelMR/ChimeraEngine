/************************************************************************/
/**
 * @file chUIWidget.cpp
 * @author Swampertor
 * @date 2026/11/19
 * @brief Base of every widget in a UI document, as UMG's UWidget.
 */
/************************************************************************/
#include "chUIWidget.h"

#include "chMath.h"
#include "chUIUserWidget.h"
#include "chUIViewport.h"
#include "chUIWidgetTree.h"

namespace chEngineSDK {

UIWidget::~UIWidget() = default;

/*
 */
void
UIWidget::arrange(const UIGeometry& geometry)
{
  m_geometry = geometry;
  arrangeChildren();
}

/*
 */
void
UIWidget::arrangeChildren()
{
}

/*
 */
Vector2
UIWidget::getDesiredSize() const
{
  return m_geometry.size;
}

/*
 */
UIWidget*
UIWidget::getChildAt(SIZE_T index) const noexcept
{
  CH_PARAMETER_UNUSED(index);
  return nullptr;
}

/*
 */
UIUserWidget*
UIWidget::getUserWidget() const noexcept
{
  return m_tree ? m_tree->getUserWidget() : nullptr;
}

/*
 */
UIViewport*
UIWidget::getViewport() const noexcept
{
  UIUserWidget* userWidget = getUserWidget();
  return userWidget ? userWidget->getViewport() : nullptr;
}

/*
 */
Vector2
UIWidget::toLocalPoint(const Vector2& point) const noexcept
{
  const Vector2 size = m_geometry.size;
  const Vector2 pivot{size.x * m_renderTransform.pivot.x,
                      size.y * m_renderTransform.pivot.y};

  Vector2 local = point - m_geometry.position - m_renderTransform.translation;
  local -= pivot;

  if (m_renderTransform.angle != 0.0f) {
    // Inverse of the rotation applied when painting: rotate by -angle.
    float sinAngle = 0.0f;
    float cosAngle = 1.0f;
    Math::sinCos(-m_renderTransform.angle, sinAngle, cosAngle);
    local = {local.x * cosAngle - local.y * sinAngle,
             local.x * sinAngle + local.y * cosAngle};
  }

  if (m_renderTransform.scale.x == 0.0f || m_renderTransform.scale.y == 0.0f) {
    return {-1.0f, -1.0f};
  }
  local.x /= m_renderTransform.scale.x;
  local.y /= m_renderTransform.scale.y;

  return local + pivot;
}

/*
 */
bool
UIWidget::containsPoint(const Vector2& point) const noexcept
{
  if (m_geometry.size.x <= 0.0f || m_geometry.size.y <= 0.0f) {
    return false;
  }
  if (m_geometry.bClipped && !m_geometry.clipRect.contains(point)) {
    return false;
  }

  const Vector2 local = toLocalPoint(point);
  return local.x >= 0.0f && local.y >= 0.0f &&
         local.x <= m_geometry.size.x && local.y <= m_geometry.size.y;
}

/*
 */
UIWidget*
UIWidget::hitTest(const Vector2& point) const
{
  // Hidden and Collapsed take the whole subtree out of input.
  if (!isVisible()) {
    return nullptr;
  }
  // HitTestInvisible takes itself and its children out of input.
  if (m_visibility == UIVisibility::HitTestInvisible) {
    return nullptr;
  }

  // Children are painted in order, so the last one is on top and is tested first.
  for (SIZE_T index = getChildCount(); index > 0; --index) {
    if (UIWidget* child = getChildAt(index - 1)) {
      if (UIWidget* hit = child->hitTest(point)) {
        return hit;
      }
    }
  }

  // SelfHitTestInvisible lets the children be hit but not the widget itself.
  if (!isSelfHitTestVisible()) {
    return nullptr;
  }
  return containsPoint(point) ? const_cast<UIWidget*>(this) : nullptr;
}

/*
 */
void
UIWidget::onAddedToTree()
{
}

/*
 */
void
UIWidget::onRemovedFromTree()
{
}

/*
 */
void
UIWidget::onConstruct()
{
}

/*
 */
void
UIWidget::onDestruct()
{
}

/*
 */
void
UIWidget::onTick(float deltaTime)
{
  CH_PARAMETER_UNUSED(deltaTime);
}

/*
 */
void
UIWidget::paint(UIDrawList& drawList) const
{
  CH_PARAMETER_UNUSED(drawList);
}

/*
 */
void
UIWidget::onPointerEnter(const Vector2& localPosition)
{
  CH_PARAMETER_UNUSED(localPosition);
}

/*
 */
void
UIWidget::onPointerExit(const Vector2& localPosition)
{
  CH_PARAMETER_UNUSED(localPosition);
}

/*
 */
void
UIWidget::onPointerDown(const Vector2& localPosition)
{
  CH_PARAMETER_UNUSED(localPosition);
}

/*
 */
void
UIWidget::onPointerMove(const Vector2& localPosition)
{
  CH_PARAMETER_UNUSED(localPosition);
}

/*
 */
void
UIWidget::onPointerUp(const Vector2& localPosition)
{
  CH_PARAMETER_UNUSED(localPosition);
}

/*
 */
void
UIWidget::onPointerClick(const Vector2& localPosition)
{
  CH_PARAMETER_UNUSED(localPosition);
}

/*
 */
bool
UIWidget::onScroll(float delta)
{
  CH_PARAMETER_UNUSED(delta);
  return false;
}

/*
 */
void
UIWidget::onFocusReceived()
{
}

/*
 */
void
UIWidget::onFocusLost()
{
}

/*
 */
void
UIWidget::onSubmit()
{
}

/*
 */
void
UIWidget::onCancel()
{
}

/*
 */
bool
UIWidget::onNavigation(UINavigation direction)
{
  CH_PARAMETER_UNUSED(direction);
  return false;
}

/*
 */
void
UIWidget::onTextInput(StringView text)
{
  CH_PARAMETER_UNUSED(text);
}

/*
 */
bool
UIWidget::onTextEditingKey(InputKey key, bool bControl)
{
  CH_PARAMETER_UNUSED(key);
  CH_PARAMETER_UNUSED(bControl);
  return false;
}

/*
 */
void
UIWidget::attachToTree(UIWidgetTree* tree)
{
  m_tree = tree;
  onAddedToTree();

  const SIZE_T childCount = getChildCount();
  for (SIZE_T index = 0; index < childCount; ++index) {
    if (UIWidget* child = getChildAt(index)) {
      child->attachToTree(tree);
    }
  }
}

/*
 */
void
UIWidget::detachFromTree()
{
  const SIZE_T childCount = getChildCount();
  for (SIZE_T index = 0; index < childCount; ++index) {
    if (UIWidget* child = getChildAt(index)) {
      child->detachFromTree();
    }
  }

  onRemovedFromTree();
  m_tree = nullptr;
}

/*
 */
void
UIWidget::constructHierarchy()
{
  onConstruct();

  const SIZE_T childCount = getChildCount();
  for (SIZE_T index = 0; index < childCount; ++index) {
    if (UIWidget* child = getChildAt(index)) {
      child->constructHierarchy();
    }
  }
}

/*
 */
void
UIWidget::destructHierarchy()
{
  const SIZE_T childCount = getChildCount();
  for (SIZE_T index = 0; index < childCount; ++index) {
    if (UIWidget* child = getChildAt(index)) {
      child->destructHierarchy();
    }
  }

  onDestruct();
}

/*
 */
void
UIWidget::tickHierarchy(float deltaTime)
{
  onTick(deltaTime);

  const SIZE_T childCount = getChildCount();
  for (SIZE_T index = 0; index < childCount; ++index) {
    if (UIWidget* child = getChildAt(index)) {
      child->tickHierarchy(deltaTime);
    }
  }
}

/*
 */
void
UIWidget::paintHierarchy(UIDrawList& drawList) const
{
  if (!isVisible()) {
    return;
  }
  // A clipped widget that lies entirely outside its clip has nothing to show, and neither
  // do its children: skip the whole subtree. Partial overlap still paints until the
  // renderer learns scissor rectangles.
  if (m_geometry.bClipped && !m_geometry.toRect().overlaps(m_geometry.clipRect)) {
    return;
  }

  paint(drawList);

  const SIZE_T childCount = getChildCount();
  for (SIZE_T index = 0; index < childCount; ++index) {
    if (UIWidget* child = getChildAt(index)) {
      child->paintHierarchy(drawList);
    }
  }
}

/*
 */
SIZE_T
UIWidget::countWidgets() const noexcept
{
  SIZE_T count = 1;
  const SIZE_T childCount = getChildCount();
  for (SIZE_T index = 0; index < childCount; ++index) {
    if (UIWidget* child = getChildAt(index)) {
      count += child->countWidgets();
    }
  }
  return count;
}

} // namespace chEngineSDK
