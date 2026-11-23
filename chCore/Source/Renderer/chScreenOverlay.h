/************************************************************************/
/**
 * @file chScreenOverlay.h
 * @author AccelMR
 * @date 2026/11/23
 * @brief Sprites and text drawn over the final image, in window pixels.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chSpriteDraw.h"
#include "chVector2.h"

namespace chEngineSDK {

class Font;
class TextLayout;

/**
 * Exists so whatever is drawn over the final image (the in-game console, game UI) has one
 * place to go that works in every render path: sprites and text in window pixels, x right
 * and y down from the top left corner, drawn after post-processing, with no depth and blended
 * as the screen stores colors. It keeps a SpriteDraw whose plane maps Y to x and Z to -y, so
 * any SpriteDraw call works through toPlane. Fill it every frame (clear() first) and give it
 * to SceneRenderer::extract, which copies it. Main thread only.
 */
class CH_CORE_EXPORT ScreenOverlay
{
 public:
  FORCEINLINE void
  clear() noexcept
  {
    m_draw.clear();
  }

  NODISCARD bool
  isEmpty() const noexcept;

  /**
   * A filled rectangle whose top left corner is at position.
   */
  void
  rectangle(const Vector2& position,
            const Vector2& size,
            const LinearColor& color,
            int16 sortingLayer = 0,
            int16 order = 0);

  /**
   * A layout built with sizes in pixels, its pivot standing at position.
   */
  void
  text(Font& font,
       const TextLayout& layout,
       const Vector2& position,
       int16 sortingLayer = 0,
       int16 order = 0);

  NODISCARD FORCEINLINE SpriteDraw&
  getSpriteDraw() noexcept
  {
    return m_draw;
  }

  NODISCARD FORCEINLINE const SpriteDraw&
  getSpriteDraw() const noexcept
  {
    return m_draw;
  }

  /**
   * Where a window pixel is in the plane of getSpriteDraw().
   */
  NODISCARD static constexpr Vector2
  toPlane(const Vector2& pixel) noexcept
  {
    return Vector2(pixel.x, -pixel.y);
  }

 private:
  SpriteDraw m_draw;
};

} // namespace chEngineSDK
