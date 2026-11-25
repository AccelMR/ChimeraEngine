/************************************************************************/
/**
 * @file chUIBoxSlot.h
 * @author Swampertor
 * @date 2026/11/24
 * @brief Where a box keeps one child: padding, size rule and alignment.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chUIPanelSlot.h"
#include "chUIRect.h"
#include "chUITypes.h"

namespace chEngineSDK {

/**
 * Layout data of one child of a vertical or horizontal box, as UMG's UBoxSlot. A slot with
 * the Auto rule asks the child for its desired size; a Fill slot takes a share of the
 * space the Auto children leave, weighted by fillWeight. The cross axis is placed by the
 * alignment of the box's orientation.
 *
 * The box itself arranges its children (it has to know every sibling to divide the space),
 * so arrange is only the fallback that fills the parent.
 */
class CH_CORE_EXPORT UIBoxSlot : public UIPanelSlot
{
 public:
  explicit UIBoxSlot(UIOrientation orientation = UIOrientation::Vertical)
   : m_orientation(orientation)
  {
  }

  ~UIBoxSlot() override;

  NODISCARD const ANSICHAR*
  getTypeName() const noexcept override
  {
    return "UIBoxSlot";
  }

  void
  arrange(const UIGeometry& parentGeometry, UIGeometry& outChildGeometry) const override;

  NODISCARD FORCEINLINE UIOrientation
  getOrientation() const noexcept
  {
    return m_orientation;
  }

  // -- Padding ----------------------------------------------------------------

  NODISCARD FORCEINLINE const UIMargin&
  getPadding() const noexcept
  {
    return m_padding;
  }

  FORCEINLINE void
  setPadding(const UIMargin& padding) noexcept
  {
    m_padding = padding;
  }

  // -- Size -------------------------------------------------------------------

  NODISCARD FORCEINLINE UISizeRule
  getSizeRule() const noexcept
  {
    return m_sizeRule;
  }

  FORCEINLINE void
  setSizeRule(UISizeRule rule) noexcept
  {
    m_sizeRule = rule;
  }

  /**
   * Share of the free space a Fill child takes, before its siblings are considered.
   */
  NODISCARD FORCEINLINE float
  getFillWeight() const noexcept
  {
    return m_fillWeight;
  }

  FORCEINLINE void
  setFillWeight(float weight) noexcept
  {
    m_fillWeight = weight;
  }

  // -- Alignment --------------------------------------------------------------

  /**
   * Where the child sits across a vertical box, or along a horizontal one.
   */
  NODISCARD FORCEINLINE UIHorizontalAlignment
  getHorizontalAlignment() const noexcept
  {
    return m_horizontalAlignment;
  }

  FORCEINLINE void
  setHorizontalAlignment(UIHorizontalAlignment alignment) noexcept
  {
    m_horizontalAlignment = alignment;
  }

  /**
   * Where the child sits across a horizontal box, or along a vertical one.
   */
  NODISCARD FORCEINLINE UIVerticalAlignment
  getVerticalAlignment() const noexcept
  {
    return m_verticalAlignment;
  }

  FORCEINLINE void
  setVerticalAlignment(UIVerticalAlignment alignment) noexcept
  {
    m_verticalAlignment = alignment;
  }

 private:
  UIOrientation m_orientation = UIOrientation::Vertical;
  UIMargin m_padding;
  UISizeRule m_sizeRule = UISizeRule::Auto;
  float m_fillWeight = 1.0f;
  UIHorizontalAlignment m_horizontalAlignment = UIHorizontalAlignment::Fill;
  UIVerticalAlignment m_verticalAlignment = UIVerticalAlignment::Fill;
};

/**
 * Slot a UIVerticalBox gives its children.
 */
class CH_CORE_EXPORT UIVerticalBoxSlot final : public UIBoxSlot
{
 public:
  UIVerticalBoxSlot() : UIBoxSlot(UIOrientation::Vertical) {}

  NODISCARD const ANSICHAR*
  getTypeName() const noexcept override
  {
    return "UIVerticalBoxSlot";
  }
};

/**
 * Slot a UIHorizontalBox gives its children.
 */
class CH_CORE_EXPORT UIHorizontalBoxSlot final : public UIBoxSlot
{
 public:
  UIHorizontalBoxSlot() : UIBoxSlot(UIOrientation::Horizontal) {}

  NODISCARD const ANSICHAR*
  getTypeName() const noexcept override
  {
    return "UIHorizontalBoxSlot";
  }
};

} // namespace chEngineSDK
