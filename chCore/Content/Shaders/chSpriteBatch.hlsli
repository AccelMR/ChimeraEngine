// Push constants of a plugin's sprite shader, drawn by a CustomSpriteBatch. The shader
// declares them as
//
//   struct PushConstants
//   {
//     CH_SPRITE_BATCH_HEADER
//     // Up to 32 bytes: CustomSpriteBatch::parameters, as the plugin wrote them.
//     uint runBufferIndex;
//     ...
//   };
//   [[vk::push_constant]] ConstantBuffer<PushConstants> g_push : register(b0);
//
// and reads textures through shadeSprite with the two samplers of the header. Sprites use
// the same header, so a sprite material's pixel shader (chSpriteMaterial.hlsli) finds its
// parameters and the time at the same place whichever vertex shader drew it.
#ifndef CH_SPRITE_BATCH_HLSLI
#define CH_SPRITE_BATCH_HLSLI

#include "chSpriteShading.hlsli"

// Must match SpriteBatchHeader in chSpritePass.cpp. 96 bytes, so what follows starts on 16
// bytes and lays out the same in C++ and every shader target.
#define CH_SPRITE_BATCH_HEADER                                                                \
  row_major float4x4 viewProjection;                                                          \
  uint pointSamplerIndex;                                                                     \
  uint linearSamplerIndex;                                                                    \
  uint materialParametersIndex;                                                               \
  uint materialParametersOffset;                                                              \
  float spriteTime;                                                                           \
  uint _headerPad0;                                                                           \
  uint _headerPad1;                                                                           \
  uint _headerPad2;

#endif // CH_SPRITE_BATCH_HLSLI
