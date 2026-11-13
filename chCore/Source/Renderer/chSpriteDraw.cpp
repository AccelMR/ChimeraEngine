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
#include "chFont.h"
#include "chITexture.h"
#include "chLogger.h"
#include "chMath.h"
#include "chRenderSettings.h"
#include "chTextLayout.h"
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

NODISCARD uint32
scaleHighHalf(uint32 packed, float factor)
{
  const float high = Float16::toFloat(static_cast<uint16>(packed >> 16));
  return (packed & 0xFFFFu) | (static_cast<uint32>(Float16::fromFloat(high * factor)) << 16);
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
SpriteDraw::text(Font& font, const TextLayout& layout, const TextDrawDesc& desc)
{
  // A text is drawn whole or not at all, so a full list never shows half a word.
  const SIZE_T passes = 1 + (desc.outlineWidth > 0.0f ? 1 : 0) + (desc.shadowColor.a > 0.0f ? 1 : 0);
  if (!hasRoom(layout.getGlyphs().size() * passes)) {
    return;
  }
  appendTextSprites(font, layout, desc, m_sprites);
}

/*
 */
void
SpriteDraw::appendTextSprites(Font& font,
                              const TextLayout& layout,
                              const TextDrawDesc& desc,
                              Vector<SpriteItem>& outSprites)
{
  const Span<const TextGlyph> glyphs = layout.getGlyphs();
  if (glyphs.empty() || !font.isValid()) {
    return;
  }

  const uint32 atlasIndex = font.prepareGpuAtlas();
  const FontSettings& settings = font.getSettings();
  const bool bSdf = settings.mode == FontRenderMode::Sdf;
  const SpriteTextureMode mode = bSdf ? SpriteTextureMode::Sdf : SpriteTextureMode::Coverage;
  // The field falls by 128 over sdfSpread pixels, in 1/255 steps.
  const uint32 outlineEdge =
      bSdf ? static_cast<uint32>(Math::clamp(desc.outlineWidth * 128.0f /
                                                 static_cast<float>(settings.sdfSpread),
                                             0.0f,
                                             static_cast<float>(GpuSpriteInstance::EDGE_MASK)) +
                                 0.5f)
           : 0u;
  const uint32 fillFlags = GpuSpriteInstance::makeTextureFlags(atlasIndex, !bSdf, mode);
  const uint32 outlineFlags =
      GpuSpriteInstance::makeTextureFlags(atlasIndex, !bSdf, mode, outlineEdge);
  const bool bOutline = outlineEdge > 0 && desc.outlineColor.a * desc.opacity > 0.0f;
  const bool bShadow = desc.shadowColor.a * desc.opacity > 0.0f;

  float sine = 0.0f;
  float cosine = 1.0f;
  Math::sinCos(desc.rotation.valueRadian(), sine, cosine);
  const Vector3 axisU(0.0f, cosine, sine);
  const Vector3 axisV(0.0f, -sine, cosine);
  const Vector3 origin(desc.depth, desc.position.x, desc.position.y);
  const float inverseWidth = 1.0f / static_cast<float>(font.getAtlasWidth());
  const float inverseHeight = 1.0f / static_cast<float>(font.getAtlasHeight());

  const auto addPass = [&](const Vector2& offset,
                           uint32 textureAndFlags,
                           const LinearColor* passColor) {
    const Vector3 shift(0.0f, offset.x, offset.y);
    uint32 colorRG = 0;
    uint32 colorBA = 0;
    if (passColor != nullptr) {
      LinearColor color = *passColor;
      color.a *= desc.opacity;
      packColor(color, colorRG, colorBA);
    }
    for (const TextGlyph& glyph : glyphs) {
      const FontGlyph& image = font.getGlyph(glyph.slot);
      const Vector2 size = glyph.size * desc.scale;
      const Vector2 center = glyph.position * desc.scale + size * 0.5f;

      SpriteItem& sprite = outSprites.emplace_back();
      GpuSpriteInstance& gpu = sprite.instance;
      gpu.center = origin + axisU * center.x + axisV * center.y + shift;
      gpu.textureAndFlags = textureAndFlags;
      gpu.halfAxisU = axisU * (size.x * 0.5f);
      gpu.halfAxisV = axisV * (size.y * 0.5f);
      if (passColor != nullptr) {
        gpu.colorRG = colorRG;
        gpu.colorBA = colorBA;
      }
      else {
        gpu.colorRG = glyph.colorRG;
        gpu.colorBA = desc.opacity == 1.0f ? glyph.colorBA : scaleHighHalf(glyph.colorBA, desc.opacity);
      }
      gpu.uvRect[0] = static_cast<float>(image.atlasX) * inverseWidth;
      gpu.uvRect[1] = static_cast<float>(image.atlasY) * inverseHeight;
      gpu.uvRect[2] = static_cast<float>(image.width) * inverseWidth;
      gpu.uvRect[3] = static_cast<float>(image.height) * inverseHeight;
      sprite.radius = (Math::abs(size.x) + Math::abs(size.y)) * 0.5f;
      sprite.sortingLayer = desc.sortingLayer;
      sprite.order = desc.order;
      sprite.blendMode = desc.blendMode;
      sprite.bInUse = true;
    }
  };

  // Same layer, order and depth, so the sort keeps them in this order: shadow at the back.
  if (bShadow) {
    addPass(desc.shadowOffset, bOutline ? outlineFlags : fillFlags, &desc.shadowColor);
  }
  if (bOutline) {
    addPass(Vector2::ZERO, outlineFlags, &desc.outlineColor);
  }
  addPass(Vector2::ZERO, fillFlags, nullptr);
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
  return GpuSpriteInstance::makeTextureFlags(index, filter == SpriteFilter::Point);
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
SpriteDraw::hasRoom(SIZE_T count)
{
  const SIZE_T maxSprites = static_cast<SIZE_T>(Math::max(g_cvarSpriteDrawMax.get(), 1));
  if (m_sprites.size() + count <= maxSprites) {
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
