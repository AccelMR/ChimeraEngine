/************************************************************************/
/**
 * @file chUIImage.cpp
 * @author Swampertor
 * @date 2026/11/19
 * @brief Widget that draws a texture, as UMG's UImage.
 */
/************************************************************************/
#include "chUIImage.h"

#include "chTextureAsset.h"
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
Vector2
UIImage::getDesiredSize() const
{
  // A source rectangle crops the texture, so it is the size the image asks for, even before
  // the texture is bound.
  if (m_sourceSize.x > 0.0f && m_sourceSize.y > 0.0f) {
    return m_sourceSize;
  }
  if (m_texture != nullptr) {
    return {static_cast<float>(m_texture->getWidth()),
            static_cast<float>(m_texture->getHeight())};
  }
  return UIWidget::getDesiredSize();
}

/*
 */
void
UIImage::paint(UIDrawList& drawList) const
{
  drawList.addImage(getRect(), m_texture, m_filter, getColor(), m_sourcePosition, m_sourceSize,
                    m_bFlipX, m_bFlipY, Radian(0.0f), Vector2::ZERO, m_blendMode);
}

} // namespace chEngineSDK
