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
// Must match GlyphRunBatch::NO_MOTION, GpuGlyphRun's motion bits and GpuGlyphRunMotion.
static const uint kNoMotion = 0xFFFFFFFFu;
static const uint kMotionTierMask = 0xFFu;
static const uint kMotionOutlineBit = 0x100u;
static const uint kMotionSeedShift = 16;
static const uint kMotionHeaderSize = 32;
static const uint kTierStride = 80;
static const float kTwoPi = 6.28318530718f;

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
  // A GpuGlyphRunMotion, or kNoMotion; the time the runs' ages are taken from.
  uint motionIndex;
  float time;
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

  float4 color = float4(unpackHalves(run.colorRG), unpackHalves(run.colorBA));
  float3 origin = run.position;
  float scale = 1.0f;
  if (g_push.motionIndex != kNoMotion) {
    RWByteAddressBuffer motion = ResourceDescriptorHeap[g_push.motionIndex];
    const float age = g_push.time - asfloat(run.userData.x);
    const float lifetime = asfloat(run.userData.z);
    if (age < 0.0f || age >= lifetime) {
      output.position = float4(0.0f, 0.0f, 0.0f, 1.0f);
      return output;
    }
    const float4 header = asfloat(motion.Load4(0));
    const float4 header2 = asfloat(motion.Load4(16));
    const float2 gravity = header.xy;
    const float drag = header.z;
    const float fadeIn = header.w;
    const float fadeOut = header2.x;
    const uint tierCount = asuint(header2.y);
    const uint tierIndex = min(run.userData.w & kMotionTierMask, max(tierCount, 1u) - 1u);
    const uint tierAddress = kMotionHeaderSize + tierIndex * kTierStride;
    const float4 tierColor = asfloat(motion.Load4(tierAddress));
    const float4 flashColor = asfloat(motion.Load4(tierAddress + 16));
    const float4 outlineColor = asfloat(motion.Load4(tierAddress + 32));
    // scale, popScale, popDuration, shakeAmplitude
    const float4 sizeAndShake = asfloat(motion.Load4(tierAddress + 48));
    // shakeFrequency, shakeDuration, flashDuration, flashRate
    const float4 timing = asfloat(motion.Load4(tierAddress + 64));

    // An arc: the velocity, slowed by drag, plus gravity.
    const float2 velocity = unpackHalves(run.userData.y);
    const float travel = drag > 0.0f ? (1.0f - exp(-drag * age)) / drag : age;
    float2 offset = velocity * travel + 0.5f * gravity * age * age;

    // Born at popScale times its size, settling quickly with an ease out.
    const float pop = sizeAndShake.z > 0.0f ? saturate(age / sizeAndShake.z) : 1.0f;
    const float settle = 1.0f - pop;
    scale = sizeAndShake.x * lerp(1.0f, sizeAndShake.y, settle * settle * settle);

    const float phase = (float)(run.userData.w >> kMotionSeedShift) * (kTwoPi / 65536.0f);
    if (age < timing.y && timing.y > 0.0f) {
      const float strength = sizeAndShake.w * (1.0f - age / timing.y);
      const float angle = age * timing.x * kTwoPi + phase;
      offset += strength * float2(sin(angle), cos(angle * 1.37f + phase));
    }
    origin += float3(0.0f, offset.x, offset.y);

    const bool bOutline = (run.userData.w & kMotionOutlineBit) != 0;
    float4 look = bOutline ? outlineColor : tierColor;
    if (!bOutline && age < timing.z) {
      const float blink = 0.5f + 0.5f * sin(age * timing.w * kTwoPi);
      look = lerp(tierColor, flashColor, blink);
    }
    color *= look;
    const float fade = (fadeIn > 0.0f ? saturate(age / fadeIn) : 1.0f) *
                       (fadeOut > 0.0f ? saturate((lifetime - age) / fadeOut) : 1.0f);
    color.a *= fade;
  }

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

  output.position = mul(float4(position, 1.0f), g_push.viewProjection);
  output.color = color;
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
