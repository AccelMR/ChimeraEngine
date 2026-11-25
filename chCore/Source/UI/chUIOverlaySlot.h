/************************************************************************/
/**
 * @file chUIOverlaySlot.h
 * @author Swampertor
 * @date 2026/11/24
 * @brief Where an overlay keeps one child: padding and alignment.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chUIPanelSlot.h"
#include "chUIRect.h"
#include "chUITypes.h"

namespace chEngineSDK {

/**
 * Places one child of a UIOverlay. A Fill axis gives the child the space left after the
 * padding; the other modes give it its desired size and align it in that space.
 */
class CH_CORE_EXPORT UIOverlaySlot : public UIPanelSlot
{
 public:
  UIOverlaySlot() = default;
  ~UIOverlaySlot() override;

  NODISCARD const ANSICHAR*
  getTypeName() const noexcept override
  {
    return "UIOverlaySlot";
  }

  void
  arrange(const UIGeometry& parentGeometry, UIGeometry& outChildGeometry) const override;

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
  UIMargin m_padding;
  UIHorizontalAlignment m_horizontalAlignment = UIHorizontalAlignment::Fill;
  UIVerticalAlignment m_verticalAlignment = UIVerticalAlignment::Fill;
};

} // namespace chEngineSDK
