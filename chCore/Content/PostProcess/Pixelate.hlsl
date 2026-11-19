// Draws the image in big square pixels.

#include "chPostProcess.hlsli"

// CH_PARAM int pixelSize 6 1 64 "Size of each big pixel, in screen pixels"

float4
PSMain(PostProcessInput input) : SV_Target0
{
  const ShaderParams params = getParams();
  const float size = max(1.0f, (float)params.pixelSize);
  // The center of the block this pixel is in.
  const float2 block = (floor(input.position.xy / size) + 0.5f) * size;
  return loadSource(int2(block));
}
