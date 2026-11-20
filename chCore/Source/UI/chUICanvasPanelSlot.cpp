/************************************************************************/
/**
 * @file chUICanvasPanelSlot.cpp
 * @author Swampertor
 * @date 2026/11/19
 * @brief Places one child of a canvas panel with anchors, an offset and a size.
 */
/************************************************************************/
#include "chUICanvasPanelSlot.h"

namespace chEngineSDK {

UICanvasPanelSlot::~UICanvasPanelSlot() = default;

/*
 */
void
UICanvasPanelSlot::arrange(const UIGeometry& parentGeometry,
                           UIGeometry& outChildGeometry) const
{
  const Vector2 parentSize = parentGeometry.size;
  const Vector2 refMin{m_anchorMin.x * parentSize.x, m_anchorMin.y * parentSize.y};
  const Vector2 refMax{m_anchorMax.x * parentSize.x, m_anchorMax.y * parentSize.y};
  const Vector2 reference{refMin.x + (refMax.x - refMin.x) * m_alignment.x,
                          refMin.y + (refMax.y - refMin.y) * m_alignment.y};

  // Geometry is absolute, but anchors and the offset are relative to the panel, so the
  // panel's own origin has to be added back here.
  outChildGeometry = parentGeometry;
  outChildGeometry.position = {
      parentGeometry.position.x + reference.x + m_position.x - m_alignment.x * m_size.x,
      parentGeometry.position.y + reference.y + m_position.y - m_alignment.y * m_size.y};
  outChildGeometry.size = m_size;
}

} // namespace chEngineSDK
