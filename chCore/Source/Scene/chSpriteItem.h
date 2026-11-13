/************************************************************************/
/**
 * @file chSpriteItem.h
 * @author AccelMR
 * @date 2026/11/12
 * @brief
 *  A sprite as the renderer sees it, kept in a flat list by the Scene.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chVector3.h"

namespace chEngineSDK {

/**
 * How a sprite mixes with what is drawn before it. Saved in scenes, so the values are fixed.
 */
enum class SpriteBlendMode : uint8
{
  // color * alpha + below * (1 - alpha)
  Alpha = 0,
  // color * alpha + below, for glows and sparks.
  Additive = 1,
  COUNT
};

/**
 * How a sprite's texture is read. Saved in scenes, so the values are fixed.
 */
enum class SpriteFilter : uint8
{
  // Nearest texel, for pixel art.
  Point = 0,
  Linear = 1,
  COUNT
};

/**
 * How sprite.hlsl reads a sprite's texture, kept in GpuSpriteInstance::textureAndFlags.
 */
enum class SpriteTextureMode : uint32
{
  // The texture's color times the sprite's.
  Color = 0,
  // The red channel is the coverage: the sprite's color with that alpha (pixel fonts).
  Coverage = 1,
  // The red channel is a signed distance field whose edge is at 0.5 (text).
  Sdf = 2
};

/**
 * Exists so the renderer can upload the visible sprites as they are, without converting
 * them: one quad in world space, 64 bytes. Must match the layout read by sprite.hlsl.
 */
struct GpuSpriteInstance
{
  // Bit set in textureAndFlags when the texture is read with the Point filter.
  static constexpr uint32 POINT_FILTER_BIT = 0x80000000u;
  // The bindless index part of textureAndFlags, and its value when there is no texture.
  static constexpr uint32 TEXTURE_INDEX_MASK = 0x000FFFFFu;
  static constexpr uint32 NO_TEXTURE = TEXTURE_INDEX_MASK;
  static constexpr uint32 MODE_SHIFT = 20;
  static constexpr uint32 MODE_MASK = 0x3u;
  // SDF only: how far out the edge moves, in 1/255 of the field (outlines, bold).
  static constexpr uint32 EDGE_SHIFT = 22;
  static constexpr uint32 EDGE_MASK = 0xFFu;

  NODISCARD static constexpr uint32
  makeTextureFlags(uint32 bindlessIndex,
                   bool bPointFilter,
                   SpriteTextureMode mode = SpriteTextureMode::Color,
                   uint32 edgeOffset = 0) noexcept
  {
    return (bindlessIndex & TEXTURE_INDEX_MASK) |
           ((static_cast<uint32>(mode) & MODE_MASK) << MODE_SHIFT) |
           ((edgeOffset & EDGE_MASK) << EDGE_SHIFT) | (bPointFilter ? POINT_FILTER_BIT : 0u);
  }

  Vector3 center;
  // Bindless index of the texture (NO_TEXTURE draws the color alone), its SpriteTextureMode,
  // the SDF edge offset and POINT_FILTER_BIT; see makeTextureFlags.
  uint32 textureAndFlags;
  // Half the width of the quad, along its right side.
  Vector3 halfAxisU;
  // Red and green of the color as halves.
  uint32 colorRG;
  // Half the height of the quad, along its top side.
  Vector3 halfAxisV;
  // Blue and alpha of the color as halves.
  uint32 colorBA;
  // Left, top, width and height of the part of the texture shown, in 0..1 with v = 0 at
  // the top of the image. A negative width or height flips the image.
  float uvRect[4];
};
static_assert(sizeof(GpuSpriteInstance) == 64, "GpuSpriteInstance must match sprite.hlsl");

/**
 * Exists so the renderer reads sprites from a flat array instead of walking GameObjects and
 * components. Sprite components add one to their Scene and keep it up to date.
 */
struct SpriteItem
{
  GpuSpriteInstance instance{};
  // Distance from the center that holds every corner, for culling.
  float radius = 0.0f;
  // Sprites draw by layer, then by order inside a layer, lowest first.
  int16 sortingLayer = 0;
  int16 order = 0;
  SpriteBlendMode blendMode = SpriteBlendMode::Alpha;
  // The object the sprite belongs to, so the editor can find it from the sprite.
  GameObject* owner = nullptr;
  // False marks a free slot of the scene.
  bool bInUse = false;
};

} // namespace chEngineSDK
