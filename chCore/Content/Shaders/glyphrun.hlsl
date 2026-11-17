// Glyph runs as they are: each instance is a short text, and every six vertices of the draw
// make the quad of one of its glyphs (see chGlyphRun.hlsli).

#include "chGlyphRun.hlsli"

// Must match GlyphRunPushConstants in chSpritePass.cpp.
struct PushConstants
{
  row_major float4x4 viewProjection;
  uint runBufferIndex;
  uint firstRun;
  uint pointSamplerIndex;
  uint linearSamplerIndex;
  // Buffers made GpuOnly are bound as writable storage; the frame's own one is read only.
  uint bWritableBuffer;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> g_push : register(b0);

GlyphVSOutput
VSMain(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
  const uint glyphIndex = vertexId / 6;
  const float2 corner = kCorners[vertexId - glyphIndex * 6];
  const uint address = (g_push.firstRun + instanceId) * kRunStride;

  GlyphRun run;
  if (g_push.bWritableBuffer != 0) {
    RWByteAddressBuffer runs = ResourceDescriptorHeap[g_push.runBufferIndex];
    run = loadRun(runs, address, glyphIndex);
  }
  else {
    ByteAddressBuffer runs = ResourceDescriptorHeap[g_push.runBufferIndex];
    run = loadRun(runs, address, glyphIndex);
  }
  if (glyphIndex >= run.glyphCount) {
    return makeEmptyGlyphVertex();
  }
  return makeGlyphVertex(
      run, corner, run.position, 1.0f, getRunColor(run), g_push.viewProjection);
}

float4
PSMain(GlyphVSOutput input) : SV_Target0
{
  return shadeSprite(input.color,
                     input.texCoord,
                     input.textureAndFlags,
                     g_push.pointSamplerIndex,
                     g_push.linearSamplerIndex);
}
