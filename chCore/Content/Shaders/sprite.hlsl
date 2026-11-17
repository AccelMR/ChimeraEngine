// Sprites as instanced quads. There is no vertex buffer: each vertex reads its sprite from a
// storage buffer by the instance index and makes its corner from the vertex index.

#include "chSpriteBatch.hlsli"

// Must match GpuSpriteInstance in chSpriteItem.h.
static const uint kInstanceStride = 64;

// Must match SpritePushConstants in chSpritePass.cpp.
struct PushConstants
{
  CH_SPRITE_BATCH_HEADER
  uint instanceBufferIndex;
  // The draw always starts at instance 0, so where its run starts in the buffer comes here.
  uint firstInstance;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> g_push : register(b0);

SpriteVertexOutput
VSMain(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
  ByteAddressBuffer instances = ResourceDescriptorHeap[g_push.instanceBufferIndex];
  const uint address = (g_push.firstInstance + instanceId) * kInstanceStride;
  const float3 center = asfloat(instances.Load3(address));
  const uint textureAndFlags = instances.Load(address + 12);
  const float3 halfAxisU = asfloat(instances.Load3(address + 16));
  const uint colorRG = instances.Load(address + 28);
  const float3 halfAxisV = asfloat(instances.Load3(address + 32));
  const uint colorBA = instances.Load(address + 44);
  const float4 uvRect = asfloat(instances.Load4(address + 48));

  const float2 corner = kCorners[vertexId];
  // U runs to the right and V to the top of the quad, while v grows down the image.
  const float3 position =
      center + halfAxisU * (corner.x * 2.0f - 1.0f) + halfAxisV * (1.0f - corner.y * 2.0f);

  SpriteVertexOutput output;
  output.position = mul(float4(position, 1.0f), g_push.viewProjection);
  output.color = float4(unpackHalves(colorRG), unpackHalves(colorBA));
  output.texCoord = uvRect.xy + corner * uvRect.zw;
  output.textureAndFlags = textureAndFlags;
  return output;
}

float4
PSMain(SpriteVertexOutput input) : SV_Target0
{
  return shadeSprite(input.color,
                     input.texCoord,
                     input.textureAndFlags,
                     g_push.pointSamplerIndex,
                     g_push.linearSamplerIndex);
}
