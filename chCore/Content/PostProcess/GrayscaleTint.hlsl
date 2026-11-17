// Takes the color out of the image and gives it another, with brightness and contrast.

#include "chPostProcess.hlsli"

// CH_PARAM float amount 1 0 1 "1 is fully gray, 0 keeps the colors"
// CH_PARAM color tint 1 1 1 "Multiplies the result; a warm color gives sepia"
// CH_PARAM float brightness 0 -1 1
// CH_PARAM float contrast 1 0 3

float4
PSMain(PostProcessInput input) : SV_Target0
{
  const ShaderParams params = getParams();
  const float4 color = sampleSource(input.uv);
  // Rec. 709 weights; the image is display values after the tonemap, close enough for a look.
  const float gray = dot(color.rgb, float3(0.2126f, 0.7152f, 0.0722f));
  float3 result = lerp(color.rgb, gray.xxx, params.amount) * params.tint.rgb;
  result = (result - 0.5f) * params.contrast + 0.5f + params.brightness;
  return float4(saturate(result), color.a);
}
