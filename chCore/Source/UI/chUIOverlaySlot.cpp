/************************************************************************/
/**
 * @file chUIOverlaySlot.cpp
 * @author Swampertor
 * @date 2026/11/24
 * @brief Where an overlay keeps one child: padding and alignment.
 */
/************************************************************************/
#include "chUIOverlaySlot.h"

#include "chMath.h"
#include "chUIWidget.h"

namespace chEngineSDK {

UIOverlaySlot::~UIOverlaySlot() = default;

/*
 */
void
UIOverlaySlot::arrange(const UIGeometry& parentGeometry,
                       UIGeometry& outChildGeometry) const
{
  const Vector2 available =
      Vector2(Math::max(0.0f, parentGeometry.size.x - m_padding.getHorizontal()),
              Math::max(0.0f, parentGeometry.size.y - m_padding.getVertical()));
  const Vector2 origin(parentGeometry.position.x + m_padding.left,
                       parentGeometry.position.y + m_padding.top);

  const UIWidget* content = getContent();
  const Vector2 desired = content ? content->getDesiredSize() : Vector2::ZERO;

  float width = available.x;
  float x = origin.x;
  if (m_horizontalAlignment != UIHorizontalAlignment::Fill) {
    width = Math::min(available.x, Math::max(0.0f, desired.x));
    if (m_horizontalAlignment == UIHorizontalAlignment::Center) {
      x += (available.x - width) * 0.5f;
    }
    else if (m_horizontalAlignment == UIHorizontalAlignment::Right) {
      x += available.x - width;
    }
  }

  float height = available.y;
  float y = origin.y;
  if (m_verticalAlignment != UIVerticalAlignment::Fill) {
    height = Math::min(available.y, Math::max(0.0f, desired.y));
    if (m_verticalAlignment == UIVerticalAlignment::Center) {
      y += (available.y - height) * 0.5f;
    }
    else if (m_verticalAlignment == UIVerticalAlignment::Bottom) {
      y += available.y - height;
    }
  }

  outChildGeometry = parentGeometry;
  outChildGeometry.position = {x, y};
  outChildGeometry.size = {width, height};
}

} // namespace chEngineSDK
