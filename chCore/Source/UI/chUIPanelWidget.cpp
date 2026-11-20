/************************************************************************/
/**
 * @file chUIPanelWidget.cpp
 * @author Swampertor
 * @date 2026/11/19
 * @brief Widget with many children, each placed by its own slot.
 */
/************************************************************************/
#include "chUIPanelWidget.h"

#include <cstddef>

#include "chUIWidgetTree.h"

namespace chEngineSDK {

UIPanelWidget::UIPanelWidget()
{
  // A panel is a layout construct, not a control: navigation lands on its children and
  // empty panel space falls through to whatever is below. A panel that must block input
  // sets Visible explicitly.
  setFocusable(false);
  setVisibility(UIVisibility::SelfHitTestInvisible);
}

UIPanelWidget::~UIPanelWidget()
{
  // The slots and children are destroyed by this object's members right after this body.
  // Clear the back-pointers first, so a child destructor never reaches a parent that is
  // already tearing down.
  for (UniquePtr<UIWidget>& child : m_children) {
    if (child) {
      child->m_parent = nullptr;
      child->m_slot = nullptr;
    }
  }
}

/*
 */
UIWidget*
UIPanelWidget::getChildAt(SIZE_T index) const noexcept
{
  return index < m_children.size() ? m_children[index].get() : nullptr;
}

/*
 */
UIPanelSlot*
UIPanelWidget::getSlotAt(SIZE_T index) const noexcept
{
  return index < m_slots.size() ? m_slots[index].get() : nullptr;
}

/*
 */
UIWidget*
UIPanelWidget::addChild(UniquePtr<UIWidget> child)
{
  return addChild(std::move(child), chMakeUnique<UIPanelSlot>());
}

/*
 */
UIWidget*
UIPanelWidget::addChild(UniquePtr<UIWidget> child, UniquePtr<UIPanelSlot> slot)
{
  if (!child || !slot || child->getParent() != nullptr) {
    return nullptr;
  }

  UIWidget* rawChild = child.get();

  rawChild->m_parent = this;
  slot->setParentWidget(this);
  slot->setContent(rawChild);

  m_children.push_back(std::move(child));
  m_slots.push_back(std::move(slot));

  if (m_tree != nullptr) {
    rawChild->attachToTree(m_tree);
    if (m_tree->isConstructed()) {
      rawChild->constructHierarchy();
    }
  }

  return rawChild;
}

/*
 */
bool
UIPanelWidget::removeChild(UIWidget* child)
{
  if (child == nullptr) {
    return false;
  }

  for (SIZE_T index = 0; index < m_children.size(); ++index) {
    if (m_children[index].get() != child) {
      continue;
    }

    child->detachFromTree();
    child->m_parent = nullptr;
    child->m_slot = nullptr;

    if (UIPanelSlot* slot = m_slots[index].get()) {
      slot->setContent(nullptr);
    }

    // Destroy the slot first: it points at the child, and the child must outlive it.
    m_slots.erase(m_slots.begin() + static_cast<ptrdiff_t>(index));
    m_children.erase(m_children.begin() + static_cast<ptrdiff_t>(index));
    return true;
  }

  return false;
}

/*
 */
void
UIPanelWidget::clearChildren()
{
  for (UniquePtr<UIWidget>& child : m_children) {
    if (child) {
      child->detachFromTree();
      child->m_parent = nullptr;
      child->m_slot = nullptr;
    }
  }

  for (UniquePtr<UIPanelSlot>& slot : m_slots) {
    if (slot) {
      slot->setContent(nullptr);
    }
  }

  m_slots.clear();
  m_children.clear();
}

/*
 */
void
UIPanelWidget::arrangeChildren()
{
  const UIGeometry& parentGeometry = getGeometry();

  for (SIZE_T index = 0; index < m_children.size(); ++index) {
    UIWidget* child = m_children[index].get();
    if (child == nullptr) {
      continue;
    }

    UIGeometry childGeometry = parentGeometry;
    if (UIPanelSlot* slot = m_slots[index].get()) {
      slot->arrange(parentGeometry, childGeometry);
    }
    child->arrange(childGeometry);
  }
}

} // namespace chEngineSDK
