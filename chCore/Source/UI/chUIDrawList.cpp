/************************************************************************/
/**
 * @file chUIDrawList.cpp
 * @author Swampertor
 * @date 2026/11/19
 * @brief Draw commands of one UI frame, in UI space, ready for the sprite pass.
 */
/************************************************************************/
#include "chUIDrawList.h"

#include "chSpriteItem.h"
#include "chTextLayout.h"

namespace chEngineSDK {

/*
 */
UIDrawList::UIDrawList(const Vector2& viewportSize) : m_viewportSize(viewportSize)
{
}

/*
 */
void
UIDrawList::clear() noexcept
{
  m_draw.clear();
  m_drawIndex = 0;
}

/*
 */
bool
UIDrawList::isEmpty() const noexcept
{
  return m_draw.getSprites().empty() && m_draw.getGlyphRuns().empty() &&
         m_draw.getGlyphRunBatches().empty() && m_draw.getCustomBatches().empty();
}

/*
 */
void
UIDrawList::nextDrawableKey(int16& outSortingLayer, int16& outOrder) noexcept
{
  const uint32 index = m_drawIndex++;
  // Layer first, then order: 32768 drawables per layer keep the paint order exactly.
  outSortingLayer = static_cast<int16>(index >> 15);
  outOrder = static_cast<int16>(index & 0x7FFFu);
}

/*
 */
SpriteQuad
UIDrawList::makeQuad(const UIRect& rect,
                     const LinearColor& color,
                     Radian rotation,
                     const Vector2& pivot) const noexcept
{
  SpriteQuad quad;
  // The pivot is a UI fraction, so the world point is the one the quad turns around. Sprite
  // quads have their V axis up, while UI Y grows down: the pivot flips.
  const Vector2 pivotPosition{rect.position.x + rect.size.x * pivot.x,
                              rect.position.y + rect.size.y * pivot.y};
  quad.position = toWorldPosition(pivotPosition);
  quad.size = rect.size;
  quad.rotation = rotation;
  quad.pivot = {pivot.x, 1.0f - pivot.y};
  quad.color = color;
  return quad;
}

/*
 */
void
UIDrawList::addRect(const UIRect& rect,
                    const LinearColor& color,
                    Radian rotation,
                    const Vector2& pivot,
                    SpriteBlendMode blendMode)
{
  if (rect.isEmpty()) {
    return;
  }

  SpriteQuad quad = makeQuad(rect, color, rotation, pivot);
  quad.textureAndFlags = GpuSpriteInstance::NO_TEXTURE;
  quad.blendMode = blendMode;
  nextDrawableKey(quad.sortingLayer, quad.order);
  m_draw.quad(quad);
}

/*
 */
void
UIDrawList::addImage(const UIRect& rect,
                     const TextureAsset* texture,
                     SpriteFilter filter,
                     const LinearColor& tint,
                     const Vector2& sourcePosition,
                     const Vector2& sourceSize,
                     bool bFlipX,
                     bool bFlipY,
                     Radian rotation,
                     const Vector2& pivot,
                     SpriteBlendMode blendMode)
{
  float uvRect[4] = {0.0f, 0.0f, 1.0f, 1.0f};
  SpriteDraw::makeUvRect(texture, sourcePosition, sourceSize, bFlipX, bFlipY, uvRect);
  addImage(rect,
           SpriteDraw::makeTextureFlags(texture, filter),
           uvRect,
           tint,
           rotation,
           pivot,
           blendMode);
}

/*
 */
void
UIDrawList::addImage(const UIRect& rect,
                     uint32 textureAndFlags,
                     const float (&uvRect)[4],
                     const LinearColor& tint,
                     Radian rotation,
                     const Vector2& pivot,
                     SpriteBlendMode blendMode)
{
  if (rect.isEmpty()) {
    return;
  }

  SpriteQuad quad = makeQuad(rect, tint, rotation, pivot);
  quad.textureAndFlags = textureAndFlags;
  for (uint32 index = 0; index < 4; ++index) {
    quad.uvRect[index] = uvRect[index];
  }
  quad.blendMode = blendMode;
  nextDrawableKey(quad.sortingLayer, quad.order);
  m_draw.quad(quad);
}

/*
 */
void
UIDrawList::addText(Font& font,
                    const TextLayout& layout,
                    const Vector2& uiPivotPosition,
                    TextDrawDesc desc)
{
  if (layout.getGlyphs().empty()) {
    return;
  }
  desc.position = toWorldPosition(uiPivotPosition);
  nextDrawableKey(desc.sortingLayer, desc.order);
  m_draw.text(font, layout, desc);
}

/*
 */
void
UIDrawList::addGlyphRun(Font& font,
                        StringView text,
                        const Vector2& uiPivotPosition,
                        GlyphRunDesc desc)
{
  if (text.empty()) {
    return;
  }
  desc.position = toWorldPosition(uiPivotPosition);
  nextDrawableKey(desc.sortingLayer, desc.order);
  m_draw.glyphRun(font, text, desc);
}

} // namespace chEngineSDK
