/************************************************************************/
/**
 * @file chUIPanelSlot.cpp
 * @author Swampertor
 * @date 2026/11/19
 * @brief Where a panel keeps one child: the rectangle it gives it.
 */
/************************************************************************/
#include "chUIPanelSlot.h"

#include "chUIWidget.h"

namespace chEngineSDK {

UIPanelSlot::~UIPanelSlot()
{
  if (m_content != nullptr && m_content->m_slot == this) {
    m_content->m_slot = nullptr;
  }
}

/*
 */
void
UIPanelSlot::arrange(const UIGeometry& parentGeometry, UIGeometry& outChildGeometry) const
{
  // The base slot has no layout of its own: the child fills the parent.
  outChildGeometry = parentGeometry;
}

/*
 */
void
UIPanelSlot::setContent(UIWidget* content) noexcept
{
  if (m_content == content) {
    return;
  }

  if (m_content != nullptr && m_content->m_slot == this) {
    m_content->m_slot = nullptr;
  }

  m_content = content;

  if (m_content != nullptr) {
    m_content->m_slot = this;
  }
}

/*
 */
void
UIPanelSlot::setParentWidget(UIWidget* parent) noexcept
{
  m_parent = parent;
}

} // namespace chEngineSDK
