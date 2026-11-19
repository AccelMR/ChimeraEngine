/************************************************************************/
/**
 * @file chUIViewport.h
 * @author Swampertor
 * @date 2026/11/19
 * @brief Owns the user widgets on screen and routes focus between them.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chUIUserWidget.h"
#include "chUITypes.h"

namespace chEngineSDK {
class UIDrawList;
class UIWidget;

/**
 * The runtime UI root, replacing SFMX's UIManager singleton. It owns every UIUserWidget
 * added to the screen, gives them the viewport rectangle, ticks and paints them, and
 * decides which widget has keyboard focus.
 *
 * User widgets are drawn and hit-tested in the order they were added: later ones are on
 * top, and hit-testing walks them backwards.
 */
class CH_CORE_EXPORT UIViewport
{
 public:
  UIViewport() = default;
  ~UIViewport();

  UIViewport(const UIViewport&) = delete;
  UIViewport&
  operator=(const UIViewport&) = delete;
  UIViewport(UIViewport&&) = delete;
  UIViewport&
  operator=(UIViewport&&) = delete;

  // -- User widgets -----------------------------------------------------------

  /**
   * Takes ownership of a user widget, initializes it, lays it out and constructs it.
   * Returns the widget, or nullptr when it is null or already added.
   */
  UIUserWidget*
  addUserWidget(UniquePtr<UIUserWidget> userWidget);

  /**
   * Destructs a user widget and destroys it. Clears focus first when it belongs to the
   * removed document.
   */
  bool
  removeUserWidget(UIUserWidget* userWidget);

  void
  clearUserWidgets();

  NODISCARD FORCEINLINE const Vector<UniquePtr<UIUserWidget>>&
  getUserWidgets() const noexcept
  {
    return m_userWidgets;
  }

  NODISCARD FORCEINLINE SIZE_T
  getUserWidgetCount() const noexcept
  {
    return m_userWidgets.size();
  }

  NODISCARD UIUserWidget*
  getUserWidgetAt(SIZE_T index) const noexcept;

  // -- Layout -----------------------------------------------------------------

  /**
   * The rectangle every user widget is arranged into, in pixels from the top-left of the
   * screen. Changing it re-arranges every document.
   */
  void
  setViewportSize(const Vector2& size);

  NODISCARD FORCEINLINE const Vector2&
  getViewportSize() const noexcept
  {
    return m_viewportSize;
  }

  void
  refreshLayout();

  // -- Frame ------------------------------------------------------------------

  /**
   * Validates the focus and ticks every user widget.
   */
  void
  tick(float deltaTime);

  /**
   * Paints every user widget in add order. Implemented with the renderer in point 3.
   */
  void
  paint(UIDrawList& drawList) const;

  // -- Queries ----------------------------------------------------------------

  /**
   * Topmost widget under point, in viewport space. Walks user widgets last to first.
   */
  NODISCARD UIWidget*
  hitTest(const Vector2& point) const;

  /**
   * First widget with this name in any document, or nullptr.
   */
  NODISCARD UIWidget*
  findWidgetByName(StringView name) const;

  /**
   * True when widget belongs to a document added to this viewport.
   */
  NODISCARD bool
  ownsWidget(const UIWidget* widget) const noexcept;

  // -- Focus ------------------------------------------------------------------

  /**
   * Moves focus to widget, firing onFocusLost and onFocusReceived. Pass nullptr to clear
   * focus. Returns false when the widget is not in this viewport or cannot be focused.
   */
  bool
  setFocusedWidget(UIWidget* widget);

  NODISCARD FORCEINLINE UIWidget*
  getFocusedWidget() const noexcept
  {
    return m_focused;
  }

  /**
   * Moves focus one step in a direction, following the focused widget's navigation link
   * and falling back to the nearest focusable widget. Next and Previous walk the document
   * order. Returns false when focus did not move.
   */
  bool
  moveFocus(UINavigation direction);

  /**
   * Focuses the first focusable widget in document order. Returns false when there is none.
   */
  bool
  focusFirst();

 private:
  NODISCARD UIWidget*
  findFocusableInDirection(UINavigation direction, bool wrap) const;

  void
  collectFocusable(UIWidget* widget, Vector<UIWidget*>& out) const;

  void
  collectAllFocusable(Vector<UIWidget*>& out) const;

  Vector<UniquePtr<UIUserWidget>> m_userWidgets;
  Vector2 m_viewportSize = Vector2::ZERO;
  UIWidget* m_focused = nullptr;
};

} // namespace chEngineSDK
