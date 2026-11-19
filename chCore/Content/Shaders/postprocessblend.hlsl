// Blends a post-process pass's result with the image it read, for passes whose weight is
// below 1, so their shaders need not know about the weight.

#include "chFullscreen.hlsli"

// Must match BlendPushConstants in chPostProcessRenderer.cpp.
struct PushConstants
{
  uint sourceIndex;
  uint effectIndex;
  float weight;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> g_push : register(b0);

float4
VSMain(uint vertexId : SV_VertexID) : SV_Position
{
  return fullscreenTrianglePosition(vertexId);
}

float4
PSMain(float4 position : SV_Position) : SV_Target0
{
  // Every texture of the chain has the output's size, so each pixel reads its own texel.
  Texture2D<float4> source = ResourceDescriptorHeap[g_push.sourceIndex];
  Texture2D<float4> effect = ResourceDescriptorHeap[g_push.effectIndex];
  const int3 texel = int3(position.xy, 0);
  return lerp(source.Load(texel), effect.Load(texel), g_push.weight);
}
