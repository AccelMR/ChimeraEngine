// How sprites and glyph runs read their texture, from GpuSpriteInstance::textureAndFlags.
#ifndef CH_SPRITE_SHADING_HLSLI
#define CH_SPRITE_SHADING_HLSLI

// Must match GpuSpriteInstance in chSpriteItem.h.
static const uint kPointFilterBit = 0x80000000u;
static const uint kTextureIndexMask = 0x000FFFFFu;
static const uint kNoTexture = 0x000FFFFFu;
static const uint kModeShift = 20;
static const uint kModeMask = 0x3u;
static const uint kEdgeShift = 22;
static const uint kEdgeMask = 0xFFu;
// SpriteTextureMode
static const uint kModeCoverage = 1;
static const uint kModeSdf = 2;
// Where stb writes the edge of a distance field.
static const float kSdfEdge = 128.0f / 255.0f;

// Two triangles, as (u, v) with v = 0 at the top of the image.
static const float2 kCorners[6] = {float2(0.0f, 0.0f), float2(1.0f, 0.0f), float2(0.0f, 1.0f),
                                   float2(0.0f, 1.0f), float2(1.0f, 0.0f), float2(1.0f, 1.0f)};

float2
unpackHalves(uint packed)
{
  return float2(f16tof32(packed & 0xFFFF), f16tof32(packed >> 16));
}

float4
shadeSprite(float4 color,
            float2 texCoord,
            uint textureAndFlags,
            uint pointSamplerIndex,
            uint linearSamplerIndex)
{
  const uint textureIndex = textureAndFlags & kTextureIndexMask;
  if (textureIndex == kNoTexture) {
    return color;
  }
  // Sprites of one draw read different textures, so the indexes are not uniform.
  const uint samplerIndex =
      (textureAndFlags & kPointFilterBit) != 0 ? pointSamplerIndex : linearSamplerIndex;
  Texture2D spriteTexture = ResourceDescriptorHeap[NonUniformResourceIndex(textureIndex)];
  SamplerState spriteSampler = SamplerDescriptorHeap[NonUniformResourceIndex(samplerIndex)];
  const float4 texel = spriteTexture.Sample(spriteSampler, texCoord);
  // Taken here, where every pixel of the quad runs, so the derivative is defined.
  const float fieldWidth = max(fwidth(texel.r), 1e-5f) * 0.5f;

  const uint mode = (textureAndFlags >> kModeShift) & kModeMask;
  if (mode == kModeCoverage) {
    return float4(color.rgb, color.a * texel.r);
  }
  if (mode == kModeSdf) {
    const float edgeOffset = (float)((textureAndFlags >> kEdgeShift) & kEdgeMask);
    const float edge = kSdfEdge - edgeOffset / 255.0f;
    const float coverage = smoothstep(edge - fieldWidth, edge + fieldWidth, texel.r);
    return float4(color.rgb, color.a * coverage);
  }
  return texel * color;
}

#endif // CH_SPRITE_SHADING_HLSLI
