/************************************************************************/
/**
 * @file chUIVerticalBox.h
 * @author Swampertor
 * @date 2026/11/24
 * @brief Panel that stacks its children top to bottom, as UMG's UVerticalBox.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chUIBoxSlot.h"
#include "chUIPanelWidget.h"

namespace chEngineSDK {

/**
 * Stacks its children from the top of its rectangle down. Auto children ask for their
 * desired height; Fill children share what is left by fillWeight. Each child is placed
 * across the width by its horizontal alignment, Fill taking the whole width.
 *
 * Children default to Auto, which is what a label or a button wants; set the rule to Fill
 * for the row that should grow with the box.
 */
class CH_CORE_EXPORT UIVerticalBox : public UIPanelWidget
{
 public:
  UIVerticalBox() = default;
  ~UIVerticalBox() override = default;

  NODISCARD const ANSICHAR*
  getTypeName() const noexcept override
  {
    return "UIVerticalBox";
  }

  using UIPanelWidget::addChild;

  /**
   * Adds a child with an Auto vertical slot.
   */
  UIWidget*
  addChild(UniquePtr<UIWidget> child) override;

  NODISCARD Vector2
  getDesiredSize() const override;

 protected:
  void
  arrangeChildren() override;
};

} // namespace chEngineSDK
