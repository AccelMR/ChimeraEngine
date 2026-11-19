// CH_SHADER Sprite
// Turns the sprite into one color, keeping its shape: the flash of an enemy that was hit.
// Enemy.lua puts the material on for a moment and takes it off again.

#include "chSpriteMaterial.hlsli"

// CH_PARAM color flashColor 1 1 1 "The color the sprite turns into"
// CH_PARAM float amount 1 0 1 "1 is the flash color, 0 the sprite as it is"

float4
PSMain(SpriteInput input) : SV_Target0
{
  const ShaderParams params = getParams();
  const float4 color = shadeSprite(input);
  return float4(lerp(color.rgb, params.flashColor.rgb, params.amount), color.a);
}
