// What every post-process shader shares, also the engine's own (postprocess.hlsl,
// postprocessblend.hlsl), which have no parameter struct. A project's shader includes
// chPostProcess.hlsli instead.
#ifndef CH_POST_PROCESS_COMMON_HLSLI
#define CH_POST_PROCESS_COMMON_HLSLI

#include "chBindless.hlsli"
#include "chFullscreen.hlsli"

// Must match PostProcessPushConstants in chPostProcessRenderer.cpp.
struct PostProcessPushConstants
{
  // Texture2D<float4> the pass reads: the scene, or what the pass before it wrote.
  uint sourceIndex;
  // ByteAddressBuffer holding the ShaderParams of every pass of the frame.
  uint parametersIndex;
  // Where this pass's ShaderParams start in it, in bytes.
  uint parametersOffset;
  uint linearSamplerIndex;
  uint pointSamplerIndex;
  uint frameIndex;
  // Seconds since the renderer started, and since its last frame.
  float time;
  float deltaTime;
  // Size of the source and the output in pixels, and one over it.
  float2 resolution;
  float2 inverseResolution;
};

[[vk::push_constant]] ConstantBuffer<PostProcessPushConstants> g_postProcess : register(b0);

struct PostProcessInput
{
  float4 position : SV_Position;
  // (0, 0) is the top left corner of the image.
  float2 uv : TEXCOORD0;
};

PostProcessInput
postProcessVertex(uint vertexId)
{
  PostProcessInput output;
  output.position = fullscreenTrianglePosition(vertexId);
  output.uv = float2(output.position.x * 0.5f + 0.5f, 0.5f - output.position.y * 0.5f);
  return output;
}

float4
sampleSource(float2 uv)
{
  Texture2D<float4> source = ResourceDescriptorHeap[g_postProcess.sourceIndex];
  SamplerState linearSampler = SamplerDescriptorHeap[g_postProcess.linearSamplerIndex];
  return source.SampleLevel(linearSampler, uv, 0.0f);
}

float4
sampleSourcePoint(float2 uv)
{
  Texture2D<float4> source = ResourceDescriptorHeap[g_postProcess.sourceIndex];
  SamplerState pointSampler = SamplerDescriptorHeap[g_postProcess.pointSamplerIndex];
  return source.SampleLevel(pointSampler, uv, 0.0f);
}

// One texel by its pixel position, clamped to the image.
float4
loadSource(int2 pixel)
{
  Texture2D<float4> source = ResourceDescriptorHeap[g_postProcess.sourceIndex];
  const int2 last = int2(g_postProcess.resolution) - 1;
  return source.Load(int3(clamp(pixel, int2(0, 0), last), 0));
}

#endif // CH_POST_PROCESS_COMMON_HLSLI
