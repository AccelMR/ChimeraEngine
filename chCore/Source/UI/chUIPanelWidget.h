/************************************************************************/
/**
 * @file chUIPanelWidget.h
 * @author Swampertor
 * @date 2026/11/19
 * @brief Widget with many children, each placed by its own slot.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chUIPanelSlot.h"
#include "chUIWidget.h"

namespace chEngineSDK {

/**
 * Base of every widget that owns many children, as UMG's UPanelWidget. The panel owns the
 * child widgets and their slots; the children know the panel only through UIWidget::getParent.
 *
 * The base panel stacks every child on its own rectangle (the default slot fills the
 * parent). UICanvasPanel, the boxes and UIScrollBox override arrangeChildren with their own
 * layout.
 */
class CH_CORE_EXPORT UIPanelWidget : public UIWidget
{
 public:
  UIPanelWidget();
  ~UIPanelWidget() override;

  NODISCARD const ANSICHAR*
  getTypeName() const noexcept override
  {
    return "UIPanelWidget";
  }

  NODISCARD SIZE_T
  getChildCount() const noexcept override
  {
    return m_children.size();
  }

  NODISCARD UIWidget*
  getChildAt(SIZE_T index) const noexcept override;

  NODISCARD UIPanelSlot*
  getSlotAt(SIZE_T index) const noexcept;

  /**
   * Adds a child with a default slot that fills the panel. Returns the child, or nullptr
   * when it is null or still owned by another parent.
   */
  UIWidget*
  addChild(UniquePtr<UIWidget> child);

  /**
   * Adds a child with the slot that should place it. The panel takes both.
   */
  UIWidget*
  addChild(UniquePtr<UIWidget> child, UniquePtr<UIPanelSlot> slot);

  /**
   * Destroys a direct child and its slot. Returns false when it is not a child.
   */
  bool
  removeChild(UIWidget* child);

  void
  clearChildren();

 protected:
  void
  arrangeChildren() override;

 private:
  Vector<UniquePtr<UIWidget>> m_children;
  Vector<UniquePtr<UIPanelSlot>> m_slots;
};

} // namespace chEngineSDK
