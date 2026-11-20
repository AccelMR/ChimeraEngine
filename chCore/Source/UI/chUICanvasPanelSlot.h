/************************************************************************/
/**
 * @file chUICanvasPanelSlot.h
 * @author Swampertor
 * @date 2026/11/19
 * @brief Places one child of a canvas panel with anchors, an offset and a size.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chUIPanelSlot.h"
#include "chVector2.h"

namespace chEngineSDK {

/**
 * The layout data of a child of a UICanvasPanel, as UMG's UCanvasPanelSlot. It keeps the
 * SFMX UISlot formula exactly, so documents made for the old runtime place their widgets
 * the same way:
 *
 *   refMin = anchorMin * parentSize
 *   refMax = anchorMax * parentSize
 *   ref    = lerp(refMin, refMax, alignment)
 *   pos    = parentPosition + ref + position - alignment * size
 *
 * alignment is UMG's name for the SFMX pivot: (0, 0) puts the widget's top-left at the
 * reference point, (0.5, 0.5) its center and (1, 1) its bottom-right. The size is always
 * the one stored here; anchors move the reference point, they do not stretch the widget.
 * The panel origin is added because arranged geometry is absolute viewport space, while
 * anchors and the offset are authored in the panel's space.
 */
class CH_CORE_EXPORT UICanvasPanelSlot : public UIPanelSlot
{
 public:
  UICanvasPanelSlot() = default;
  ~UICanvasPanelSlot() override;

  NODISCARD const ANSICHAR*
  getTypeName() const noexcept override
  {
    return "UICanvasPanelSlot";
  }

  void
  arrange(const UIGeometry& parentGeometry, UIGeometry& outChildGeometry) const override;

  // -- Anchors ----------------------------------------------------------------

  NODISCARD FORCEINLINE const Vector2&
  getAnchorMin() const noexcept
  {
    return m_anchorMin;
  }

  FORCEINLINE void
  setAnchorMin(const Vector2& anchor) noexcept
  {
    m_anchorMin = anchor;
  }

  NODISCARD FORCEINLINE const Vector2&
  getAnchorMax() const noexcept
  {
    return m_anchorMax;
  }

  FORCEINLINE void
  setAnchorMax(const Vector2& anchor) noexcept
  {
    m_anchorMax = anchor;
  }

  /**
   * Sets both corners at once; equal corners pin the widget to one point of the parent.
   */
  FORCEINLINE void
  setAnchors(const Vector2& anchorMin, const Vector2& anchorMax) noexcept
  {
    m_anchorMin = anchorMin;
    m_anchorMax = anchorMax;
  }

  // -- Placement --------------------------------------------------------------

  /**
   * UMG's alignment (the SFMX pivot), as a fraction of the widget's size.
   */
  NODISCARD FORCEINLINE const Vector2&
  getAlignment() const noexcept
  {
    return m_alignment;
  }

  FORCEINLINE void
  setAlignment(const Vector2& alignment) noexcept
  {
    m_alignment = alignment;
  }

  /**
   * Offset from the reference point the anchors and the alignment compute.
   */
  NODISCARD FORCEINLINE const Vector2&
  getPosition() const noexcept
  {
    return m_position;
  }

  FORCEINLINE void
  setPosition(const Vector2& position) noexcept
  {
    m_position = position;
  }

  NODISCARD FORCEINLINE const Vector2&
  getSize() const noexcept
  {
    return m_size;
  }

  FORCEINLINE void
  setSize(const Vector2& size) noexcept
  {
    m_size = size;
  }

 private:
  Vector2 m_anchorMin = Vector2::ZERO;
  Vector2 m_anchorMax = Vector2::ZERO;
  Vector2 m_alignment = Vector2::ZERO;
  Vector2 m_position = Vector2::ZERO;
  Vector2 m_size{100.0f, 100.0f};
};

} // namespace chEngineSDK
