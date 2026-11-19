/************************************************************************/
/**
 * @file chUIUserWidget.cpp
 * @author Swampertor
 * @date 2026/11/19
 * @brief A widget document added to a viewport, as UMG's UUserWidget.
 */
/************************************************************************/
#include "chUIUserWidget.h"

#include "chUIViewport.h"

namespace chEngineSDK {

UIUserWidget::UIUserWidget()
 : m_widgetTree(chMakeUnique<UIWidgetTree>())
{
  m_widgetTree->setUserWidget(this);
}

UIUserWidget::~UIUserWidget()
{
  if (m_bConstructed) {
    destruct();
  }
}

/*
 */
void
UIUserWidget::initialize()
{
  if (m_bInitialized) {
    return;
  }

  m_bInitialized = true;
  onInitialized();
}

/*
 */
void
UIUserWidget::construct()
{
  if (!m_bInitialized) {
    initialize();
  }
  if (m_bConstructed) {
    return;
  }

  refreshLayout();

  m_bConstructed = true;
  if (m_widgetTree) {
    m_widgetTree->construct();
  }
  onConstruct();
}

/*
 */
void
UIUserWidget::destruct()
{
  if (!m_bConstructed) {
    return;
  }

  onDestruct();

  m_bConstructed = false;
  if (m_widgetTree) {
    m_widgetTree->destruct();
  }
}

/*
 */
void
UIUserWidget::tick(float deltaTime)
{
  if (!m_bConstructed) {
    return;
  }

  if (m_widgetTree) {
    m_widgetTree->tick(deltaTime);
  }
  onTick(deltaTime);
}

/*
 */
UIWidget*
UIUserWidget::findWidgetByName(StringView name) const
{
  return m_widgetTree ? m_widgetTree->findWidgetByName(name) : nullptr;
}

/*
 */
bool
UIUserWidget::ownsWidget(const UIWidget* widget) const noexcept
{
  if (widget == nullptr) {
    return false;
  }

  const UIWidget* root = widget;
  while (root->getParent() != nullptr) {
    root = root->getParent();
  }
  return root == getRootWidget();
}

/*
 */
void
UIUserWidget::refreshLayout()
{
  if (!m_widgetTree) {
    return;
  }

  UIWidget* root = m_widgetTree->getRoot();
  if (root == nullptr) {
    return;
  }

  Vector2 size = m_viewport ? m_viewport->getViewportSize() : Vector2::ZERO;
  if (size.x <= 0.0f && size.y <= 0.0f) {
    size = root->getSize();
  }

  UIGeometry geometry;
  geometry.position = Vector2::ZERO;
  geometry.size = size;
  m_widgetTree->arrange(geometry);
}

/*
 */
void
UIUserWidget::onInitialized()
{
}

/*
 */
void
UIUserWidget::onConstruct()
{
}

/*
 */
void
UIUserWidget::onTick(float deltaTime)
{
  CH_PARAMETER_UNUSED(deltaTime);
}

/*
 */
void
UIUserWidget::onDestruct()
{
}

} // namespace chEngineSDK
