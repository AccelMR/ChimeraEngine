// Push constants of a plugin's sprite shader, drawn by a CustomSpriteBatch. The shader
// declares them as
//
//   struct PushConstants
//   {
//     CH_SPRITE_BATCH_HEADER
//     // Up to 56 bytes: CustomSpriteBatch::parameters, as the plugin wrote them.
//     uint runBufferIndex;
//     ...
//   };
//   [[vk::push_constant]] ConstantBuffer<PushConstants> g_push : register(b0);
//
// and reads textures through shadeSprite with the two samplers of the header.
#ifndef CH_SPRITE_BATCH_HLSLI
#define CH_SPRITE_BATCH_HLSLI

#include "chSpriteShading.hlsli"

// Must match CustomPushConstants in chSpritePass.cpp.
#define CH_SPRITE_BATCH_HEADER                                                                \
  row_major float4x4 viewProjection;                                                          \
  uint pointSamplerIndex;                                                                     \
  uint linearSamplerIndex;

#endif // CH_SPRITE_BATCH_HLSLI
