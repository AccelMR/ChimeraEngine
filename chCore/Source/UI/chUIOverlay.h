/************************************************************************/
/**
 * @file chUIOverlay.h
 * @author Swampertor
 * @date 2026/11/24
 * @brief Panel that stacks its children on one rectangle, as UMG's UOverlay.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chUIOverlaySlot.h"
#include "chUIPanelWidget.h"

namespace chEngineSDK {

/**
 * Gives every child the same rectangle and draws them in add order, so a later child sits
 * on top of an earlier one. Each slot chooses padding and alignment; the defaults fill the
 * overlay, which is what a text drawn over an image wants.
 */
class CH_CORE_EXPORT UIOverlay : public UIPanelWidget
{
 public:
  UIOverlay() = default;
  ~UIOverlay() override = default;

  NODISCARD const ANSICHAR*
  getTypeName() const noexcept override
  {
    return "UIOverlay";
  }

  using UIPanelWidget::addChild;

  UIWidget*
  addChild(UniquePtr<UIWidget> child) override;

  NODISCARD Vector2
  getDesiredSize() const override;
};

} // namespace chEngineSDK
