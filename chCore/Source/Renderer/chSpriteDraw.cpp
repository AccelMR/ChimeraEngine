/************************************************************************/
/**
 * @file chSpriteDraw.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Sprites drawn for one frame without GameObjects.
 */
/************************************************************************/
#include "chSpriteDraw.h"

#include "chFloat16.h"
#include "chITexture.h"
#include "chLogger.h"
#include "chMath.h"
#include "chRenderSettings.h"
#include "chTextureAsset.h"

namespace chEngineSDK {

CH_LOG_DECLARE_STATIC(SpriteDrawLog, All);

namespace {
NODISCARD uint32
packHalves(float low, float high)
{
  return static_cast<uint32>(Float16::fromFloat(low)) |
         (static_cast<uint32>(Float16::fromFloat(high)) << 16);
}
} // namespace

/*
 */
void
SpriteDraw::quad(const SpriteQuad& quad)
{
  if (!hasRoom()) {
    return;
  }

  float sine = 0.0f;
  float cosine = 1.0f;
  Math::sinCos(quad.rotation.valueRadian(), sine, cosine);
  const float halfWidth = quad.size.x * 0.5f;
  const float halfHeight = quad.size.y * 0.5f;
  // The right side turns from +Y toward +Z, which is counterclockwise seen from the camera.
  const Vector3 halfU(0.0f, cosine * halfWidth, sine * halfWidth);
  const Vector3 halfV(0.0f, -sine * halfHeight, cosine * halfHeight);
  const Vector3 origin(quad.depth, quad.position.x, quad.position.y);

  SpriteItem& sprite = m_sprites.emplace_back();
  GpuSpriteInstance& gpu = sprite.instance;
  gpu.center =
      origin - halfU * (quad.pivot.x * 2.0f - 1.0f) - halfV * (quad.pivot.y * 2.0f - 1.0f);
  gpu.textureAndFlags = quad.textureAndFlags;
  gpu.halfAxisU = halfU;
  gpu.halfAxisV = halfV;
  packColor(quad.color, gpu.colorRG, gpu.colorBA);
  for (uint32 i = 0; i < 4; ++i) {
    gpu.uvRect[i] = quad.uvRect[i];
  }
  sprite.radius = Math::abs(halfWidth) + Math::abs(halfHeight);
  sprite.sortingLayer = quad.sortingLayer;
  sprite.order = quad.order;
  sprite.blendMode = quad.blendMode;
  sprite.bInUse = true;
}

/*
 */
void
SpriteDraw::instance(const GpuSpriteInstance& instance,
                     float radius,
                     int16 sortingLayer,
                     int16 order,
                     SpriteBlendMode blendMode)
{
  if (!hasRoom()) {
    return;
  }
  m_sprites.push_back({.instance = instance,
                       .radius = radius,
                       .sortingLayer = sortingLayer,
                       .order = order,
                       .blendMode = blendMode,
                       .bInUse = true});
}

/*
 */
void
SpriteDraw::clear() noexcept
{
  m_sprites.clear();
  m_bWarnedFull = false;
}

/*
 */
uint32
SpriteDraw::makeTextureFlags(const TextureAsset* texture, SpriteFilter filter)
{
  const ITexture* gpuTexture = texture ? texture->getTexture().get() : nullptr;
  const uint32 index =
      gpuTexture ? gpuTexture->getBindlessIndex() : GpuSpriteInstance::NO_TEXTURE;
  return index | (filter == SpriteFilter::Point ? GpuSpriteInstance::POINT_FILTER_BIT : 0u);
}

/*
 */
void
SpriteDraw::makeUvRect(const TextureAsset* texture,
                       const Vector2& sourcePosition,
                       const Vector2& sourceSize,
                       bool bFlipX,
                       bool bFlipY,
                       float (&outUvRect)[4])
{
  outUvRect[0] = 0.0f;
  outUvRect[1] = 0.0f;
  outUvRect[2] = 1.0f;
  outUvRect[3] = 1.0f;
  const uint32 textureWidth = texture ? texture->getWidth() : 0;
  const uint32 textureHeight = texture ? texture->getHeight() : 0;
  if (textureWidth > 0 && textureHeight > 0 && sourceSize.x > 0.0f && sourceSize.y > 0.0f) {
    const float width = static_cast<float>(textureWidth);
    const float height = static_cast<float>(textureHeight);
    outUvRect[0] = sourcePosition.x / width;
    outUvRect[1] = sourcePosition.y / height;
    outUvRect[2] = sourceSize.x / width;
    outUvRect[3] = sourceSize.y / height;
  }
  // Flipping reads the rectangle from its other edge, so the quad keeps its corners.
  if (bFlipX) {
    outUvRect[0] += outUvRect[2];
    outUvRect[2] = -outUvRect[2];
  }
  if (bFlipY) {
    outUvRect[1] += outUvRect[3];
    outUvRect[3] = -outUvRect[3];
  }
}

/*
 */
void
SpriteDraw::packColor(const LinearColor& color, uint32& outRG, uint32& outBA)
{
  outRG = packHalves(color.r, color.g);
  outBA = packHalves(color.b, color.a);
}

/*
 */
bool
SpriteDraw::hasRoom()
{
  const SIZE_T maxSprites = static_cast<SIZE_T>(Math::max(g_cvarSpriteDrawMax.get(), 1));
  if (m_sprites.size() < maxSprites) {
    return true;
  }
  if (!m_bWarnedFull) {
    m_bWarnedFull = true;
    CH_LOG_WARNING(SpriteDrawLog,
                   "More than {0} sprites between two clears; the rest are dropped "
                   "(Renderer.SpriteDrawMax).",
                   maxSprites);
  }
  return false;
}

} // namespace chEngineSDK
