/************************************************************************/
/**
 * @file chSpriteDraw.h
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Sprites drawn for one frame without GameObjects.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chAngle.h"
#include "chLinearColor.h"
#include "chSpriteItem.h"
#include "chVector2.h"

namespace chEngineSDK {
class TextureAsset;

/**
 * One quad in the YZ plane (right is +Y, up is +Z), as SpriteDraw::quad takes it. The
 * texture part comes already resolved (SpriteDraw::makeTextureFlags and makeUvRect), so a
 * system that draws many quads of one look computes it once.
 */
struct SpriteQuad
{
  // Y and Z of the point the pivot stands on.
  Vector2 position = Vector2::ZERO;
  // World X; sprites farther along the camera's forward axis draw first within an order.
  float depth = 0.0f;
  // Width and height in world units.
  Vector2 size{1.0f, 1.0f};
  // Counterclockwise on screen.
  Radian rotation = Radian(0.0f);
  // (0, 0) is the bottom left corner, (1, 1) the top right one.
  Vector2 pivot{0.5f, 0.5f};
  LinearColor color = LinearColor::WHITE;
  uint32 textureAndFlags = GpuSpriteInstance::NO_TEXTURE;
  float uvRect[4] = {0.0f, 0.0f, 1.0f, 1.0f};
  int16 sortingLayer = 0;
  int16 order = 0;
  SpriteBlendMode blendMode = SpriteBlendMode::Alpha;
};

/**
 * Exists so systems that draw many short-lived images (bullets, particles, effects) need no
 * GameObject per image: they add sprites every frame and the renderer sorts them with the
 * sprite components, by layer and order. Each Scene has one, cleared when the scene
 * updates, and one for the fixed step, cleared when a fixed step starts, so what a fixed
 * step adds shows on every frame until the next one. The sprites are kept in a vector that
 * keeps its memory after clear(), so adding the same number every frame does not allocate.
 * Main thread only.
 *
 * Sprites added here are not picked by Scene::raycast: they have no object to give back.
 */
class CH_CORE_EXPORT SpriteDraw
{
 public:
  void
  quad(const SpriteQuad& quad);

  /**
   * The fastest way, for callers that build the instance themselves (see sprite.hlsl for
   * what each field means). radius is the distance from the center that holds every
   * corner.
   */
  void
  instance(const GpuSpriteInstance& instance,
           float radius,
           int16 sortingLayer = 0,
           int16 order = 0,
           SpriteBlendMode blendMode = SpriteBlendMode::Alpha);

  /**
   * Forgets every sprite but keeps the memory.
   */
  void
  clear() noexcept;

  NODISCARD FORCEINLINE Span<const SpriteItem>
  getSprites() const noexcept
  {
    return m_sprites;
  }

  /**
   * Bindless index of the texture's GPU copy (NO_TEXTURE without one) plus the filter bit.
   */
  NODISCARD static uint32
  makeTextureFlags(const TextureAsset* texture, SpriteFilter filter);

  /**
   * The part of the texture a source rectangle in pixels shows, as sprite.hlsl reads it. A
   * zero size, or a texture without a size yet, gives the whole texture.
   */
  static void
  makeUvRect(const TextureAsset* texture,
             const Vector2& sourcePosition,
             const Vector2& sourceSize,
             bool bFlipX,
             bool bFlipY,
             float (&outUvRect)[4]);

  /**
   * The color as the two words of halves GpuSpriteInstance keeps.
   */
  static void
  packColor(const LinearColor& color, uint32& outRG, uint32& outBA);

 private:
  NODISCARD bool
  hasRoom();

  Vector<SpriteItem> m_sprites;
  bool m_bWarnedFull = false;
};

} // namespace chEngineSDK
