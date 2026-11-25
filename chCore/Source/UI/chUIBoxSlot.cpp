/************************************************************************/
/**
 * @file chUIBoxSlot.cpp
 * @author Swampertor
 * @date 2026/11/24
 * @brief Where a box keeps one child: padding, size rule and alignment.
 */
/************************************************************************/
#include "chUIBoxSlot.h"

#include "chMath.h"

namespace chEngineSDK {

UIBoxSlot::~UIBoxSlot() = default;

/*
 */
void
UIBoxSlot::arrange(const UIGeometry& parentGeometry, UIGeometry& outChildGeometry) const
{
  outChildGeometry = parentGeometry;
  outChildGeometry.position.x += m_padding.left;
  outChildGeometry.position.y += m_padding.top;
  outChildGeometry.size.x =
      Math::max(0.0f, parentGeometry.size.x - m_padding.getHorizontal());
  outChildGeometry.size.y =
      Math::max(0.0f, parentGeometry.size.y - m_padding.getVertical());
}

} // namespace chEngineSDK
