// CH_SHADER Sprite
// Eats the sprite away with noise, a glowing edge around what is left. The alpha of the
// sprite's color is how much is left (1 whole, 0 gone), so every sprite sharing the material
// dissolves at its own pace: Enemy.lua lowers it when the enemy dies.

#include "chSpriteMaterial.hlsli"

// CH_PARAM color edgeColor 1 0.55 0.15 "Glow at the edge of what is left"
// CH_PARAM float edgeWidth 0.15 0 0.5 "How wide the glowing edge is"
// CH_PARAM float noiseScale 40 1 200 "How small the holes are"

float
hash(float2 p)
{
  return frac(sin(dot(p, float2(127.1f, 311.7f))) * 43758.5453f);
}

// Smooth value noise between 0 and 1.
float
valueNoise(float2 p)
{
  const float2 cell = floor(p);
  const float2 f = frac(p);
  const float2 u = f * f * (3.0f - 2.0f * f);
  const float bottom = lerp(hash(cell), hash(cell + float2(1.0f, 0.0f)), u.x);
  const float top = lerp(hash(cell + float2(0.0f, 1.0f)), hash(cell + float2(1.0f, 1.0f)), u.x);
  return lerp(bottom, top, u.y);
}

float4
PSMain(SpriteInput input) : SV_Target0
{
  const ShaderParams params = getParams();
  const float4 texel = sampleSpriteTexture(input, input.texCoord);
  // Stretched by the edge width, so a whole sprite shows no glow and an empty one nothing.
  const float threshold = input.color.a * (1.0f + params.edgeWidth);
  const float noise = valueNoise(input.texCoord * params.noiseScale);
  if (noise > threshold) {
    return float4(0.0f, 0.0f, 0.0f, 0.0f);
  }
  // Near the edge of what is left the sprite turns into the glow color.
  const float edge = params.edgeWidth > 0.0f
                         ? smoothstep(threshold - params.edgeWidth, threshold, noise)
                         : 0.0f;
  const float3 color = lerp(texel.rgb * input.color.rgb, params.edgeColor.rgb, edge);
  return float4(color, texel.a);
}
