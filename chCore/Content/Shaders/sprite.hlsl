// Sprites as instanced quads. There is no vertex buffer: each vertex reads its sprite from a
// storage buffer by the instance index and makes its corner from the vertex index.

// Must match GpuSpriteInstance in chSpriteItem.h.
static const uint kInstanceStride = 64;
static const uint kPointFilterBit = 0x80000000u;
static const uint kTextureIndexMask = 0x000FFFFFu;
static const uint kNoTexture = 0x000FFFFFu;
static const uint kModeShift = 20;
static const uint kModeMask = 0x3u;
static const uint kEdgeShift = 22;
static const uint kEdgeMask = 0xFFu;
// SpriteTextureMode
static const uint kModeCoverage = 1;
static const uint kModeSdf = 2;
// Where stb writes the edge of a distance field.
static const float kSdfEdge = 128.0f / 255.0f;

// Must match SpritePushConstants in chSpritePass.cpp.
struct PushConstants
{
  row_major float4x4 viewProjection;
  uint instanceBufferIndex;
  // The draw always starts at instance 0, so where its run starts in the buffer comes here.
  uint firstInstance;
  uint pointSamplerIndex;
  uint linearSamplerIndex;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> g_push : register(b0);

// Two triangles, as (u, v) with v = 0 at the top of the image.
static const float2 kCorners[6] = {float2(0.0f, 0.0f), float2(1.0f, 0.0f), float2(0.0f, 1.0f),
                                   float2(0.0f, 1.0f), float2(1.0f, 0.0f), float2(1.0f, 1.0f)};

struct VSOutput
{
  float4 position : SV_Position;
  float4 color : COLOR0;
  float2 texCoord : TEXCOORD0;
  nointerpolation uint textureAndFlags : TEXCOORD1;
};

float2
unpackHalves(uint packed)
{
  return float2(f16tof32(packed & 0xFFFF), f16tof32(packed >> 16));
}

VSOutput
VSMain(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
  ByteAddressBuffer instances = ResourceDescriptorHeap[g_push.instanceBufferIndex];
  const uint address = (g_push.firstInstance + instanceId) * kInstanceStride;
  const float3 center = asfloat(instances.Load3(address));
  const uint textureAndFlags = instances.Load(address + 12);
  const float3 halfAxisU = asfloat(instances.Load3(address + 16));
  const uint colorRG = instances.Load(address + 28);
  const float3 halfAxisV = asfloat(instances.Load3(address + 32));
  const uint colorBA = instances.Load(address + 44);
  const float4 uvRect = asfloat(instances.Load4(address + 48));

  const float2 corner = kCorners[vertexId];
  // U runs to the right and V to the top of the quad, while v grows down the image.
  const float3 position =
      center + halfAxisU * (corner.x * 2.0f - 1.0f) + halfAxisV * (1.0f - corner.y * 2.0f);

  VSOutput output;
  output.position = mul(float4(position, 1.0f), g_push.viewProjection);
  output.color = float4(unpackHalves(colorRG), unpackHalves(colorBA));
  output.texCoord = uvRect.xy + corner * uvRect.zw;
  output.textureAndFlags = textureAndFlags;
  return output;
}

float4
PSMain(VSOutput input) : SV_Target0
{
  const uint textureIndex = input.textureAndFlags & kTextureIndexMask;
  if (textureIndex == kNoTexture) {
    return input.color;
  }
  // Sprites of one draw read different textures, so the indexes are not uniform.
  const uint samplerIndex = (input.textureAndFlags & kPointFilterBit) != 0
                                ? g_push.pointSamplerIndex
                                : g_push.linearSamplerIndex;
  Texture2D spriteTexture = ResourceDescriptorHeap[NonUniformResourceIndex(textureIndex)];
  SamplerState spriteSampler = SamplerDescriptorHeap[NonUniformResourceIndex(samplerIndex)];
  const float4 texel = spriteTexture.Sample(spriteSampler, input.texCoord);
  // Taken here, where every pixel of the quad runs, so the derivative is defined.
  const float fieldWidth = max(fwidth(texel.r), 1e-5f) * 0.5f;

  const uint mode = (input.textureAndFlags >> kModeShift) & kModeMask;
  if (mode == kModeCoverage) {
    return float4(input.color.rgb, input.color.a * texel.r);
  }
  if (mode == kModeSdf) {
    const float edgeOffset = (float)((input.textureAndFlags >> kEdgeShift) & kEdgeMask);
    const float edge = kSdfEdge - edgeOffset / 255.0f;
    const float coverage = smoothstep(edge - fieldWidth, edge + fieldWidth, texel.r);
    return float4(input.color.rgb, input.color.a * coverage);
  }
  return texel * input.color;
}
