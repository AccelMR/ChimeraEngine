/************************************************************************/
/**
 * @file chUIHorizontalBox.h
 * @author Swampertor
 * @date 2026/11/24
 * @brief Panel that places its children left to right, as UMG's UHorizontalBox.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chUIBoxSlot.h"
#include "chUIPanelWidget.h"

namespace chEngineSDK {

/**
 * Places its children from the left of its rectangle rightwards. Auto children ask for
 * their desired width; Fill children share what is left by fillWeight. Each child is
 * placed across the height by its vertical alignment, Fill taking the whole height.
 */
class CH_CORE_EXPORT UIHorizontalBox : public UIPanelWidget
{
 public:
  UIHorizontalBox() = default;
  ~UIHorizontalBox() override = default;

  NODISCARD const ANSICHAR*
  getTypeName() const noexcept override
  {
    return "UIHorizontalBox";
  }

  using UIPanelWidget::addChild;

  /**
   * Adds a child with an Auto horizontal slot.
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
