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

#include "chInputManager.h"
#include "chUIUserWidget.h"
#include "chUITypes.h"

namespace chEngineSDK {
class UIDrawList;
class UIWidget;

/**
 * The runtime UI root, replacing SFMX's UIManager singleton. It owns every UIUserWidget
 * added to the screen, gives them the viewport rectangle, ticks and paints them, decides
 * which widget has keyboard focus and routes pointer and keyboard/gamepad input to them.
 *
 * User widgets are drawn and hit-tested in the order they were added: later ones are on
 * top, and hit-testing walks them backwards.
 */
class CH_CORE_EXPORT UIViewport
{
 public:
  /**
   * Actions the viewport reads for keyboard and gamepad control. A project binds them in
   * Input.ini and gives the ids to the viewport; leaving one at INVALID_ACTION disables it.
   */
  struct ActionBindings {
    InputActionId submit = InputManager::INVALID_ACTION;
    InputActionId cancel = InputManager::INVALID_ACTION;
    InputActionId navigateUp = InputManager::INVALID_ACTION;
    InputActionId navigateDown = InputManager::INVALID_ACTION;
    InputActionId navigateLeft = InputManager::INVALID_ACTION;
    InputActionId navigateRight = InputManager::INVALID_ACTION;
    InputActionId next = InputManager::INVALID_ACTION;
    InputActionId previous = InputManager::INVALID_ACTION;
  };

  /**
   * What the UI actions read this frame. processInput fills it from InputManager; tests
   * and custom backends can build it by hand.
   */
  struct ActionState {
    bool bSubmit = false;
    bool bCancel = false;
    bool bNavigateUp = false;
    bool bNavigateDown = false;
    bool bNavigateLeft = false;
    bool bNavigateRight = false;
    bool bNext = false;
    bool bPrevious = false;
  };

  /**
   * What one input call delivered to the UI. The game layer can use it to keep gameplay
   * from reacting to input the UI already owns, for example a click that focused a button
   * or a wheel step a scroll box consumed.
   */
  struct InputResult {
    // The pointer is over a widget or a document this frame.
    bool bPointerOverUI = false;
    // A widget is holding the pointer after a press, even if the pointer has left it.
    bool bPointerCaptureActive = false;
    // A press and release landed on the same widget and fired onPointerClick.
    bool bPointerClicked = false;
    // A wheel step was consumed by the widget under the pointer or one of its ancestors.
    bool bWheelHandled = false;
    // Submit/cancel were delivered to the focused widget.
    bool bSubmitDelivered = false;
    bool bCancelDelivered = false;
    // A navigation action actually moved focus.
    bool bNavigationMoved = false;
    // The focused widget edits text, so gameplay should not read the keyboard this frame.
    bool bKeyboardCaptured = false;

    NODISCARD FORCEINLINE bool
    isPointerHandled() const noexcept
    {
      return bPointerOverUI || bPointerCaptureActive || bPointerClicked;
    }
  };

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

  // -- Input ------------------------------------------------------------------

  /**
   * Reads the mouse position, the left button and the wheel from state and routes them:
   * hover enter/exit, press, release and click. Keyboard/gamepad actions are not read;
   * call the InputManager overload for those.
   *
   * @return What the pointer and focus did this frame.
   */
  InputResult
  processInput(const InputState& state);

  /**
   * Processes the pointer as above and reads the configured actions from input, firing
   * submit, cancel and focus navigation. Call it once per frame after the input actions
   * are updated. A frame loop that has to own the input before the game reads it can call
   * processPointer earlier and processActions(const InputManager&) after updateActions.
   *
   * @return The combined pointer, wheel and action result.
   */
  InputResult
  processInput(const InputManager& input);

  /**
   * Routes one frame of pointer state directly, for a frame loop that processes the
   * pointer before the actions are evaluated, and for tests and custom backends. The wheel
   * is in steps, y away from the user.
   *
   * @return What the pointer and wheel did this frame.
   */
  InputResult
  processPointer(const Vector2& position,
                 bool bDown,
                 bool bPressed,
                 bool bReleased,
                 const Vector2& wheel);

  /**
   * Fires submit/cancel on the focused widget and moves focus for the navigation actions.
   * At most one direction moves focus per call, in Up, Down, Left, Right, Next, Previous
   * order.
   *
   * @return Which actions were delivered and whether focus moved.
   */
  InputResult
  processActions(const ActionState& actions);

  /**
   * Reads the configured actions from input and processes them as above. Call it after
   * updateActions, after the pointer was already processed this frame.
   *
   * @return Which actions were delivered and whether focus moved.
   */
  InputResult
  processActions(const InputManager& input);

  FORCEINLINE void
  setActionBindings(const ActionBindings& bindings) noexcept
  {
    m_actionBindings = bindings;
  }

  NODISCARD FORCEINLINE const ActionBindings&
  getActionBindings() const noexcept
  {
    return m_actionBindings;
  }

  /**
   * Widget the pointer is over, or nullptr. Driven by processPointer.
   */
  NODISCARD FORCEINLINE UIWidget*
  getHoveredWidget() const noexcept
  {
    return m_hovered;
  }

  /**
   * Widget that received the pointer press and will receive the release and click, or
   * nullptr when no button is down.
   */
  NODISCARD FORCEINLINE UIWidget*
  getCapturedWidget() const noexcept
  {
    return m_captured;
  }

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

  /**
   * Moves focus to the first focusable widget at or above hit, as a click does.
   */
  void
  focusForPointer(UIWidget* hit);

  void
  dispatchPointerExit();

  Vector<UniquePtr<UIUserWidget>> m_userWidgets;
  Vector2 m_viewportSize = Vector2::ZERO;
  Vector2 m_pointerPosition = Vector2::ZERO;
  UIWidget* m_focused = nullptr;
  UIWidget* m_hovered = nullptr;
  UIWidget* m_captured = nullptr;
  ActionBindings m_actionBindings;
};

} // namespace chEngineSDK
