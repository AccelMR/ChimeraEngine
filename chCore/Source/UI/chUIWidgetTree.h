/************************************************************************/
/**
 * @file chUIWidgetTree.h
 * @author Swampertor
 * @date 2026/11/19
 * @brief Owns the widgets of one UI document, as UMG's UWidgetTree.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chUIWidget.h"

namespace chEngineSDK {
class UIDrawList;
class UIUserWidget;

/**
 * One widget hierarchy. The tree owns the root; every panel owns its children, so the
 * whole hierarchy lives and dies with the tree. It is also the place where document-wide
 * operations walk the widgets: construction, ticking, painting and name lookup.
 *
 * A tree belongs to one UIUserWidget, which gives its widgets the user widget and the
 * viewport they are in.
 */
class CH_CORE_EXPORT UIWidgetTree
{
 public:
  UIWidgetTree() = default;
  ~UIWidgetTree();

  UIWidgetTree(const UIWidgetTree&) = delete;
  UIWidgetTree&
  operator=(const UIWidgetTree&) = delete;
  UIWidgetTree(UIWidgetTree&&) = delete;
  UIWidgetTree&
  operator=(UIWidgetTree&&) = delete;

  /**
   * Replaces the root. The old root (and its children) is destroyed. Returns the new root.
   */
  UIWidget*
  setRoot(UniquePtr<UIWidget> root);

  /**
   * Creates a widget as the root and returns it. The tree owns it from now on.
   */
  template<typename T, typename... Args>
  T*
  createRoot(Args&&... args)
  {
    static_assert(std::is_base_of_v<UIWidget, T>, "A UI root must be a UIWidget");
    T* raw = new T(std::forward<Args>(args)...);
    setRoot(UniquePtr<UIWidget>(raw));
    return raw;
  }

  NODISCARD FORCEINLINE UIWidget*
  getRoot() const noexcept
  {
    return m_root.get();
  }

  void
  clear();

  void
  setUserWidget(UIUserWidget* userWidget) noexcept
  {
    m_userWidget = userWidget;
  }

  NODISCARD FORCEINLINE UIUserWidget*
  getUserWidget() const noexcept
  {
    return m_userWidget;
  }

  NODISCARD FORCEINLINE bool
  isConstructed() const noexcept
  {
    return m_bConstructed;
  }

  /**
   * First widget in depth-first order with this name, or nullptr.
   */
  NODISCARD UIWidget*
  findWidgetByName(StringView name) const;

  /**
   * Visits every widget in depth-first order, root first.
   */
  void
  forEachWidget(const Function<void(UIWidget*)>& visitor) const;

  NODISCARD SIZE_T
  getWidgetCount() const noexcept;

  /**
   * Gives the root the viewport rectangle and lays the whole hierarchy out.
   */
  void
  arrange(const UIGeometry& geometry);

  /**
   * Calls onConstruct on every widget, root first. A tree is constructed once.
   */
  void
  construct();

  /**
   * Calls onDestruct on every widget, children first.
   */
  void
  destruct();

  void
  tick(float deltaTime);

  void
  paint(UIDrawList& drawList) const;

 private:
  UniquePtr<UIWidget> m_root;
  UIUserWidget* m_userWidget = nullptr;
  bool m_bConstructed = false;
};

} // namespace chEngineSDK
