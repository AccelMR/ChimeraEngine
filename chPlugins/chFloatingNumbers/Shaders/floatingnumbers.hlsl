// Floating numbers: glyph runs written once, moved by their age. Each run flies from where
// it was made along its velocity under gravity, slowed by drag, and pops, shakes, flashes
// and fades with the look of its tier (GpuFloatingNumberMotion).

#include "chGlyphRun.hlsli"
#include "chSpriteBatch.hlsli"

// Must match FloatingNumberGpu and GpuFloatingNumberMotion in chFloatingNumberGpu.h.
static const uint kTierMask = 0xFFu;
static const uint kOutlineBit = 0x100u;
static const uint kSeedShift = 16;
static const uint kMotionHeaderSize = 32;
static const uint kTierStride = 80;
static const float kTwoPi = 6.28318530718f;

// Must match FloatingNumberBatchParameters in chFloatingNumberGpu.h.
struct PushConstants
{
  CH_SPRITE_BATCH_HEADER
  uint runBufferIndex;
  uint firstRun;
  uint motionIndex;
  float time;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> g_push : register(b0);

GlyphVSOutput
VSMain(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
  const uint glyphIndex = vertexId / 6;
  const float2 corner = kCorners[vertexId - glyphIndex * 6];
  RWByteAddressBuffer runs = ResourceDescriptorHeap[g_push.runBufferIndex];
  const GlyphRun run = loadRun(runs, (g_push.firstRun + instanceId) * kRunStride, glyphIndex);
  if (glyphIndex >= run.glyphCount) {
    return makeEmptyGlyphVertex();
  }

  const float age = g_push.time - asfloat(run.userData.x);
  const float lifetime = asfloat(run.userData.z);
  if (age < 0.0f || age >= lifetime) {
    return makeEmptyGlyphVertex();
  }

  RWByteAddressBuffer motion = ResourceDescriptorHeap[g_push.motionIndex];
  const float4 header = asfloat(motion.Load4(0));
  const float4 header2 = asfloat(motion.Load4(16));
  const float2 gravity = header.xy;
  const float drag = header.z;
  const float fadeIn = header.w;
  const float fadeOut = header2.x;
  const uint tierCount = asuint(header2.y);
  const uint tierIndex = min(run.userData.w & kTierMask, max(tierCount, 1u) - 1u);
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
  const float scale = sizeAndShake.x * lerp(1.0f, sizeAndShake.y, settle * settle * settle);

  const float phase = (float)(run.userData.w >> kSeedShift) * (kTwoPi / 65536.0f);
  if (age < timing.y && timing.y > 0.0f) {
    const float strength = sizeAndShake.w * (1.0f - age / timing.y);
    const float angle = age * timing.x * kTwoPi + phase;
    offset += strength * float2(sin(angle), cos(angle * 1.37f + phase));
  }

  const bool bOutline = (run.userData.w & kOutlineBit) != 0;
  float4 look = bOutline ? outlineColor : tierColor;
  if (!bOutline && age < timing.z) {
    const float blink = 0.5f + 0.5f * sin(age * timing.w * kTwoPi);
    look = lerp(tierColor, flashColor, blink);
  }
  float4 color = getRunColor(run) * look;
  const float fade = (fadeIn > 0.0f ? saturate(age / fadeIn) : 1.0f) *
                     (fadeOut > 0.0f ? saturate((lifetime - age) / fadeOut) : 1.0f);
  color.a *= fade;

  return makeGlyphVertex(run,
                         corner,
                         run.position + float3(0.0f, offset.x, offset.y),
                         scale,
                         color,
                         g_push.viewProjection);
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
