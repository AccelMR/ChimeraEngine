/************************************************************************/
/**
 * @file chUIContentWidget.cpp
 * @author Swampertor
 * @date 2026/11/19
 * @brief Widget with exactly one child, as UMG's UContentWidget.
 */
/************************************************************************/
#include "chUIContentWidget.h"

#include "chUIWidgetTree.h"

namespace chEngineSDK {

UIContentWidget::UIContentWidget()
{
  // A wrapper is a layout construct, not a control: navigation lands on its content.
  setFocusable(false);
}

UIContentWidget::~UIContentWidget()
{
  // The content and its slot are destroyed by this object's members right after this
  // body; clear the back-pointers first so the content destructor never reaches a parent
  // that is already tearing down.
  if (m_content) {
    m_content->m_parent = nullptr;
    m_content->m_slot = nullptr;
  }
}

/*
 */
UIWidget*
UIContentWidget::getChildAt(SIZE_T index) const noexcept
{
  return index == 0 ? m_content.get() : nullptr;
}

/*
 */
UIWidget*
UIContentWidget::setContent(UniquePtr<UIWidget> content)
{
  return setContent(std::move(content), chMakeUnique<UIPanelSlot>());
}

/*
 */
UIWidget*
UIContentWidget::setContent(UniquePtr<UIWidget> content, UniquePtr<UIPanelSlot> slot)
{
  if (!content || !slot || content->getParent() != nullptr) {
    return nullptr;
  }

  clearContent();

  UIWidget* rawContent = content.get();
  rawContent->m_parent = this;

  slot->setParentWidget(this);
  slot->setContent(rawContent);

  m_content = std::move(content);
  m_contentSlot = std::move(slot);

  if (m_tree != nullptr) {
    rawContent->attachToTree(m_tree);
    if (m_tree->isConstructed()) {
      rawContent->constructHierarchy();
    }
  }

  return rawContent;
}

/*
 */
void
UIContentWidget::clearContent()
{
  if (m_content) {
    m_content->detachFromTree();
    m_content->m_parent = nullptr;
    m_content->m_slot = nullptr;
  }

  if (m_contentSlot) {
    m_contentSlot->setContent(nullptr);
  }

  // The slot points at the content, so it goes first.
  m_contentSlot.reset();
  m_content.reset();
}

/*
 */
void
UIContentWidget::arrangeChildren()
{
  if (!m_content) {
    return;
  }

  const UIGeometry& parentGeometry = getGeometry();
  UIGeometry childGeometry = parentGeometry;
  if (m_contentSlot) {
    m_contentSlot->arrange(parentGeometry, childGeometry);
  }
  m_content->arrange(childGeometry);
}

} // namespace chEngineSDK
