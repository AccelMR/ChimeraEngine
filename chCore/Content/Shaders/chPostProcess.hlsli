// What a project's post-process shader includes. The shader writes
//
//   #include "chPostProcess.hlsli"
//
//   // CH_PARAM float intensity 1.0 0 2 "How strong the effect is"
//
//   float4
//   PSMain(PostProcessInput input) : SV_Target0
//   {
//     const ShaderParams params = getParams();
//     float4 color = sampleSource(input.uv);
//     ...
//   }
//
// The shader codec reads the CH_PARAM lines (types float, float2, float3, float4, color, int,
// bool; default, then minimum and maximum for float and int, then a tooltip in quotes) and
// makes chShaderParams.hlsli with the ShaderParams struct. A "// CH_STAGE BeforeTonemap" line
// runs the pass on the linear HDR scene color instead of the final image. A shader without
// VSMain is drawn with postProcessVertex.
#ifndef CH_POST_PROCESS_HLSLI
#define CH_POST_PROCESS_HLSLI

#include "chBindless.hlsli"
#include "chFullscreen.hlsli"
#include "chShaderParams.hlsli"

// Must match PostProcessPushConstants in the engine.
struct PostProcessPushConstants
{
  // Texture2D<float4> the pass reads: the scene, or what the pass before it wrote.
  uint sourceIndex;
  // Constant buffer holding ShaderParams.
  uint parametersIndex;
  uint linearSamplerIndex;
  uint pointSamplerIndex;
  // Size of the source and the output in pixels, and one over it.
  float2 resolution;
  float2 inverseResolution;
  // Seconds since the game started, and since the last frame.
  float time;
  float deltaTime;
  // How much of the effect shows (0 to 1); the engine blends the result with the source.
  float weight;
  uint frameIndex;
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

ShaderParams
getParams()
{
  return loadConstants<ShaderParams>(g_postProcess.parametersIndex);
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

#endif // CH_POST_PROCESS_HLSLI
