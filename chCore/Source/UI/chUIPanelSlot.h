/************************************************************************/
/**
 * @file chUIPanelSlot.h
 * @author Swampertor
 * @date 2026/11/19
 * @brief Where a panel keeps one child: the rectangle it gives it.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chUIRect.h"

namespace chEngineSDK {
class UIWidget;

/**
 * A panel's per-child layout data, as UMG's UPanelSlot. The slot belongs to the panel and
 * points at the child it places; the child points back at the slot that placed it.
 *
 * The base slot hands the child the panel's whole rectangle. Subclasses add anchors,
 * padding or alignment (UICanvasPanelSlot in point 4, the box slots in point 8).
 */
class CH_CORE_EXPORT UIPanelSlot
{
 public:
  UIPanelSlot() = default;
  virtual ~UIPanelSlot();

  UIPanelSlot(const UIPanelSlot&) = delete;
  UIPanelSlot&
  operator=(const UIPanelSlot&) = delete;
  UIPanelSlot(UIPanelSlot&&) = delete;
  UIPanelSlot&
  operator=(UIPanelSlot&&) = delete;

  NODISCARD FORCEINLINE UIWidget*
  getContent() const noexcept
  {
    return m_content;
  }

  /**
   * The panel or content widget that owns this slot.
   */
  NODISCARD FORCEINLINE UIWidget*
  getParentWidget() const noexcept
  {
    return m_parent;
  }

  /**
   * Computes the rectangle of the child from the rectangle the parent was arranged with.
   * The base slot fills the parent; derived slots may move, shrink or align the child.
   */
  virtual void
  arrange(const UIGeometry& parentGeometry, UIGeometry& outChildGeometry) const;

 protected:
  friend class UIPanelWidget;
  friend class UIContentWidget;

  void
  setContent(UIWidget* content) noexcept;

  void
  setParentWidget(UIWidget* parent) noexcept;

 private:
  UIWidget* m_content = nullptr;
  UIWidget* m_parent = nullptr;
};

} // namespace chEngineSDK
