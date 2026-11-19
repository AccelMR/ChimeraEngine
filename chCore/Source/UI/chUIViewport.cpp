/************************************************************************/
/**
 * @file chUIViewport.cpp
 * @author Swampertor
 * @date 2026/11/19
 * @brief Owns the user widgets on screen and routes focus between them.
 */
/************************************************************************/
#include "chUIViewport.h"

#include <cstddef>
#include <limits>

#include "chMath.h"
#include "chUIWidget.h"
#include "chUIWidgetTree.h"

namespace chEngineSDK {

namespace {

Vector2
getNavigationDirection(UINavigation direction)
{
  switch (direction) {
    case UINavigation::Up: return {0.0f, -1.0f};
    case UINavigation::Down: return {0.0f, 1.0f};
    case UINavigation::Left: return {-1.0f, 0.0f};
    case UINavigation::Right: return {1.0f, 0.0f};
    case UINavigation::Next:
    case UINavigation::Previous:
    case UINavigation::COUNT: break;
  }
  return Vector2::ZERO;
}

} // namespace

UIViewport::~UIViewport()
{
  clearUserWidgets();
}

/*
 */
UIUserWidget*
UIViewport::addUserWidget(UniquePtr<UIUserWidget> userWidget)
{
  if (!userWidget) {
    return nullptr;
  }

  UIUserWidget* rawUserWidget = userWidget.get();
  for (const UniquePtr<UIUserWidget>& existing : m_userWidgets) {
    if (existing.get() == rawUserWidget) {
      return nullptr;
    }
  }

  rawUserWidget->setViewport(this);
  m_userWidgets.push_back(std::move(userWidget));

  rawUserWidget->initialize();
  rawUserWidget->construct();
  return rawUserWidget;
}

/*
 */
bool
UIViewport::removeUserWidget(UIUserWidget* userWidget)
{
  if (userWidget == nullptr) {
    return false;
  }

  for (SIZE_T index = 0; index < m_userWidgets.size(); ++index) {
    if (m_userWidgets[index].get() != userWidget) {
      continue;
    }

    if (m_focused != nullptr && m_focused->getUserWidget() == userWidget) {
      setFocusedWidget(nullptr);
    }

    if (m_hovered != nullptr && m_hovered->getUserWidget() == userWidget) {
      dispatchPointerExit();
    }

    if (m_captured != nullptr && m_captured->getUserWidget() == userWidget) {
      m_captured = nullptr;
    }

    userWidget->destruct();
    userWidget->setViewport(nullptr);
    m_userWidgets.erase(m_userWidgets.begin() + static_cast<ptrdiff_t>(index));
    return true;
  }

  return false;
}

/*
 */
void
UIViewport::clearUserWidgets()
{
  setFocusedWidget(nullptr);
  dispatchPointerExit();
  m_captured = nullptr;

  for (UniquePtr<UIUserWidget>& userWidget : m_userWidgets) {
    if (userWidget) {
      userWidget->destruct();
      userWidget->setViewport(nullptr);
    }
  }

  m_userWidgets.clear();
}

/*
 */
UIUserWidget*
UIViewport::getUserWidgetAt(SIZE_T index) const noexcept
{
  return index < m_userWidgets.size() ? m_userWidgets[index].get() : nullptr;
}

/*
 */
void
UIViewport::setViewportSize(const Vector2& size)
{
  if (m_viewportSize == size) {
    return;
  }

  m_viewportSize = size;
  refreshLayout();
}

/*
 */
void
UIViewport::refreshLayout()
{
  for (UniquePtr<UIUserWidget>& userWidget : m_userWidgets) {
    if (userWidget) {
      userWidget->refreshLayout();
    }
  }
}

/*
 */
void
UIViewport::tick(float deltaTime)
{
  // A focused widget that was hidden or removed must not keep the keyboard.
  if (m_focused != nullptr &&
      (!ownsWidget(m_focused) || !m_focused->isEnabled() || !m_focused->isVisible())) {
    setFocusedWidget(nullptr);
  }

  // The same goes for the widget under the pointer and the one holding the capture.
  if (m_hovered != nullptr &&
      (!ownsWidget(m_hovered) || !m_hovered->isEnabled() || !m_hovered->isVisible())) {
    dispatchPointerExit();
  }

  if (m_captured != nullptr &&
      (!ownsWidget(m_captured) || !m_captured->isEnabled() || !m_captured->isVisible())) {
    m_captured = nullptr;
  }

  for (UniquePtr<UIUserWidget>& userWidget : m_userWidgets) {
    if (userWidget) {
      userWidget->tick(deltaTime);
    }
  }
}

/*
 */
void
UIViewport::paint(UIDrawList& drawList) const
{
  for (const UniquePtr<UIUserWidget>& userWidget : m_userWidgets) {
    if (userWidget && userWidget->getWidgetTree()) {
      userWidget->getWidgetTree()->paint(drawList);
    }
  }
}

/*
 */
void
UIViewport::processInput(const InputState& state)
{
  processPointer(state.getMousePosition(),
                 state.isDown(InputKey::MouseLeft),
                 state.wasPressed(InputKey::MouseLeft),
                 state.wasReleased(InputKey::MouseLeft),
                 state.getMouseWheel());
}

/*
 */
void
UIViewport::processInput(const InputManager& input)
{
  processInput(input.getState());

  ActionState actions;
  actions.bSubmit = input.wasActionPressed(m_actionBindings.submit);
  actions.bCancel = input.wasActionPressed(m_actionBindings.cancel);
  actions.bNavigateUp = input.wasActionPressed(m_actionBindings.navigateUp);
  actions.bNavigateDown = input.wasActionPressed(m_actionBindings.navigateDown);
  actions.bNavigateLeft = input.wasActionPressed(m_actionBindings.navigateLeft);
  actions.bNavigateRight = input.wasActionPressed(m_actionBindings.navigateRight);
  actions.bNext = input.wasActionPressed(m_actionBindings.next);
  actions.bPrevious = input.wasActionPressed(m_actionBindings.previous);

  processActions(actions);
}

/*
 */
void
UIViewport::processPointer(const Vector2& position,
                           bool bDown,
                           bool bPressed,
                           bool bReleased,
                           const Vector2& wheel)
{
  m_pointerPosition = position;
  UIWidget* hit = hitTest(position);

  // Hover follows the topmost widget under the pointer; enter and exit only fire on a
  // change, so a widget does not get the same event every frame.
  if (hit != m_hovered) {
    dispatchPointerExit();
    m_hovered = hit;
    if (m_hovered != nullptr) {
      m_hovered->onPointerEnter(m_hovered->toLocalPoint(position));
    }
  }

  if (bPressed && bDown) {
    // The widget that takes the press keeps the pointer until the release, even when the
    // pointer leaves it. A click also gives keyboard focus to the control it lands on.
    m_captured = hit;
    if (m_captured != nullptr) {
      focusForPointer(m_captured);
      m_captured->onPointerDown(m_captured->toLocalPoint(position));
    }
  }

  if (bReleased) {
    UIWidget* captured = m_captured;
    m_captured = nullptr;
    if (captured != nullptr) {
      captured->onPointerUp(captured->toLocalPoint(position));
      // A release outside the widget cancels the click, as UMG does.
      if (captured->containsPoint(position)) {
        captured->onPointerClick(captured->toLocalPoint(position));
      }
    }
  }

  if (hit != nullptr && wheel != Vector2::ZERO) {
    // The wheel bubbles from the widget under the pointer to its ancestors, so a scroll
    // box still sees it when a label or a button is on top.
    const float delta = wheel.y != 0.0f ? wheel.y : wheel.x;
    for (UIWidget* widget = hit; widget != nullptr; widget = widget->getParent()) {
      widget->onScroll(delta);
    }
  }
}

/*
 */
void
UIViewport::processActions(const ActionState& actions)
{
  if (actions.bSubmit && m_focused != nullptr) {
    m_focused->onSubmit();
  }

  if (actions.bCancel && m_focused != nullptr) {
    m_focused->onCancel();
  }

  // One navigation step per frame; a direction wins over Next/Previous.
  if (actions.bNavigateUp) {
    moveFocus(UINavigation::Up);
  }
  else if (actions.bNavigateDown) {
    moveFocus(UINavigation::Down);
  }
  else if (actions.bNavigateLeft) {
    moveFocus(UINavigation::Left);
  }
  else if (actions.bNavigateRight) {
    moveFocus(UINavigation::Right);
  }
  else if (actions.bNext) {
    moveFocus(UINavigation::Next);
  }
  else if (actions.bPrevious) {
    moveFocus(UINavigation::Previous);
  }
}

/*
 */
UIWidget*
UIViewport::hitTest(const Vector2& point) const
{
  // Later documents are drawn on top, so they are tested first.
  for (SIZE_T index = m_userWidgets.size(); index > 0; --index) {
    const UniquePtr<UIUserWidget>& userWidget = m_userWidgets[index - 1];
    if (!userWidget) {
      continue;
    }

    if (UIWidget* root = userWidget->getRootWidget()) {
      if (UIWidget* hit = root->hitTest(point)) {
        return hit;
      }
    }
  }

  return nullptr;
}

/*
 */
UIWidget*
UIViewport::findWidgetByName(StringView name) const
{
  for (const UniquePtr<UIUserWidget>& userWidget : m_userWidgets) {
    if (userWidget) {
      if (UIWidget* found = userWidget->findWidgetByName(name)) {
        return found;
      }
    }
  }
  return nullptr;
}

/*
 */
bool
UIViewport::ownsWidget(const UIWidget* widget) const noexcept
{
  if (widget == nullptr) {
    return false;
  }

  UIUserWidget* userWidget = widget->getUserWidget();
  return userWidget != nullptr && userWidget->getViewport() == this;
}

/*
 */
bool
UIViewport::setFocusedWidget(UIWidget* widget)
{
  if (widget == m_focused) {
    return true;
  }

  if (widget != nullptr && (!ownsWidget(widget) || !widget->canFocus())) {
    return false;
  }

  if (m_focused != nullptr) {
    m_focused->m_bFocused = false;
    m_focused->onFocusLost();
  }

  m_focused = widget;

  if (m_focused != nullptr) {
    m_focused->m_bFocused = true;
    m_focused->onFocusReceived();
  }

  return true;
}

/*
 */
bool
UIViewport::moveFocus(UINavigation direction)
{
  if (m_focused == nullptr) {
    return focusFirst();
  }

  // A text editor owns the arrow keys while it has focus.
  if (m_focused->isTextEditor()) {
    return false;
  }

  if (direction == UINavigation::Next || direction == UINavigation::Previous) {
    Vector<UIWidget*> focusables;
    collectAllFocusable(focusables);
    if (focusables.empty()) {
      return false;
    }

    SIZE_T current = 0;
    bool found = false;
    for (SIZE_T index = 0; index < focusables.size(); ++index) {
      if (focusables[index] == m_focused) {
        current = index;
        found = true;
        break;
      }
    }

    SIZE_T next = 0;
    if (found) {
      if (direction == UINavigation::Next) {
        next = (current + 1) % focusables.size();
      }
      else {
        next = (current + focusables.size() - 1) % focusables.size();
      }
    }

    return setFocusedWidget(focusables[next]);
  }

  const UINavigationLink& link = m_focused->getNavigation(direction);
  switch (link.rule) {
    case UINavigationRule::Stop:
      return false;

    case UINavigationRule::Explicit:
      return link.target != nullptr && setFocusedWidget(link.target);

    case UINavigationRule::Custom:
      if (link.custom) {
        UIWidget* target = link.custom();
        return target != nullptr && setFocusedWidget(target);
      }
      return false;

    case UINavigationRule::Escape: {
      UIWidget* target = findFocusableInDirection(direction, false);
      return target != nullptr && setFocusedWidget(target);
    }

    case UINavigationRule::Wrap: {
      UIWidget* target = findFocusableInDirection(direction, true);
      return target != nullptr && setFocusedWidget(target);
    }

    case UINavigationRule::COUNT:
      break;
  }

  return false;
}

/*
 */
bool
UIViewport::focusFirst()
{
  Vector<UIWidget*> focusables;
  collectAllFocusable(focusables);

  for (UIWidget* widget : focusables) {
    if (setFocusedWidget(widget)) {
      return true;
    }
  }
  return false;
}

/*
 */
void
UIViewport::collectFocusable(UIWidget* widget, Vector<UIWidget*>& out) const
{
  if (widget == nullptr || !widget->isEnabled() || !widget->isVisible()) {
    return;
  }

  if (widget->canFocus()) {
    out.push_back(widget);
  }

  const SIZE_T childCount = widget->getChildCount();
  for (SIZE_T index = 0; index < childCount; ++index) {
    collectFocusable(widget->getChildAt(index), out);
  }
}

/*
 */
void
UIViewport::collectAllFocusable(Vector<UIWidget*>& out) const
{
  for (const UniquePtr<UIUserWidget>& userWidget : m_userWidgets) {
    if (userWidget) {
      collectFocusable(userWidget->getRootWidget(), out);
    }
  }
}

/*
 */
void
UIViewport::focusForPointer(UIWidget* hit)
{
  // A click on a label inside a button focuses the button, the first control above it.
  for (UIWidget* widget = hit; widget != nullptr; widget = widget->getParent()) {
    if (widget->canFocus()) {
      setFocusedWidget(widget);
      return;
    }
  }
}

/*
 */
void
UIViewport::dispatchPointerExit()
{
  if (m_hovered == nullptr) {
    return;
  }

  m_hovered->onPointerExit(m_hovered->toLocalPoint(m_pointerPosition));
  m_hovered = nullptr;
}

/*
 */
UIWidget*
UIViewport::findFocusableInDirection(UINavigation direction, bool wrap) const
{
  if (m_focused == nullptr) {
    return nullptr;
  }

  const Vector2 navDirection = getNavigationDirection(direction);
  if (navDirection == Vector2::ZERO) {
    return nullptr;
  }

  Vector<UIWidget*> focusables;
  collectAllFocusable(focusables);

  const Vector2 origin = m_focused->getGeometry().getCenter();

  UIWidget* best = nullptr;
  float bestScore = -std::numeric_limits<float>::max();

  for (UIWidget* candidate : focusables) {
    if (candidate == m_focused) {
      continue;
    }

    const Vector2 offset = candidate->getGeometry().getCenter() - origin;
    const float dot = offset.dot(navDirection);
    if (dot <= 0.0f) {
      continue;
    }

    const float length = offset.magnitude();
    if (length < Math::SMALL_NUMBER) {
      continue;
    }

    // Favour alignment with the direction over distance, as the SFMX manager did.
    const float angleScore = dot / length;
    const float distanceScore = 1.0f / (1.0f + length);
    const float score = angleScore * 3.0f + distanceScore;

    if (score > bestScore) {
      bestScore = score;
      best = candidate;
    }
  }

  if (best != nullptr || !wrap) {
    return best;
  }

  // Wrap: nothing ahead, so take the candidate farthest behind the direction.
  float bestDot = std::numeric_limits<float>::max();
  for (UIWidget* candidate : focusables) {
    if (candidate == m_focused) {
      continue;
    }

    const Vector2 offset = candidate->getGeometry().getCenter() - origin;
    const float dot = offset.dot(navDirection);
    if (dot < bestDot) {
      bestDot = dot;
      best = candidate;
    }
  }

  return best;
}

} // namespace chEngineSDK
