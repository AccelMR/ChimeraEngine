/************************************************************************/
/**
 * @file chUIImage.h
 * @author Swampertor
 * @date 2026/11/19
 * @brief Widget that draws a texture, as UMG's UImage.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chLinearColor.h"
#include "chSpriteDraw.h"
#include "chUIRect.h"
#include "chUIWidget.h"
#include "chVector2.h"

namespace chEngineSDK {
class TextureAsset;

/**
 * Draws a texture inside its rectangle, tinted by the widget's color. A widget without a
 * texture draws the tint alone, which makes it a solid rectangle until an image is set.
 *
 * The texture is not owned: a document keeps its texture assets alive. The render
 * transform is not applied to images yet.
 */
class CH_CORE_EXPORT UIImage : public UIWidget
{
 public:
  UIImage();
  ~UIImage() override;

  NODISCARD const ANSICHAR*
  getTypeName() const noexcept override
  {
    return "UIImage";
  }

  NODISCARD FORCEINLINE const TextureAsset*
  getTexture() const noexcept
  {
    return m_texture;
  }

  void
  setTexture(const TextureAsset* texture) noexcept
  {
    m_texture = texture;
  }

  NODISCARD FORCEINLINE SpriteFilter
  getFilter() const noexcept
  {
    return m_filter;
  }

  FORCEINLINE void
  setFilter(SpriteFilter filter) noexcept
  {
    m_filter = filter;
  }

  /**
   * Top-left corner of the part of the texture to show, in pixels.
   */
  NODISCARD FORCEINLINE const Vector2&
  getSourcePosition() const noexcept
  {
    return m_sourcePosition;
  }

  FORCEINLINE void
  setSourcePosition(const Vector2& position) noexcept
  {
    m_sourcePosition = position;
  }

  /**
   * Size of the part of the texture to show, in pixels; zero shows all of it.
   */
  NODISCARD FORCEINLINE const Vector2&
  getSourceSize() const noexcept
  {
    return m_sourceSize;
  }

  FORCEINLINE void
  setSourceSize(const Vector2& size) noexcept
  {
    m_sourceSize = size;
  }

  NODISCARD FORCEINLINE bool
  getFlipX() const noexcept
  {
    return m_bFlipX;
  }

  FORCEINLINE void
  setFlipX(bool flip) noexcept
  {
    m_bFlipX = flip;
  }

  NODISCARD FORCEINLINE bool
  getFlipY() const noexcept
  {
    return m_bFlipY;
  }

  FORCEINLINE void
  setFlipY(bool flip) noexcept
  {
    m_bFlipY = flip;
  }

  NODISCARD FORCEINLINE SpriteBlendMode
  getBlendMode() const noexcept
  {
    return m_blendMode;
  }

  FORCEINLINE void
  setBlendMode(SpriteBlendMode blendMode) noexcept
  {
    m_blendMode = blendMode;
  }

  void
  paint(UIDrawList& drawList) const override;

 private:
  const TextureAsset* m_texture = nullptr;
  SpriteFilter m_filter = SpriteFilter::Linear;
  Vector2 m_sourcePosition = Vector2::ZERO;
  Vector2 m_sourceSize = Vector2::ZERO;
  bool m_bFlipX = false;
  bool m_bFlipY = false;
  SpriteBlendMode m_blendMode = SpriteBlendMode::Alpha;
};

} // namespace chEngineSDK
