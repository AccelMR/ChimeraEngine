/************************************************************************/
/**
 * @file chUIImage.cpp
 * @author Swampertor
 * @date 2026/11/19
 * @brief Widget that draws a texture, as UMG's UImage.
 */
/************************************************************************/
#include "chUIImage.h"

#include "chUIDrawList.h"

namespace chEngineSDK {

UIImage::UIImage()
{
  // An image is not a control: navigation lands on the widgets that act. The default
  // visibility also lets an image inside a button pass hover and clicks through to it.
  setFocusable(false);
  setVisibility(UIVisibility::SelfHitTestInvisible);
}

UIImage::~UIImage() = default;

/*
 */
void
UIImage::paint(UIDrawList& drawList) const
{
  drawList.addImage(getRect(), m_texture, m_filter, getColor(), m_sourcePosition, m_sourceSize,
                    m_bFlipX, m_bFlipY, Radian(0.0f), Vector2::ZERO, m_blendMode);
}

} // namespace chEngineSDK
