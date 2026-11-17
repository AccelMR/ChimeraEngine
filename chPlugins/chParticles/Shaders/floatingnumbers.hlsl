// Floating numbers: glyph runs written once, moved by their age. Each run flies from where
// it was made along its velocity under gravity, slowed by drag (chAgeMotion.hlsli), and
// pops, shakes, flashes and fades with the look of its tier (GpuFloatingNumberMotion).

#include "chAgeMotion.hlsli"
#include "chGlyphRun.hlsli"
#include "chSpriteBatch.hlsli"

// Must match FloatingNumberGpu and GpuFloatingNumberMotion in chFloatingNumberGpu.h.
static const uint kTierMask = 0xFFu;
static const uint kOutlineBit = 0x100u;
static const uint kSeedShift = 16;
static const uint kTierCountAddress = kAgeMotionSize;
static const uint kTiersAddress = kAgeMotionSize + 16;
static const uint kTierStride = 80;

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

  RWByteAddressBuffer motionBuffer = ResourceDescriptorHeap[g_push.motionIndex];
  const AgeMotion motion = loadAgeMotion(motionBuffer, 0);
  const uint tierCount = motionBuffer.Load(kTierCountAddress);
  const uint tierIndex = min(run.userData.w & kTierMask, max(tierCount, 1u) - 1u);
  const uint tierAddress = kTiersAddress + tierIndex * kTierStride;
  const float4 tierColor = asfloat(motionBuffer.Load4(tierAddress));
  const float4 flashColor = asfloat(motionBuffer.Load4(tierAddress + 16));
  const float4 outlineColor = asfloat(motionBuffer.Load4(tierAddress + 32));
  // scale, popScale, popDuration, shakeAmplitude
  const float4 sizeAndShake = asfloat(motionBuffer.Load4(tierAddress + 48));
  // shakeFrequency, shakeDuration, flashDuration, flashRate
  const float4 timing = asfloat(motionBuffer.Load4(tierAddress + 64));

  const float2 velocity = unpackHalves(run.userData.y);
  const float phase = getSeedPhase(run.userData.w >> kSeedShift);
  const float2 offset = getAgeOffset(motion, velocity, age) +
                        getAgeShake(age, sizeAndShake.w, timing.x, timing.y, phase);
  const float scale = sizeAndShake.x * getAgePop(age, sizeAndShake.y, sizeAndShake.z);

  const bool bOutline = (run.userData.w & kOutlineBit) != 0;
  float4 look = bOutline ? outlineColor : tierColor;
  if (!bOutline && age < timing.z) {
    const float blink = 0.5f + 0.5f * sin(age * timing.w * kTwoPi);
    look = lerp(tierColor, flashColor, blink);
  }
  float4 color = getRunColor(run) * look;
  const float fade = getAgeFade(motion, age, lifetime);
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
