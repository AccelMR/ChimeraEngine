/************************************************************************/
/**
 * @file chUIDrawList.h
 * @author Swampertor
 * @date 2026/11/19
 * @brief Draw commands of one UI frame, in UI space, ready for the sprite pass.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chAngle.h"
#include "chLinearColor.h"
#include "chSpriteDraw.h"
#include "chUIRect.h"
#include "chVector2.h"

namespace chEngineSDK {
class Font;
class TextLayout;
class TextureAsset;

/**
 * Exists so widgets paint in UI coordinates and the renderer still receives plain sprites:
 * it converts every top-left rectangle into the YZ plane the sprite pass draws (right is
 * +Y, up is +Z), gives each drawable its own order so the paint order is kept, and reuses
 * SpriteDraw for quads, text and glyph runs.
 *
 * UI space starts at the top-left of the viewport with +X right and +Y down. A UI point
 * (x, y) becomes world (Y, Z) = (x, viewportHeight - y). The conversion is its own inverse.
 *
 * A draw list is cleared at the start of a frame and painted by UIViewport::paint. Its
 * vectors keep their memory after clear(), so a frame that draws the same amount allocates
 * nothing after the first ones. Main thread only.
 */
class CH_CORE_EXPORT UIDrawList
{
 public:
  UIDrawList() = default;

  explicit UIDrawList(const Vector2& viewportSize);

  // -- Viewport ---------------------------------------------------------------

  /**
   * Size in pixels of the surface the UI covers. It is what turns UI Y into world Z.
   */
  void
  setViewportSize(const Vector2& size) noexcept
  {
    m_viewportSize = size;
  }

  NODISCARD FORCEINLINE const Vector2&
  getViewportSize() const noexcept
  {
    return m_viewportSize;
  }

  /**
   * The Y and Z of a UI point, as the sprite pass reads them.
   */
  NODISCARD FORCEINLINE Vector2
  toWorldPosition(const Vector2& uiPosition) const noexcept
  {
    return {uiPosition.x, m_viewportSize.y - uiPosition.y};
  }

  /**
   * The UI point of a Y and Z position; the inverse of toWorldPosition.
   */
  NODISCARD FORCEINLINE Vector2
  toUIPosition(const Vector2& worldPosition) const noexcept
  {
    return {worldPosition.x, m_viewportSize.y - worldPosition.y};
  }

  // -- Frame ------------------------------------------------------------------

  /**
   * Forgets every drawable but keeps the memory, and restarts the paint order.
   */
  void
  clear() noexcept;

  NODISCARD bool
  isEmpty() const noexcept;

  // -- Drawables --------------------------------------------------------------

  /**
   * A solid rectangle. rotation turns it counterclockwise on screen around pivot, a
   * fraction of the rectangle: (0, 0) is its top-left corner, (1, 1) its bottom-right.
   */
  void
  addRect(const UIRect& rect,
          const LinearColor& color,
          Radian rotation = Radian(0.0f),
          const Vector2& pivot = Vector2::ZERO,
          SpriteBlendMode blendMode = SpriteBlendMode::Alpha);

  /**
   * A texture rectangle. sourcePosition and sourceSize pick a part of the texture in
   * pixels; a zero size shows the whole image. filter decides how the texture is read.
   */
  void
  addImage(const UIRect& rect,
           const TextureAsset* texture,
           SpriteFilter filter = SpriteFilter::Linear,
           const LinearColor& tint = LinearColor::WHITE,
           const Vector2& sourcePosition = Vector2::ZERO,
           const Vector2& sourceSize = Vector2::ZERO,
           bool bFlipX = false,
           bool bFlipY = false,
           Radian rotation = Radian(0.0f),
           const Vector2& pivot = Vector2::ZERO,
           SpriteBlendMode blendMode = SpriteBlendMode::Alpha);

  /**
   * A texture rectangle whose flags and UV rectangle are already resolved, for callers
   * that draw many images of one atlas and compute them once (SpriteDraw helpers).
   */
  void
  addImage(const UIRect& rect,
           uint32 textureAndFlags,
           const float (&uvRect)[4],
           const LinearColor& tint = LinearColor::WHITE,
           Radian rotation = Radian(0.0f),
           const Vector2& pivot = Vector2::ZERO,
           SpriteBlendMode blendMode = SpriteBlendMode::Alpha);

  /**
   * A laid-out text whose pivot stands at uiPivotPosition, in UI space. The layout holds
   * the glyph positions and its own pivot; desc carries the look and any outline/shadow.
   * Its position, sorting layer and order are set here.
   */
  void
  addText(Font& font,
          const TextLayout& layout,
          const Vector2& uiPivotPosition,
          TextDrawDesc desc = {});

  /**
   * A short text as glyph runs, for labels that change every frame. uiPivotPosition is
   * where desc.pivot stands, in UI space.
   */
  void
  addGlyphRun(Font& font,
              StringView text,
              const Vector2& uiPivotPosition,
              GlyphRunDesc desc = {});

  // -- Sprites ----------------------------------------------------------------

  NODISCARD FORCEINLINE Span<const SpriteItem>
  getSprites() const noexcept
  {
    return m_draw.getSprites();
  }

  NODISCARD FORCEINLINE Span<const GlyphRunItem>
  getGlyphRuns() const noexcept
  {
    return m_draw.getGlyphRuns();
  }

  NODISCARD FORCEINLINE Span<const GlyphRunBatch>
  getGlyphRunBatches() const noexcept
  {
    return m_draw.getGlyphRunBatches();
  }

  NODISCARD FORCEINLINE Span<const CustomSpriteBatch>
  getCustomBatches() const noexcept
  {
    return m_draw.getCustomBatches();
  }

  NODISCARD FORCEINLINE const SpriteDraw&
  getSpriteDraw() const noexcept
  {
    return m_draw;
  }

 private:
  /**
   * Gives the next drawable a layer and an order that put it after every earlier one. The
   * pair holds 2^30 drawables: the layer takes the high bits, the order the low 15.
   */
  void
  nextDrawableKey(int16& outSortingLayer, int16& outOrder) noexcept;

  NODISCARD FORCEINLINE SpriteQuad
  makeQuad(const UIRect& rect,
           const LinearColor& color,
           Radian rotation,
           const Vector2& pivot) const noexcept;

  SpriteDraw m_draw;
  Vector2 m_viewportSize = Vector2::ZERO;
  uint32 m_drawIndex = 0;
};

} // namespace chEngineSDK
