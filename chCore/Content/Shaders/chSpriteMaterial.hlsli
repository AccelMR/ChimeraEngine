// What a project's sprite material includes. The shader writes
//
//   // CH_SHADER Sprite
//   #include "chSpriteMaterial.hlsli"
//
//   // CH_PARAM color flashColor 1 1 1 "What the sprite turns into"
//
//   float4
//   PSMain(SpriteInput input) : SV_Target0
//   {
//     const ShaderParams params = getParams();
//     const float4 color = shadeSprite(input);
//     ...
//   }
//
// The engine draws it with its own vertex shader, the same for sprites, projectiles and
// particles. input.color is the sprite's color: a material may read it as data of its own
// (a dissolve amount in its alpha), since it is the one value every sprite carries.
#ifndef CH_SPRITE_MATERIAL_HLSLI
#define CH_SPRITE_MATERIAL_HLSLI

#include "chBindless.hlsli"
#include "chSpriteBatch.hlsli"
#include "chShaderParams.hlsli"

struct SpriteMaterialPushConstants
{
  CH_SPRITE_BATCH_HEADER
};

[[vk::push_constant]] ConstantBuffer<SpriteMaterialPushConstants> g_spriteMaterial
    : register(b0);

typedef SpriteVertexOutput SpriteInput;

ShaderParams
getParams()
{
  ByteAddressBuffer parameters = ResourceDescriptorHeap[g_spriteMaterial.materialParametersIndex];
  return parameters.Load<ShaderParams>(g_spriteMaterial.materialParametersOffset);
}

// Seconds since the renderer started.
float
getTime()
{
  return g_spriteMaterial.spriteTime;
}

// The sprite as the engine draws it: its texture (or font) times its color.
float4
shadeSprite(SpriteInput input)
{
  return shadeSprite(input.color,
                     input.texCoord,
                     input.textureAndFlags,
                     g_spriteMaterial.pointSamplerIndex,
                     g_spriteMaterial.linearSamplerIndex);
}

// Same at other texture coordinates (waves, outlines, pixelation of the sprite).
float4
shadeSpriteAt(SpriteInput input, float2 texCoord)
{
  return shadeSprite(input.color,
                     texCoord,
                     input.textureAndFlags,
                     g_spriteMaterial.pointSamplerIndex,
                     g_spriteMaterial.linearSamplerIndex);
}

// The texture alone, without the sprite's color.
float4
sampleSpriteTexture(SpriteInput input, float2 texCoord)
{
  return shadeSprite(float4(1.0f, 1.0f, 1.0f, 1.0f),
                     texCoord,
                     input.textureAndFlags,
                     g_spriteMaterial.pointSamplerIndex,
                     g_spriteMaterial.linearSamplerIndex);
}

#endif // CH_SPRITE_MATERIAL_HLSLI
