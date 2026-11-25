/************************************************************************/
/**
 * @file chUIScrollBox.h
 * @author Swampertor
 * @date 2026/11/24
 * @brief Panel that stacks its children and scrolls over them, as UMG's UScrollBox.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chUIPanelWidget.h"
#include "chUIScrollBoxSlot.h"

namespace chEngineSDK {

/**
 * Stacks its children like a vertical box and moves them up by a scroll offset. The wheel
 * scrolls the box while the content is taller than the view; the offset is clamped to the
 * content extent, so the last child always ends at the bottom edge.
 *
 * Clipping is not implemented yet: a child that scrolls past the edge still paints, and
 * the layout relies on a document placing the box where that is acceptable.
 */
class CH_CORE_EXPORT UIScrollBox : public UIPanelWidget
{
 public:
  UIScrollBox();
  ~UIScrollBox() override;

  NODISCARD const ANSICHAR*
  getTypeName() const noexcept override
  {
    return "UIScrollBox";
  }

  using UIPanelWidget::addChild;

  /**
   * Adds a child with a scroll box slot.
   */
  UIWidget*
  addChild(UniquePtr<UIWidget> child) override;

  // -- Scroll -----------------------------------------------------------------

  /**
   * Pixels the content was moved up; 0 shows the first child at the top.
   */
  NODISCARD FORCEINLINE float
  getScrollOffset() const noexcept
  {
    return m_scrollOffset;
  }

  /**
   * Moves the content, clamped into the scrollable range. Clamps to 0 until the box has
   * been arranged once.
   */
  void
  setScrollOffset(float offset);

  /**
   * Height the stacked children need, last arrange.
   */
  NODISCARD FORCEINLINE float
  getContentHeight() const noexcept
  {
    return m_contentHeight;
  }

  /**
   * Highest offset the current content allows; 0 when it fits.
   */
  NODISCARD FORCEINLINE float
  getMaxScrollOffset() const noexcept
  {
    return m_maxScrollOffset;
  }

  /**
   * Pixels one wheel step scrolls the content.
   */
  NODISCARD FORCEINLINE float
  getScrollStep() const noexcept
  {
    return m_scrollStep;
  }

  FORCEINLINE void
  setScrollStep(float step) noexcept
  {
    m_scrollStep = step;
  }

  NODISCARD Vector2
  getDesiredSize() const override;

  bool
  onScroll(float delta) override;

 protected:
  void
  arrangeChildren() override;

 private:
  void
  updateScrollRange(float viewHeight);

  float m_scrollOffset = 0.0f;
  float m_contentHeight = 0.0f;
  float m_maxScrollOffset = 0.0f;
  float m_scrollStep = 48.0f;
};

} // namespace chEngineSDK
