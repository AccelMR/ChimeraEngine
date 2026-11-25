/************************************************************************/
/**
 * @file chUIBorder.cpp
 * @author Swampertor
 * @date 2026/11/24
 * @brief Widget that draws a colored backdrop around one child, as UMG's UBorder.
 */
/************************************************************************/
#include "chUIBorder.h"

#include "chMath.h"
#include "chUIDrawList.h"

namespace chEngineSDK {

UIBorder::UIBorder() = default;

UIBorder::~UIBorder() = default;

/*
 */
Vector2
UIBorder::getDesiredSize() const
{
  Vector2 size = Vector2::ZERO;
  if (const UIWidget* content = getContent()) {
    size = content->getDesiredSize();
  }
  return size + Vector2(m_padding.getHorizontal(), m_padding.getVertical());
}

/*
 */
void
UIBorder::paint(UIDrawList& drawList) const
{
  if (m_brushColor.a <= 0.0f) {
    return;
  }
  drawList.addRect(getRect(), m_brushColor);
}

/*
 */
void
UIBorder::arrangeChildren()
{
  UIWidget* content = getContent();
  if (content == nullptr) {
    return;
  }

  const UIGeometry& parent = getGeometry();
  UIGeometry inner = parent;
  inner.position.x += m_padding.left;
  inner.position.y += m_padding.top;
  inner.size.x = Math::max(0.0f, parent.size.x - m_padding.getHorizontal());
  inner.size.y = Math::max(0.0f, parent.size.y - m_padding.getVertical());

  UIGeometry childGeometry = inner;
  if (UIPanelSlot* slot = getContentSlot()) {
    slot->arrange(inner, childGeometry);
  }
  content->arrange(childGeometry);
}

} // namespace chEngineSDK
