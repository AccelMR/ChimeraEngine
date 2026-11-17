// An old CRT television: curved glass with black corners, colors split a little, scrolling
// scanlines, an aperture grille, a vignette and a faint flicker.

#include "chPostProcess.hlsli"

// CH_PARAM float curvature 1 0 3 "How round the glass is; 0 is flat"
// CH_PARAM float aberration 2 0 8 "Screen pixels between the red and blue channels"
// CH_PARAM float scanlines 0.15 0 1 "How dark the gaps between lines are"
// CH_PARAM float scanlineSpeed 8 -40 40 "Lines per second the pattern moves"
// CH_PARAM float mask 0.05 0 0.5 "Strength of the red, green and blue columns"
// CH_PARAM float vignette 1 0 2 "How dark the edges of the tube get"
// CH_PARAM float flicker 0.02 0 0.2 "Brightness change of the mains flicker"

// Bulges the coordinates outward from the center, like the glass of a tube.
float2
curve(float2 uv, float amount)
{
  uv = uv * 2.0f - 1.0f;
  const float2 offset = abs(uv.yx) / float2(5.0f, 4.0f);
  uv += uv * offset * offset * amount;
  return uv * 0.5f + 0.5f;
}

float4
PSMain(PostProcessInput input) : SV_Target0
{
  const ShaderParams params = getParams();
  const float2 uv = curve(input.uv, params.curvature);
  if (any(uv < 0.0f) || any(uv > 1.0f)) {
    return float4(0.0f, 0.0f, 0.0f, 1.0f);
  }

  const float split = params.aberration * g_postProcess.inverseResolution.x;
  float3 color;
  color.r = sampleSource(uv + float2(split, 0.0f)).r;
  color.g = sampleSource(uv).g;
  color.b = sampleSource(uv - float2(split, 0.0f)).b;

  const float time = g_postProcess.time;
  const float scan =
      sin((uv.y * g_postProcess.resolution.y * 0.5f + time * params.scanlineSpeed) * 3.14159f);
  color *= (1.0f - params.scanlines) + params.scanlines * scan * scan;

  const float column = fmod(input.position.x, 3.0f);
  const float high = 1.0f + params.mask;
  const float low = 1.0f - params.mask * 0.6f;
  color *= column < 1.0f ? float3(high, low, low)
         : column < 2.0f ? float3(low, high, low)
                         : float3(low, low, high);

  const float2 centered = uv - 0.5f;
  const float vignette = smoothstep(0.9f, 0.2f, dot(centered, centered) * 2.0f);
  color *= lerp(1.0f - 0.5f * params.vignette, 1.0f + 0.2f * params.vignette, vignette);

  color *= 1.0f - params.flicker + params.flicker * sin(time * 10.0f);
  return float4(saturate(color), 1.0f);
}
