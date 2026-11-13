// Glyph runs: each instance is a short text of up to kMaxGlyphs glyphs, and every six
// vertices of the draw make the quad of one of them. Vertices past the run's glyph count
// all land on one point, so their triangles are empty.

#include "chSpriteShading.hlsli"

// Must match GpuGlyphRun in chSpriteItem.h.
static const uint kRunStride = 128;
static const uint kMaxGlyphs = 16;
static const uint kCountMask = 0x1Fu;
static const float kPenScale = 1024.0f;
// Must match GpuGlyph in chFont.h.
static const uint kGlyphStride = 32;

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

struct GlyphRun
{
  float3 position;
  uint glyphTableIndex;
  uint textureAndFlags;
  float size;
  float2 offset;
  uint rotation;
  uint colorRG;
  uint colorBA;
  uint glyphCount;
  uint glyph;
  float pen;
};

template<typename Buffer>
GlyphRun
loadRun(Buffer runs, uint address, uint glyphIndex)
{
  GlyphRun run;
  run.position = asfloat(runs.Load3(address));
  run.glyphTableIndex = runs.Load(address + 12);
  run.textureAndFlags = runs.Load(address + 16);
  run.size = asfloat(runs.Load(address + 20));
  run.offset = asfloat(runs.Load2(address + 24));
  run.rotation = runs.Load(address + 32);
  run.colorRG = runs.Load(address + 36);
  run.colorBA = runs.Load(address + 40);
  run.glyphCount = min(runs.Load(address + 44) & kCountMask, kMaxGlyphs);
  // Two 16-bit values per word.
  const uint shift = (glyphIndex & 1) * 16;
  const uint word = (glyphIndex >> 1) * 4;
  run.glyph = (runs.Load(address + 64 + word) >> shift) & 0xFFFF;
  run.pen = (float)((runs.Load(address + 96 + word) >> shift) & 0xFFFF) / kPenScale;
  return run;
}

struct VSOutput
{
  float4 position : SV_Position;
  float4 color : COLOR0;
  float2 texCoord : TEXCOORD0;
  nointerpolation uint textureAndFlags : TEXCOORD1;
};

VSOutput
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

  VSOutput output = (VSOutput)0;
  if (glyphIndex >= run.glyphCount) {
    output.position = float4(0.0f, 0.0f, 0.0f, 1.0f);
    return output;
  }

  RWByteAddressBuffer glyphs = ResourceDescriptorHeap[run.glyphTableIndex];
  const uint glyphAddress = run.glyph * kGlyphStride;
  const float4 uvRect = asfloat(glyphs.Load4(glyphAddress));
  // Left and bottom from the pen, width and height, in font heights.
  const float4 box = asfloat(glyphs.Load4(glyphAddress + 16));

  // v grows down the image while y goes up.
  const float2 local = run.offset + float2(run.pen + box.x + corner.x * box.z,
                                           box.y + (1.0f - corner.y) * box.w);
  const float2 scaled = local * run.size;
  const float2 rotation = unpackHalves(run.rotation);
  // Right is +Y and up is +Z; the rotation is counterclockwise on screen.
  const float3 position =
      run.position + float3(0.0f,
                            scaled.x * rotation.x - scaled.y * rotation.y,
                            scaled.x * rotation.y + scaled.y * rotation.x);

  output.position = mul(float4(position, 1.0f), g_push.viewProjection);
  output.color = float4(unpackHalves(run.colorRG), unpackHalves(run.colorBA));
  output.texCoord = uvRect.xy + corner * uvRect.zw;
  output.textureAndFlags = run.textureAndFlags;
  return output;
}

float4
PSMain(VSOutput input) : SV_Target0
{
  return shadeSprite(input.color,
                     input.texCoord,
                     input.textureAndFlags,
                     g_push.pointSamplerIndex,
                     g_push.linearSamplerIndex);
}
