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
 * Exists so the renderer can upload the visible sprites as they are, without converting
 * them: one quad in world space, 64 bytes. Must match the layout read by sprite.hlsl.
 */
struct GpuSpriteInstance
{
  // Bit set in textureAndFlags when the texture is read with the Point filter.
  static constexpr uint32 POINT_FILTER_BIT = 0x80000000u;
  // The bindless index part of textureAndFlags when the sprite has no texture.
  static constexpr uint32 NO_TEXTURE = 0x7FFFFFFFu;

  Vector3 center;
  // Bindless index of the texture (NO_TEXTURE draws the color alone) plus POINT_FILTER_BIT.
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
