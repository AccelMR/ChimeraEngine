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
// VSMain is drawn with postProcessVertex. Its weight is applied by the engine after it.
#ifndef CH_POST_PROCESS_HLSLI
#define CH_POST_PROCESS_HLSLI

#include "chPostProcessCommon.hlsli"
#include "chShaderParams.hlsli"

ShaderParams
getParams()
{
  ByteAddressBuffer parameters = ResourceDescriptorHeap[g_postProcess.parametersIndex];
  return parameters.Load<ShaderParams>(g_postProcess.parametersOffset);
}

#endif // CH_POST_PROCESS_HLSLI
