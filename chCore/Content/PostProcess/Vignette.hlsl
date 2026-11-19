// Darkens the edges of the image toward a color, as old lenses do.

#include "chPostProcess.hlsli"

// CH_PARAM float intensity 0.6 0 1 "How dark the corners get"
// CH_PARAM float radius 0.75 0 1.5 "Distance from the center where the darkening starts"
// CH_PARAM float softness 0.45 0.01 1 "How wide the change from clear to dark is"
// CH_PARAM color color 0 0 0 "The color the edges turn into"

float4
PSMain(PostProcessInput input) : SV_Target0
{
  const ShaderParams params = getParams();
  const float4 color = sampleSource(input.uv);
  // Round on any screen shape: the distance is measured in units of the image height.
  float2 centered = input.uv - 0.5f;
  centered.x *= g_postProcess.resolution.x * g_postProcess.inverseResolution.y;
  const float distance = length(centered) * 2.0f;
  const float edge = smoothstep(params.radius, params.radius + params.softness, distance);
  return float4(lerp(color.rgb, params.color.rgb, edge * params.intensity), color.a);
}
