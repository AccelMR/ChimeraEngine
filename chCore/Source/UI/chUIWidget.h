/************************************************************************/
/**
 * @file chUIWidget.h
 * @author Swampertor
 * @date 2026/11/19
 * @brief Base of every widget in a UI document, as UMG's UWidget.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chLinearColor.h"
#include "chUIRect.h"
#include "chUITypes.h"
#include "chVector2.h"

namespace chEngineSDK {
class UIDrawList;
class UIWidgetTree;
class UIUserWidget;
class UIViewport;
class UIPanelWidget;
class UIPanelSlot;
class UIContentWidget;

/**
 * One element of a UI document. A widget knows where its parent's slot arranged it, which
 * tree owns it, and how it reacts to the pointer and to focus; what it looks like and how
 * it lays out children is left to its subclasses.
 *
 * Widgets are owned by their panel (or by the widget tree for the root), never deleted by
 * hand. Parent, slot, focus and navigation references are non-owning.
 *
 * UI space starts at the top-left of the viewport with +X right and +Y down. Geometry is
 * absolute: a panel moves its children's rectangles while arranging them, so hit-testing
 * and painting can use one coordinate space from the root down.
 */
class CH_CORE_EXPORT UIWidget
{
 public:
  UIWidget() = default;
  virtual ~UIWidget();

  UIWidget(const UIWidget&) = delete;
  UIWidget&
  operator=(const UIWidget&) = delete;
  UIWidget(UIWidget&&) = delete;
  UIWidget&
  operator=(UIWidget&&) = delete;

  // -- Identity ---------------------------------------------------------------

  /**
   * Name of the concrete class, used by the UI document registry to rebuild a widget.
   */
  NODISCARD virtual const ANSICHAR*
  getTypeName() const noexcept
  {
    return "UIWidget";
  }

  /**
   * Lookup name inside the document; UI.get(name) in scripting searches for it.
   */
  NODISCARD FORCEINLINE const String&
  getName() const noexcept
  {
    return m_name;
  }

  FORCEINLINE void
  setName(StringView name)
  {
    m_name = name;
  }

  // -- State ------------------------------------------------------------------

  /**
   * Whether the widget and its input callbacks are processed. A disabled widget still
   * takes space and blocks widgets drawn below it.
   */
  NODISCARD FORCEINLINE bool
  isEnabled() const noexcept
  {
    return m_bEnabled;
  }

  FORCEINLINE void
  setEnabled(bool enabled) noexcept
  {
    m_bEnabled = enabled;
  }

  NODISCARD FORCEINLINE UIVisibility
  getVisibility() const noexcept
  {
    return m_visibility;
  }

  FORCEINLINE void
  setVisibility(UIVisibility visibility) noexcept
  {
    m_visibility = visibility;
  }

  /**
   * True when the widget is painted: Visible, HitTestInvisible or SelfHitTestInvisible.
   */
  NODISCARD FORCEINLINE bool
  isVisible() const noexcept
  {
    return m_visibility == UIVisibility::Visible ||
           m_visibility == UIVisibility::HitTestInvisible ||
           m_visibility == UIVisibility::SelfHitTestInvisible;
  }

  /**
   * True when a pointer over this widget itself can hit it.
   */
  NODISCARD FORCEINLINE bool
  isSelfHitTestVisible() const noexcept
  {
    return m_visibility == UIVisibility::Visible;
  }

  /**
   * True when the widget's children can be hit-tested.
   */
  NODISCARD FORCEINLINE bool
  areChildrenHitTestVisible() const noexcept
  {
    return m_visibility == UIVisibility::Visible ||
           m_visibility == UIVisibility::SelfHitTestInvisible;
  }

  /**
   * Whether keyboard/gamepad navigation may land on the widget.
   */
  NODISCARD FORCEINLINE bool
  isFocusable() const noexcept
  {
    return m_bFocusable;
  }

  FORCEINLINE void
  setFocusable(bool focusable) noexcept
  {
    m_bFocusable = focusable;
  }

  /**
   * True while the viewport has this widget focused. Only UIViewport sets it.
   */
  NODISCARD FORCEINLINE bool
  isFocused() const noexcept
  {
    return m_bFocused;
  }

  /**
   * True when the widget is enabled, visible and marked focusable.
   */
  NODISCARD FORCEINLINE bool
  canFocus() const noexcept
  {
    return m_bEnabled && isVisible() && m_bFocusable;
  }

  // -- Appearance -------------------------------------------------------------

  NODISCARD FORCEINLINE const LinearColor&
  getColor() const noexcept
  {
    return m_color;
  }

  FORCEINLINE void
  setColor(const LinearColor& color) noexcept
  {
    m_color = color;
  }

  NODISCARD FORCEINLINE const UIRenderTransform&
  getRenderTransform() const noexcept
  {
    return m_renderTransform;
  }

  FORCEINLINE void
  setRenderTransform(const UIRenderTransform& transform) noexcept
  {
    m_renderTransform = transform;
  }

  // -- Geometry ---------------------------------------------------------------

  NODISCARD FORCEINLINE const UIGeometry&
  getGeometry() const noexcept
  {
    return m_geometry;
  }

  NODISCARD FORCEINLINE UIRect
  getRect() const noexcept
  {
    return m_geometry.toRect();
  }

  NODISCARD FORCEINLINE Vector2
  getPosition() const noexcept
  {
    return m_geometry.position;
  }

  NODISCARD FORCEINLINE Vector2
  getSize() const noexcept
  {
    return m_geometry.size;
  }

  /**
   * Takes the rectangle the parent's slot computed and lays out the children. Panels
   * override arrangeChildren; the rectangle itself is always stored here.
   */
  virtual void
  arrange(const UIGeometry& geometry);

  // -- Hierarchy --------------------------------------------------------------

  NODISCARD FORCEINLINE UIWidget*
  getParent() const noexcept
  {
    return m_parent;
  }

  NODISCARD FORCEINLINE UIWidgetTree*
  getWidgetTree() const noexcept
  {
    return m_tree;
  }

  NODISCARD UIUserWidget*
  getUserWidget() const noexcept;

  NODISCARD UIViewport*
  getViewport() const noexcept;

  /**
   * Number of children the widget presents to traversal and layout.
   */
  NODISCARD virtual SIZE_T
  getChildCount() const noexcept
  {
    return 0;
  }

  NODISCARD virtual UIWidget*
  getChildAt(SIZE_T index) const noexcept;

  /**
   * The slot that placed this widget inside its parent, or nullptr for the tree root.
   */
  NODISCARD FORCEINLINE UIPanelSlot*
  getSlot() const noexcept
  {
    return m_slot;
  }

  // -- Hit testing ------------------------------------------------------------

  /**
   * True when point (in viewport space) lies inside the widget, after undoing its render
   * transform and honouring its clip rectangle.
   */
  NODISCARD bool
  containsPoint(const Vector2& point) const noexcept;

  /**
   * Topmost widget under point, in the subtree rooted here. Children are tested last to
   * first, so the widget drawn on top wins. Visibility decides what can be hit:
   * HitTestInvisible drops the whole subtree, SelfHitTestInvisible only the widget.
   */
  NODISCARD virtual UIWidget*
  hitTest(const Vector2& point) const;

  // -- Focus navigation -------------------------------------------------------

  FORCEINLINE void
  setNavigation(UINavigation direction, UINavigationLink link)
  {
    m_navigation[static_cast<SIZE_T>(direction)] = std::move(link);
  }

  NODISCARD FORCEINLINE const UINavigationLink&
  getNavigation(UINavigation direction) const noexcept
  {
    return m_navigation[static_cast<SIZE_T>(direction)];
  }

  // -- Lifecycle --------------------------------------------------------------

  /**
   * Called once when the widget joins a UIWidgetTree.
   */
  virtual void
  onAddedToTree();

  /**
   * Called once when the widget leaves its tree, before it is destroyed.
   */
  virtual void
  onRemovedFromTree();

  /**
   * Called when the owning user widget is constructed (added to a viewport).
   */
  virtual void
  onConstruct();

  /**
   * Called when the owning user widget is destructed (removed from a viewport).
   */
  virtual void
  onDestruct();

  /**
   * Per-frame hook, driven by UIUserWidget::tick.
   */
  virtual void
  onTick(float deltaTime);

  /**
   * Adds this widget's sprites to the draw list. Rendering is implemented in point 3.
   */
  virtual void
  paint(UIDrawList& drawList) const;

  // -- Input hooks ------------------------------------------------------------

  virtual void
  onPointerEnter(const Vector2& localPosition);

  virtual void
  onPointerExit(const Vector2& localPosition);

  virtual void
  onPointerDown(const Vector2& localPosition);

  virtual void
  onPointerUp(const Vector2& localPosition);

  virtual void
  onPointerClick(const Vector2& localPosition);

  /**
   * Routes one wheel step to this widget. The viewport starts at the widget under the
   * pointer and walks up the parents; the first widget that returns true consumes the
   * step and stops the walk. The base widget does not scroll.
   */
  NODISCARD virtual bool
  onScroll(float delta);

  virtual void
  onFocusReceived();

  virtual void
  onFocusLost();

  virtual void
  onSubmit();

  virtual void
  onCancel();

  /**
   * Whether the widget edits text, so navigation must leave the arrow keys to it.
   */
  NODISCARD virtual bool
  isTextEditor() const noexcept
  {
    return false;
  }

 protected:
  /**
   * Lays out the children after arrange stored this widget's rectangle. The base widget
   * has none; panels and content widgets override it.
   */
  virtual void
  arrangeChildren();

 private:
  friend class UIWidgetTree;
  friend class UIPanelWidget;
  friend class UIPanelSlot;
  friend class UIContentWidget;
  friend class UIUserWidget;
  friend class UIViewport;

  void
  attachToTree(UIWidgetTree* tree);

  void
  detachFromTree();

  void
  constructHierarchy();

  void
  destructHierarchy();

  void
  tickHierarchy(float deltaTime);

  void
  paintHierarchy(UIDrawList& drawList) const;

  NODISCARD SIZE_T
  countWidgets() const noexcept;

  /**
   * Maps a viewport-space point into the widget's untransformed local space, where (0, 0)
   * is its top-left corner.
   */
  NODISCARD Vector2
  toLocalPoint(const Vector2& point) const noexcept;

  String m_name;

  bool m_bEnabled = true;
  bool m_bFocusable = true;
  bool m_bFocused = false;

  UIVisibility m_visibility = UIVisibility::Visible;
  LinearColor m_color = LinearColor::WHITE;
  UIRenderTransform m_renderTransform;

  UIGeometry m_geometry;

  UIWidget* m_parent = nullptr;
  UIWidgetTree* m_tree = nullptr;
  UIPanelSlot* m_slot = nullptr;

  Array<UINavigationLink, static_cast<SIZE_T>(UINavigation::COUNT)> m_navigation{};
};

} // namespace chEngineSDK
