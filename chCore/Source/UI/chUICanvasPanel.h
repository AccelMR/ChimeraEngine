/************************************************************************/
/**
 * @file chUICanvasPanel.h
 * @author Swampertor
 * @date 2026/11/19
 * @brief Panel that places its children freely, as UMG's UCanvasPanel.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chUICanvasPanelSlot.h"
#include "chUIPanelWidget.h"
#include "chUIRect.h"

namespace chEngineSDK {

/**
 * A panel whose children carry their own anchors, offset and size. The base panel already
 * walks its slots and arranges each child, so the canvas only changes the slot type new
 * children get and adds a shortcut for an absolute rectangle.
 */
class CH_CORE_EXPORT UICanvasPanel : public UIPanelWidget
{
 public:
  UICanvasPanel() = default;
  ~UICanvasPanel() override = default;

  NODISCARD const ANSICHAR*
  getTypeName() const noexcept override
  {
    return "UICanvasPanel";
  }

  using UIPanelWidget::addChild;

  /**
   * Adds a child with a canvas slot at the panel's top-left and the slot's default size.
   */
  UIWidget*
  addChild(UniquePtr<UIWidget> child) override;

  /**
   * Adds a child placed at rect, in the panel's space, with no anchors and no alignment.
   */
  UIWidget*
  addChildAt(UniquePtr<UIWidget> child, const UIRect& rect);
};

} // namespace chEngineSDK
