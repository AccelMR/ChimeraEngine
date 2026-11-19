/************************************************************************/
/**
 * @file chUIWidgetTree.cpp
 * @author Swampertor
 * @date 2026/11/19
 * @brief Owns the widgets of one UI document, as UMG's UWidgetTree.
 */
/************************************************************************/
#include "chUIWidgetTree.h"

namespace chEngineSDK {

namespace {

UIWidget*
findNamed(UIWidget* widget, StringView name)
{
  if (widget == nullptr) {
    return nullptr;
  }
  if (widget->getName() == name) {
    return widget;
  }

  const SIZE_T childCount = widget->getChildCount();
  for (SIZE_T index = 0; index < childCount; ++index) {
    if (UIWidget* found = findNamed(widget->getChildAt(index), name)) {
      return found;
    }
  }
  return nullptr;
}

void
visitWidgets(UIWidget* widget, const Function<void(UIWidget*)>& visitor)
{
  if (widget == nullptr) {
    return;
  }

  visitor(widget);

  const SIZE_T childCount = widget->getChildCount();
  for (SIZE_T index = 0; index < childCount; ++index) {
    visitWidgets(widget->getChildAt(index), visitor);
  }
}

} // namespace

UIWidgetTree::~UIWidgetTree()
{
  clear();
}

/*
 */
UIWidget*
UIWidgetTree::setRoot(UniquePtr<UIWidget> root)
{
  if (m_root == root) {
    return m_root.get();
  }

  if (m_root) {
    if (m_bConstructed) {
      m_root->destructHierarchy();
    }
    m_root->detachFromTree();
    m_root.reset();
  }

  m_root = std::move(root);

  if (m_root) {
    m_root->attachToTree(this);
    if (m_bConstructed) {
      m_root->constructHierarchy();
    }
  }

  return m_root.get();
}

/*
 */
void
UIWidgetTree::clear()
{
  if (!m_root) {
    return;
  }

  if (m_bConstructed) {
    m_root->destructHierarchy();
    m_bConstructed = false;
  }

  m_root->detachFromTree();
  m_root.reset();
}

/*
 */
UIWidget*
UIWidgetTree::findWidgetByName(StringView name) const
{
  return findNamed(m_root.get(), name);
}

/*
 */
void
UIWidgetTree::forEachWidget(const Function<void(UIWidget*)>& visitor) const
{
  visitWidgets(m_root.get(), visitor);
}

/*
 */
SIZE_T
UIWidgetTree::getWidgetCount() const noexcept
{
  return m_root ? m_root->countWidgets() : 0;
}

/*
 */
void
UIWidgetTree::arrange(const UIGeometry& geometry)
{
  if (m_root) {
    m_root->arrange(geometry);
  }
}

/*
 */
void
UIWidgetTree::construct()
{
  if (m_bConstructed) {
    return;
  }

  m_bConstructed = true;
  if (m_root) {
    m_root->constructHierarchy();
  }
}

/*
 */
void
UIWidgetTree::destruct()
{
  if (!m_bConstructed) {
    return;
  }

  m_bConstructed = false;
  if (m_root) {
    m_root->destructHierarchy();
  }
}

/*
 */
void
UIWidgetTree::tick(float deltaTime)
{
  if (!m_bConstructed || !m_root) {
    return;
  }

  m_root->tickHierarchy(deltaTime);
}

/*
 */
void
UIWidgetTree::paint(UIDrawList& drawList) const
{
  if (m_root) {
    m_root->paintHierarchy(drawList);
  }
}

} // namespace chEngineSDK
