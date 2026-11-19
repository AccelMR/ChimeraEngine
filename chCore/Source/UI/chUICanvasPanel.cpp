/************************************************************************/
/**
 * @file chUICanvasPanel.cpp
 * @author Swampertor
 * @date 2026/11/19
 * @brief Panel that places its children freely, as UMG's UCanvasPanel.
 */
/************************************************************************/
#include "chUICanvasPanel.h"

namespace chEngineSDK {

/*
 */
UIWidget*
UICanvasPanel::addChild(UniquePtr<UIWidget> child)
{
  return UIPanelWidget::addChild(std::move(child), chMakeUnique<UICanvasPanelSlot>());
}

/*
 */
UIWidget*
UICanvasPanel::addChildAt(UniquePtr<UIWidget> child, const UIRect& rect)
{
  UniquePtr<UICanvasPanelSlot> slot = chMakeUnique<UICanvasPanelSlot>();
  slot->setPosition(rect.position);
  slot->setSize(rect.size);
  return UIPanelWidget::addChild(std::move(child), std::move(slot));
}

} // namespace chEngineSDK
