/************************************************************************/
/**
 * @file chUIScrollBoxSlot.h
 * @author Swampertor
 * @date 2026/11/24
 * @brief Where a scroll box keeps one child: padding and horizontal alignment.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chUIBoxSlot.h"

namespace chEngineSDK {

/**
 * Stacking slot of a UIScrollBox. Like a vertical box slot it knows padding, alignment and
 * a size rule, but a scroll box has no free space to hand out: every child takes its
 * desired height and the box scrolls over the total.
 */
class CH_CORE_EXPORT UIScrollBoxSlot final : public UIBoxSlot
{
 public:
  UIScrollBoxSlot() : UIBoxSlot(UIOrientation::Vertical) {}

  NODISCARD const ANSICHAR*
  getTypeName() const noexcept override
  {
    return "UIScrollBoxSlot";
  }
};

} // namespace chEngineSDK
