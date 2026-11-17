// Particles: quads written once when they are born and computed from their age. Each one
// flies along the shared age motion (chAgeMotion.hlsli), picks its scale, rotation, spin
// and color from its seed, and is colored by the gradient, sized by the curve and shows
// the flipbook frame of its age (GpuParticleLook).

#include "chAgeMotion.hlsli"
#include "chSpriteBatch.hlsli"

// Must match GpuParticle and GpuParticleLook in chParticleGpu.h.
static const uint kParticleStride = 32;
static const uint kMaxKeys = 8;
static const uint kFaceVelocityBit = 0x1u;
static const uint kLookTextureAddress = 32;
static const uint kLookSizeAddress = 48;
static const uint kLookRotationAddress = 64;
static const uint kLookColorAddress = 80;
static const uint kLookColor2Address = 96;
static const uint kLookCountsAddress = 112;
static const uint kColorTimesAddress = 128;
static const uint kColorKeysAddress = 160;
static const uint kSizeTimesAddress = 288;
static const uint kSizeKeysAddress = 320;
static const uint kFramesAddress = 352;

// Must match ParticleBatchParameters in chParticleGpu.h.
struct PushConstants
{
  CH_SPRITE_BATCH_HEADER
  uint particleBufferIndex;
  uint firstParticle;
  uint lookIndex;
  float time;
  float2 emitterPosition;
  float2 emitterRotation;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> g_push : register(b0);

struct VSOutput
{
  float4 position : SV_Position;
  float4 color : COLOR0;
  float2 texCoord : TEXCOORD0;
  nointerpolation uint textureAndFlags : TEXCOORD1;
};

uint
hashSeed(uint value)
{
  value ^= value >> 16;
  value *= 0x7FEB352Du;
  value ^= value >> 15;
  value *= 0x846CA68Bu;
  value ^= value >> 16;
  return value;
}

// 0 to 1, a different number for every stream of one seed.
float
random01(uint seed, uint stream)
{
  return (float)(hashSeed(seed + stream * 0x9E3779B9u) & 0xFFFFFFu) / 16777216.0f;
}

float2
rotate(float2 value, float2 cosineSine)
{
  return float2(value.x * cosineSine.x - value.y * cosineSine.y,
                value.x * cosineSine.y + value.y * cosineSine.x);
}

VSOutput
makeEmptyVertex()
{
  VSOutput output = (VSOutput)0;
  output.position = float4(0.0f, 0.0f, 0.0f, 1.0f);
  return output;
}

float4
sampleGradient(RWByteAddressBuffer look, uint count, float t)
{
  if (count == 0) {
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
  }
  float previousTime = asfloat(look.Load(kColorTimesAddress));
  float4 previous = asfloat(look.Load4(kColorKeysAddress));
  if (t <= previousTime) {
    return previous;
  }
  for (uint i = 1; i < count; ++i) {
    const float time = asfloat(look.Load(kColorTimesAddress + i * 4));
    const float4 key = asfloat(look.Load4(kColorKeysAddress + i * 16));
    if (t <= time) {
      return lerp(previous, key, saturate((t - previousTime) / max(time - previousTime, 1e-5f)));
    }
    previousTime = time;
    previous = key;
  }
  return previous;
}

float
sampleCurve(RWByteAddressBuffer look, uint count, float t)
{
  if (count == 0) {
    return 1.0f;
  }
  float previousTime = asfloat(look.Load(kSizeTimesAddress));
  float previous = asfloat(look.Load(kSizeKeysAddress));
  if (t <= previousTime) {
    return previous;
  }
  for (uint i = 1; i < count; ++i) {
    const float time = asfloat(look.Load(kSizeTimesAddress + i * 4));
    const float key = asfloat(look.Load(kSizeKeysAddress + i * 4));
    if (t <= time) {
      return lerp(previous, key, saturate((t - previousTime) / max(time - previousTime, 1e-5f)));
    }
    previousTime = time;
    previous = key;
  }
  return previous;
}

VSOutput
VSMain(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
  RWByteAddressBuffer particles = ResourceDescriptorHeap[g_push.particleBufferIndex];
  const uint address = (g_push.firstParticle + instanceId) * kParticleStride;
  const float4 first = asfloat(particles.Load4(address));
  const uint4 second = particles.Load4(address + 16);
  const float2 birthPosition = first.xy;
  const float age = g_push.time - first.z;
  const float lifetime = first.w;
  if (age < 0.0f || age >= lifetime) {
    return makeEmptyVertex();
  }
  const float2 velocity = unpackHalves(second.x);
  const uint seed = second.y;
  const float4 tint = float4(unpackHalves(second.z), unpackHalves(second.w));
  const float t = age / lifetime;

  RWByteAddressBuffer look = ResourceDescriptorHeap[g_push.lookIndex];
  const AgeMotion motion = loadAgeMotion(look, 0);
  const uint4 textureInfo = look.Load4(kLookTextureAddress);
  const uint textureAndFlags = textureInfo.x;
  const uint frameCount = max(textureInfo.y, 1u);
  const float framesPerSecond = asfloat(textureInfo.z);
  const uint flags = textureInfo.w;
  // size, minScale, maxScale
  const float4 sizeInfo = asfloat(look.Load4(kLookSizeAddress));
  // rotation, rotationVariance, spin, spinVariance
  const float4 rotationInfo = asfloat(look.Load4(kLookRotationAddress));
  const uint4 counts = look.Load4(kLookCountsAddress);
  const float depth = asfloat(counts.z);

  // Where it is, still from the emitter in local space.
  float2 center = birthPosition + getAgeOffset(motion, velocity, age);

  float angle;
  if ((flags & kFaceVelocityBit) != 0) {
    const float2 now = (motion.drag > 0.0f ? velocity * exp(-motion.drag * age) : velocity) +
                       motion.gravity * age;
    angle = atan2(now.y, now.x);
  }
  else {
    const float start = rotationInfo.x + (random01(seed, 1) * 2.0f - 1.0f) * rotationInfo.y;
    const float spin = rotationInfo.z + (random01(seed, 2) * 2.0f - 1.0f) * rotationInfo.w;
    angle = start + spin * age;
  }
  float2 turn;
  sincos(angle, turn.y, turn.x);

  const float scale = lerp(sizeInfo.z, sizeInfo.w, random01(seed, 3)) *
                      sampleCurve(look, counts.y, t);
  const float2 corner = kCorners[vertexId];
  // U runs to the right and V to the top of the quad, while v grows down the image.
  const float2 local = float2(corner.x - 0.5f, 0.5f - corner.y) * sizeInfo.xy * scale;
  float2 position = center + rotate(local, turn);
  position = g_push.emitterPosition + rotate(position, g_push.emitterRotation);

  const float4 color = lerp(asfloat(look.Load4(kLookColorAddress)),
                            asfloat(look.Load4(kLookColor2Address)),
                            random01(seed, 4)) *
                       sampleGradient(look, counts.x, t) * tint;

  const uint frame = framesPerSecond > 0.0f
                         ? (uint)(age * framesPerSecond) % frameCount
                         : min((uint)(t * frameCount), frameCount - 1);
  const float4 uvRect = asfloat(look.Load4(kFramesAddress + frame * 16));

  VSOutput output;
  output.position = mul(float4(depth, position, 1.0f), g_push.viewProjection);
  output.color = float4(color.rgb, color.a * getAgeFade(motion, age, lifetime));
  output.texCoord = uvRect.xy + corner * uvRect.zw;
  output.textureAndFlags = textureAndFlags;
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
