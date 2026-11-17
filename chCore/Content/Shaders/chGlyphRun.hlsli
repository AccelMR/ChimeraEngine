// Glyph runs (GpuGlyphRun): each instance is a short text of up to kMaxGlyphs glyphs, and
// every six vertices of a draw make the quad of one of them. Shared by glyphrun.hlsl and by
// plugin shaders that move runs on their own.
#ifndef CH_GLYPH_RUN_HLSLI
#define CH_GLYPH_RUN_HLSLI

#include "chSpriteShading.hlsli"

// Must match GpuGlyphRun in chSpriteItem.h.
static const uint kRunStride = 128;
static const uint kMaxGlyphs = 16;
static const uint kCountMask = 0x1Fu;
static const float kPenScale = 1024.0f;
// Must match GpuGlyph in chFont.h.
static const uint kGlyphStride = 32;

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
  // The glyph slot and pen of the glyph the vertex belongs to.
  uint glyph;
  float pen;
  uint4 userData;
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
  run.userData = runs.Load4(address + 48);
  return run;
}

float4
getRunColor(GlyphRun run)
{
  return float4(unpackHalves(run.colorRG), unpackHalves(run.colorBA));
}

struct GlyphVSOutput
{
  float4 position : SV_Position;
  float4 color : COLOR0;
  float2 texCoord : TEXCOORD0;
  nointerpolation uint textureAndFlags : TEXCOORD1;
};

// A vertex that draws nothing: every vertex of a quad lands on one point.
GlyphVSOutput
makeEmptyGlyphVertex()
{
  GlyphVSOutput output = (GlyphVSOutput)0;
  output.position = float4(0.0f, 0.0f, 0.0f, 1.0f);
  return output;
}

// The vertex at corner of the run's glyph, with the run's pivot at origin, its size times
// scale and the given color.
GlyphVSOutput
makeGlyphVertex(GlyphRun run,
                float2 corner,
                float3 origin,
                float scale,
                float4 color,
                float4x4 viewProjection)
{
  RWByteAddressBuffer glyphs = ResourceDescriptorHeap[run.glyphTableIndex];
  const uint glyphAddress = run.glyph * kGlyphStride;
  const float4 uvRect = asfloat(glyphs.Load4(glyphAddress));
  // Left and bottom from the pen, width and height, in font heights.
  const float4 box = asfloat(glyphs.Load4(glyphAddress + 16));

  // v grows down the image while y goes up.
  const float2 local = run.offset + float2(run.pen + box.x + corner.x * box.z,
                                           box.y + (1.0f - corner.y) * box.w);
  const float2 scaled = local * run.size * scale;
  const float2 rotation = unpackHalves(run.rotation);
  // Right is +Y and up is +Z; the rotation is counterclockwise on screen.
  const float3 position =
      origin + float3(0.0f,
                      scaled.x * rotation.x - scaled.y * rotation.y,
                      scaled.x * rotation.y + scaled.y * rotation.x);

  GlyphVSOutput output;
  output.position = mul(float4(position, 1.0f), viewProjection);
  output.color = color;
  output.texCoord = uvRect.xy + corner * uvRect.zw;
  output.textureAndFlags = run.textureAndFlags;
  return output;
}

#endif // CH_GLYPH_RUN_HLSLI
