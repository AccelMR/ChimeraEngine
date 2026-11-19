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
