// The editor's grid on the ground plane z = 0, or on the plane x = 0 of 2D games, drawn
// over the finished image and hidden by what the scene drew in front of it (when there is a
// scene depth). Lines come in three levels, each ten times the one
// below; with adaptive spacing the level is picked per pixel from how much ground a pixel
// covers, so cells keep about the same size on screen at any distance or height.

// Must match GridPushConstants in chViewportGrid.cpp.
// Flat, because a struct inside push constants is laid out differently by each target.
struct PushConstants
{
  // Rows of the inverse of view * projection.
  float4 clipToWorldRow0;
  float4 clipToWorldRow1;
  float4 clipToWorldRow2;
  float4 clipToWorldRow3;
  float3 cameraPosition;
  uint depthIndex;
  float spacing;
  float minCellPixels;
  float fadeDistance;
  float opacity;
  uint adaptive;
  // 2: the ground (z = 0, lines along x and y); 0: the 2D plane (x = 0, lines along y and
  // z), seen straight on, so it neither fades with distance nor at grazing angles.
  uint planeAxis;
};

static const uint kNoDepth = 0xFFFFFFFFu;

[[vk::push_constant]] ConstantBuffer<PushConstants> g_push : register(b0);

static const float3 kLineColor = float3(0.6f, 0.6f, 0.6f);
static const float3 kAxisXColor = float3(0.9f, 0.25f, 0.25f);
static const float3 kAxisYColor = float3(0.35f, 0.85f, 0.3f);
static const float3 kAxisZColor = float3(0.3f, 0.5f, 0.95f);
// Brightness of the finest level and of the levels ten and a hundred times bigger.
static const float kMinorStrength = 0.5f;
static const float kMajorStrength = 1.0f;
// Line widths in pixels.
static const float kLineWidth = 1.0f;
static const float kAxisWidth = 1.5f;
// Far from the camera the ground is seen almost edge on and its lines would shimmer; below
// this slope of the view ray the grid fades out.
static const float kGrazingSlope = 0.08f;
// A camera high above the ground still sees the grid this many times its height away.
static const float kFadeHeightMultiple = 30.0f;

struct VertexOutput
{
  float4 position : SV_Position;
  float2 clipPosition : TEXCOORD0;
};

VertexOutput
VSMain(uint vertexId : SV_VertexID)
{
  // One triangle that covers the screen: (-1, -1), (3, -1), (-1, 3). The editor's shaders
  // are built apart from chCore's, so its include is not reachable.
  const float2 position = float2((vertexId << 1) & 2, vertexId & 2) * 2.0f - 1.0f;
  VertexOutput output;
  output.position = float4(position, 0.0f, 1.0f);
  output.clipPosition = position;
  return output;
}

// 1 on a line of a grid with cells of size cell, falling to 0 just past width pixels.
float
lineCoverage(float2 position, float cell, float2 pixelSize, float width)
{
  const float2 distanceToLine = abs(frac(position / cell + 0.5f) - 0.5f) * cell / pixelSize;
  const float2 coverage = saturate(width * 0.5f + 0.5f - distanceToLine);
  return max(coverage.x, coverage.y);
}

float4
PSMain(VertexOutput input) : SV_Target0
{
  const float4x4 clipToWorld = float4x4(g_push.clipToWorldRow0, g_push.clipToWorldRow1,
                                        g_push.clipToWorldRow2, g_push.clipToWorldRow3);
  const bool bPlane2D = g_push.planeAxis == 0;
  // The pixel's view ray at the near and far planes, in homogeneous world coordinates.
  // These change linearly with the clip depth, so the depth where the ray meets the plane
  // comes out directly, and it is the value to compare with the scene depth.
  const float4 nearPoint = mul(float4(input.clipPosition, 0.0f, 1.0f), clipToWorld);
  const float4 farPoint = mul(float4(input.clipPosition, 1.0f, 1.0f), clipToWorld);
  const float nearHeight = bPlane2D ? nearPoint.x : nearPoint.z;
  const float rayHeight = (bPlane2D ? farPoint.x : farPoint.z) - nearHeight;
  const float groundDepth = -nearHeight / (abs(rayHeight) > 1e-12f ? rayHeight : 1e-12f);
  const float4 ground = lerp(nearPoint, farPoint, groundDepth);
  // The two axes the plane spans.
  const float2 position = (bPlane2D ? ground.yz : ground.xy) / ground.w;

  // Before any discard, so the derivatives see every pixel of the quad.
  const float2 pixelSize = max(fwidth(position), 1e-6f);

  float sceneDepth = 1.0f;
  if (g_push.depthIndex != kNoDepth) {
    Texture2D<float> depth = ResourceDescriptorHeap[g_push.depthIndex];
    sceneDepth = depth.Load(int3(input.position.xy, 0));
  }
  if (groundDepth < 0.0f || groundDepth > 1.0f || ground.w <= 0.0f ||
      groundDepth > sceneDepth) {
    discard;
  }

  float cell = g_push.spacing;
  float levelFade = 0.0f;
  if (g_push.adaptive != 0) {
    // How many times ten the cells must grow to stay at least minCellPixels wide.
    const float level =
        max(0.0f, log10(max(pixelSize.x, pixelSize.y) * g_push.minCellPixels / cell));
    cell *= pow(10.0f, floor(level));
    levelFade = frac(level);
  }
  // As levelFade goes to 1 the finest lines fade out and the middle ones become as faint
  // as minor lines, so the next level starts exactly where this one ends.
  const float fine = lineCoverage(position, cell, pixelSize, kLineWidth);
  const float middle = lineCoverage(position, cell * 10.0f, pixelSize, kLineWidth);
  const float coarse = lineCoverage(position, cell * 100.0f, pixelSize, kLineWidth);
  float alpha = max(fine * kMinorStrength * (1.0f - levelFade),
                    max(middle * lerp(kMajorStrength, kMinorStrength, levelFade),
                        coarse * kMajorStrength));
  float3 color = kLineColor;

  // On the ground the X axis is the line y = 0 and the Y axis the line x = 0; on the 2D
  // plane the Y axis is the line z = 0 and the Z axis the line y = 0.
  const float firstAxis = saturate(kAxisWidth * 0.5f + 0.5f - abs(position.y) / pixelSize.y);
  const float secondAxis = saturate(kAxisWidth * 0.5f + 0.5f - abs(position.x) / pixelSize.x);
  color = lerp(color, bPlane2D ? kAxisYColor : kAxisXColor, firstAxis);
  color = lerp(color, bPlane2D ? kAxisZColor : kAxisYColor, secondAxis);
  alpha = max(alpha, max(firstAxis, secondAxis));

  if (!bPlane2D) {
    const float3 toGround = float3(position, 0.0f) - g_push.cameraPosition;
    const float distance = length(toGround);
    const float fadeDistance =
        max(g_push.fadeDistance, abs(g_push.cameraPosition.z) * kFadeHeightMultiple);
    alpha *= saturate(1.0f - distance / fadeDistance);
    alpha *= saturate(abs(toGround.z) / (distance * kGrazingSlope));
  }
  alpha *= g_push.opacity;
  if (alpha <= 1.0f / 255.0f) {
    discard;
  }
  return float4(color, alpha);
}
