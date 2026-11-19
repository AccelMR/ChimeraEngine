/************************************************************************/
/**
 * @file chUIUserWidget.h
 * @author Swampertor
 * @date 2026/11/19
 * @brief A widget document added to a viewport, as UMG's UUserWidget.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chUIWidgetTree.h"

namespace chEngineSDK {
class UIViewport;

/**
 * One UI document at runtime: it owns a UIWidgetTree and has the UMG lifecycle. A
 * subclass builds its tree in onInitialized, then receives onConstruct when it joins a
 * viewport, onTick every frame, and onDestruct when it leaves.
 *
 * The viewport owns user widgets; the user widget owns its tree; the tree owns the
 * widgets. None of those references goes the other way except as a non-owning pointer.
 */
class CH_CORE_EXPORT UIUserWidget
{
 public:
  UIUserWidget();
  virtual ~UIUserWidget();

  UIUserWidget(const UIUserWidget&) = delete;
  UIUserWidget&
  operator=(const UIUserWidget&) = delete;
  UIUserWidget(UIUserWidget&&) = delete;
  UIUserWidget&
  operator=(UIUserWidget&&) = delete;

  // -- Lifecycle --------------------------------------------------------------

  /**
   * Builds the tree once by calling onInitialized. Called automatically when the user
   * widget is added to a viewport.
   */
  void
  initialize();

  /**
   * Lays the tree out, calls onConstruct on every widget and then onConstruct here.
   */
  void
  construct();

  /**
   * Destroys the tree's widgets through onDestruct and then calls onDestruct here.
   */
  void
  destruct();

  void
  tick(float deltaTime);

  NODISCARD FORCEINLINE bool
  isInitialized() const noexcept
  {
    return m_bInitialized;
  }

  NODISCARD FORCEINLINE bool
  isConstructed() const noexcept
  {
    return m_bConstructed;
  }

  // -- Tree -------------------------------------------------------------------

  NODISCARD FORCEINLINE UIWidgetTree*
  getWidgetTree() const noexcept
  {
    return m_widgetTree.get();
  }

  NODISCARD FORCEINLINE UIWidget*
  getRootWidget() const noexcept
  {
    return m_widgetTree ? m_widgetTree->getRoot() : nullptr;
  }

  NODISCARD UIWidget*
  findWidgetByName(StringView name) const;

  /**
   * True when widget is the root or a descendant of this document's tree.
   */
  NODISCARD bool
  ownsWidget(const UIWidget* widget) const noexcept;

  // -- Viewport ---------------------------------------------------------------

  NODISCARD FORCEINLINE UIViewport*
  getViewport() const noexcept
  {
    return m_viewport;
  }

  /**
   * Re-applies the viewport rectangle to the whole tree. The viewport calls it on resize
   * and when the user widget is added.
   */
  void
  refreshLayout();

  // -- Hooks ------------------------------------------------------------------

  /**
   * Builds the tree. A subclass sets the root here with
   * getMutableWidgetTree().createRoot<T>() or setRoot().
   */
  virtual void
  onInitialized();

  virtual void
  onConstruct();

  virtual void
  onTick(float deltaTime);

  virtual void
  onDestruct();

 protected:
  NODISCARD FORCEINLINE UIWidgetTree&
  getMutableWidgetTree() noexcept
  {
    CH_ASSERT(m_widgetTree);
    return *m_widgetTree;
  }

 private:
  friend class UIViewport;

  void
  setViewport(UIViewport* viewport) noexcept
  {
    m_viewport = viewport;
  }

  UniquePtr<UIWidgetTree> m_widgetTree;
  UIViewport* m_viewport = nullptr;
  bool m_bInitialized = false;
  bool m_bConstructed = false;
};

} // namespace chEngineSDK
