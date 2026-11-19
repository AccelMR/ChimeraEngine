/************************************************************************/
/**
 * @file chShadowTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of the shadow projections and the shadow light selection.
 */
/************************************************************************/
#include "chAngle.h"
#include "chCamera.h"
#include "chGraphicsTypes.h"
#include "chITexture.h"
#include "chITextureView.h"
#include "chMath.h"
#include "chShadowRenderer.h"
#include "chShapeOverlap.h"
#include "chVector3.h"
#include "chVector4.h"

#include "catch.hpp"
#include "chCoreTestHelpers.h"

using namespace chEngineSDK;

TEST_CASE("chCore - Directional shadow projection", "[Renderer]")
{
  Camera camera = createTestCamera();
  constexpr uint32 kMapSize = 1024;
  constexpr float kDistance = 20.0f;
  // Comes down at an angle, from behind the camera and to its left.
  const Vector3 lightDirection = Vector3(1.0f, 0.5f, -1.0f).getNormalized();
  const DirectionalShadowProjection projection =
      ShadowRenderer::computeDirectionalProjection(camera, lightDirection, 0.0f, kDistance,
                                                   kMapSize);

  const auto project = [](const Matrix4& viewProjection, const Vector3& point) {
    const Vector4 clip = viewProjection.transformVector4(Vector4(point.x, point.y, point.z, 1.0f));
    return Vector3(clip.x / clip.w, clip.y / clip.w, clip.z / clip.w);
  };

  SECTION("The covered part of the view is inside the map")
  {
    // A horizontal field of view of 90 degrees reaches as far sideways as forward.
    const Vector3 forward = camera.getForwardVector();
    const Vector3 right = camera.getRightVector();
    const Vector3 up = camera.getUpVector();
    constexpr float kAspect = 720.0f / 1280.0f;
    for (const float depth : {0.1f, 10.0f, kDistance}) {
      for (const float x : {-0.99f, 0.0f, 0.99f}) {
        for (const float y : {-0.99f, 0.0f, 0.99f}) {
          const Vector3 point = camera.getPosition() + forward * depth +
                                right * (x * depth) + up * (y * depth * kAspect);
          const Vector3 ndc = project(projection.viewProjection, point);
          REQUIRE(Math::abs(ndc.x) <= 1.0f);
          REQUIRE(Math::abs(ndc.y) <= 1.0f);
          REQUIRE(ndc.z >= 0.0f);
          REQUIRE(ndc.z <= 1.0f);
        }
      }
    }
  }

  SECTION("Casters toward the light are kept, casters beside the map are not")
  {
    const Vector3 viewCenter = camera.getPosition() + camera.getForwardVector() * 10.0f;
    const Vector3 towardLight = viewCenter - lightDirection * 1000.0f;
    REQUIRE(ShapeOverlap::frustumPoint(projection.casterFrustum, towardLight));
    // In front of the near plane: the depth clamp draws it at 0.
    REQUIRE(project(projection.viewProjection, towardLight).z < 0.0f);

    const Vector3 side = lightDirection.cross(Vector3::UP).getNormalized();
    REQUIRE_FALSE(ShapeOverlap::frustumPoint(projection.casterFrustum, viewCenter + side * 1000.0f));
    REQUIRE_FALSE(
        ShapeOverlap::frustumPoint(projection.casterFrustum, viewCenter + lightDirection * 1000.0f));
  }

  SECTION("The map moves in whole texels")
  {
    const Vector3 move(0.013f, 0.0271f, -0.0057f);
    Camera moved = camera;
    moved.setPosition(camera.getPosition() + move);
    moved.lookAt(camera.getLookAt() + move);
    const DirectionalShadowProjection movedProjection =
        ShadowRenderer::computeDirectionalProjection(moved, lightDirection, 0.0f, kDistance,
                                                     kMapSize);
    REQUIRE(movedProjection.texelWorldSize == projection.texelWorldSize);

    const Vector3 point(3.0f, 1.0f, 0.5f);
    const Vector3 before = project(projection.viewProjection, point);
    const Vector3 after = project(movedProjection.viewProjection, point);
    // NDC spans two units over the map.
    for (const float texels : {(after.x - before.x) * kMapSize * 0.5f,
                               (after.y - before.y) * kMapSize * 0.5f}) {
      REQUIRE(Math::abs(texels - Math::floor(texels + 0.5f)) < 0.01f);
    }
  }

  SECTION("A cascade covers its own slice of the view with smaller texels")
  {
    const DirectionalShadowProjection slice = ShadowRenderer::computeDirectionalProjection(
        camera, lightDirection, 5.0f, 10.0f, kMapSize);
    REQUIRE(slice.texelWorldSize < projection.texelWorldSize);

    const Vector3 forward = camera.getForwardVector();
    const Vector3 right = camera.getRightVector();
    const Vector3 up = camera.getUpVector();
    constexpr float kAspect = 720.0f / 1280.0f;
    for (const float depth : {5.0f, 7.5f, 10.0f}) {
      for (const float x : {-0.99f, 0.0f, 0.99f}) {
        for (const float y : {-0.99f, 0.0f, 0.99f}) {
          const Vector3 point = camera.getPosition() + forward * depth +
                                right * (x * depth) + up * (y * depth * kAspect);
          const Vector3 ndc = project(slice.viewProjection, point);
          REQUIRE(Math::abs(ndc.x) <= 1.0f);
          REQUIRE(Math::abs(ndc.y) <= 1.0f);
        }
      }
    }
  }
}

TEST_CASE("chCore - Shadow cascade splits", "[Renderer]")
{
  float splits[4];

  SECTION("Lambda 0 gives even steps")
  {
    ShadowRenderer::computeCascadeSplits(1.0f, 101.0f, 0.0f, splits);
    REQUIRE(splits[0] == Approx(26.0f));
    REQUIRE(splits[1] == Approx(51.0f));
    REQUIRE(splits[2] == Approx(76.0f));
    REQUIRE(splits[3] == 101.0f);
  }

  SECTION("Lambda 1 gives steps in proportion to the distance")
  {
    ShadowRenderer::computeCascadeSplits(1.0f, 10000.0f, 1.0f, splits);
    REQUIRE(splits[0] == Approx(10.0f));
    REQUIRE(splits[1] == Approx(100.0f));
    REQUIRE(splits[2] == Approx(1000.0f));
    REQUIRE(splits[3] == 10000.0f);
  }

  SECTION("Splits grow and the last one ends at the distance")
  {
    ShadowRenderer::computeCascadeSplits(0.1f, 200.0f, 0.8f, splits);
    REQUIRE(splits[0] > 0.1f);
    REQUIRE(splits[0] < splits[1]);
    REQUIRE(splits[1] < splits[2]);
    REQUIRE(splits[2] < splits[3]);
    REQUIRE(splits[3] == 200.0f);
  }

  SECTION("One cascade covers the whole distance")
  {
    ShadowRenderer::computeCascadeSplits(0.1f, 200.0f, 0.8f, Span<float>(splits, 1));
    REQUIRE(splits[0] == 200.0f);
  }
}

TEST_CASE("chCore - Spot shadow projection", "[Renderer]")
{
  constexpr uint32 kMapSize = 1024;
  const Degree halfAngle(30.0f);
  LightItem light;
  light.type = LightType::Spot;
  light.position = Vector3(0.0f, 0.0f, 2.0f);
  light.direction = Vector3::FORWARD;
  light.range = 10.0f;
  light.outerConeCos = Math::cos(Radian(halfAngle));
  const SpotShadowProjection projection =
      ShadowRenderer::computeSpotProjection(light, 0.1f, kMapSize);

  const auto project = [&projection](const Vector3& point) {
    const Vector4 clip =
        projection.viewProjection.transformVector4(Vector4(point.x, point.y, point.z, 1.0f));
    return Vector3(clip.x / clip.w, clip.y / clip.w, clip.z / clip.w);
  };

  SECTION("The axis is the center of the map, the range its far plane")
  {
    const Vector3 middle = project(light.position + light.direction * 5.0f);
    REQUIRE(middle.x == Approx(0.0f).margin(1e-5f));
    REQUIRE(middle.y == Approx(0.0f).margin(1e-5f));
    REQUIRE(middle.z > 0.0f);
    REQUIRE(middle.z < 1.0f);
    REQUIRE(project(light.position + light.direction * 9.9f).z < 1.0f);
    REQUIRE(project(light.position + light.direction * 10.5f).z > 1.0f);
  }

  SECTION("The cone fits the map")
  {
    for (const float degrees : {29.0f, 31.0f}) {
      const float side = 5.0f * Math::tan(Radian(Degree(degrees)));
      const Vector3 right = project(light.position + Vector3(5.0f, side, 0.0f));
      const Vector3 up = project(light.position + Vector3(5.0f, 0.0f, side));
      if (degrees < 30.0f) {
        REQUIRE(Math::abs(right.x) < 1.0f);
        REQUIRE(Math::abs(up.y) < 1.0f);
      }
      else {
        REQUIRE(Math::abs(right.x) > 1.0f);
        REQUIRE(Math::abs(up.y) > 1.0f);
      }
    }
    REQUIRE(projection.texelWorldSizePerDistance ==
            Approx(2.0f * Math::tan(Radian(halfAngle)) / kMapSize));
  }

  SECTION("Casters between the light and the near plane are kept, those behind it are not")
  {
    REQUIRE(ShapeOverlap::frustumPoint(projection.casterFrustum,
                                       light.position + light.direction * 0.05f));
    REQUIRE_FALSE(ShapeOverlap::frustumPoint(projection.casterFrustum,
                                             light.position - light.direction));
  }

  SECTION("The near plane stays under half the range")
  {
    const SpotShadowProjection farNear =
        ShadowRenderer::computeSpotProjection(light, 20.0f, kMapSize);
    REQUIRE(farNear.nearPlane == 5.0f);
    REQUIRE(projection.nearPlane == 0.1f);
  }
}

TEST_CASE("chCore - Point shadow projection", "[Renderer]")
{
  LightItem light;
  light.type = LightType::Point;
  light.position = Vector3(3.0f, -2.0f, 1.0f);
  light.range = 10.0f;
  const PointShadowProjection projection = ShadowRenderer::computePointProjection(light, 0.1f);

  const auto project = [&projection](uint32 face, const Vector3& point) {
    const Vector4 clip = projection.faceViewProjections[face].transformVector4(
        Vector4(point.x, point.y, point.z, 1.0f));
    return Vector3(clip.x / clip.w, clip.y / clip.w, clip.z / clip.w);
  };

  SECTION("Each face's image lands where cube map sampling reads it")
  {
    // Cube map face selection as Direct3D and Vulkan define it: for the major axis ma of
    // the direction, s = (sc / |ma| + 1) / 2 and t = (tc / |ma| + 1) / 2, with t going down
    // the image. A face's NDC x is then sc / |ma| and its NDC y is -tc / |ma|.
    const auto cubeCoords = [](uint32 face, const Vector3& direction, float& sc, float& tc) {
      switch (face) {
        case 0: sc = -direction.z; tc = -direction.y; return direction.x;
        case 1: sc = direction.z; tc = -direction.y; return -direction.x;
        case 2: sc = direction.x; tc = direction.z; return direction.y;
        case 3: sc = direction.x; tc = -direction.z; return -direction.y;
        case 4: sc = direction.x; tc = -direction.y; return direction.z;
        default: sc = -direction.x; tc = -direction.y; return -direction.z;
      }
    };
    const Vector3 directions[] = {Vector3(4.0f, 1.0f, -2.0f), Vector3(-5.0f, 2.0f, 1.0f),
                                  Vector3(1.0f, 3.0f, 2.0f),  Vector3(-2.0f, -4.0f, 1.0f),
                                  Vector3(1.0f, -2.0f, 6.0f), Vector3(2.0f, 1.0f, -3.0f)};
    for (uint32 face = 0; face < PointShadowProjection::FACE_COUNT; ++face) {
      const Vector3& direction = directions[face];
      float sc = 0.0f;
      float tc = 0.0f;
      const float major = cubeCoords(face, direction, sc, tc);
      REQUIRE(major > 0.0f);
      const Vector3 ndc = project(face, light.position + direction);
      REQUIRE(ndc.x == Approx(sc / major).margin(1e-4f));
      REQUIRE(ndc.y == Approx(-tc / major).margin(1e-4f));
    }
  }

  SECTION("The stored depth follows the distance along the face's axis")
  {
    for (const float distance : {0.5f, 3.0f, 9.0f}) {
      const Vector3 ndc = project(2, light.position + Vector3(0.5f, distance, -0.3f));
      REQUIRE(ndc.z ==
              Approx(projection.depthScale + projection.depthOffset / distance).margin(1e-5f));
    }
    REQUIRE(project(0, light.position + Vector3(9.9f, 0.0f, 0.0f)).z < 1.0f);
    REQUIRE(project(0, light.position + Vector3(10.5f, 0.0f, 0.0f)).z > 1.0f);
  }

  SECTION("Casters between the light and the near plane are kept, those behind it are not")
  {
    REQUIRE(ShapeOverlap::frustumPoint(projection.faceCasterFrustums[4],
                                       light.position + Vector3(0.0f, 0.0f, 0.05f)));
    REQUIRE_FALSE(ShapeOverlap::frustumPoint(projection.faceCasterFrustums[4],
                                             light.position - Vector3(0.0f, 0.0f, 1.0f)));
  }

  SECTION("The near plane stays under half the range")
  {
    REQUIRE(ShadowRenderer::computePointProjection(light, 20.0f).nearPlane == 5.0f);
  }
}

TEST_CASE("chCore - Spot and point shadow selection", "[Renderer]")
{
  // Every light stands on the X axis, the camera at the origin.
  const auto makeLight = [](LightType type, float x, float range, bool bCast) {
    LightItem light;
    light.type = type;
    light.position = Vector3(x, 0.0f, 0.0f);
    light.range = range;
    light.bCastShadows = bCast;
    return light;
  };
  const LightItem nearSmall = makeLight(LightType::Spot, 10.0f, 2.0f, true);
  const LightItem farBig = makeLight(LightType::Spot, 40.0f, 20.0f, true);
  const LightItem around = makeLight(LightType::Spot, 1.0f, 5.0f, true);
  const LightItem tooFar = makeLight(LightType::Spot, 500.0f, 10.0f, true);
  const LightItem noShadow = makeLight(LightType::Spot, 2.0f, 5.0f, false);
  const LightItem point = makeLight(LightType::Point, 2.0f, 5.0f, true);
  const LightItem* lights[] = {&nearSmall, &farBig, &around, &tooFar, &noShadow, &point};

  Vector<LocalShadowCandidate> candidates;
  ShadowRenderer::selectLocalShadows(Vector3::ZERO, lights, LightType::Spot, 8, 100.0f,
                                     candidates);

  SECTION("Only spot lights with shadows inside the distance, the biggest on screen first")
  {
    REQUIRE(candidates.size() == 3);
    REQUIRE(candidates[0].light == &around);
    REQUIRE(candidates[0].rangeDistance == 0.0f);
    REQUIRE(candidates[1].light == &farBig);
    REQUIRE(candidates[1].rangeDistance == Approx(20.0f));
    REQUIRE(candidates[2].light == &nearSmall);
    REQUIRE(candidates[2].rangeDistance == Approx(8.0f));
  }

  SECTION("At most maxCount are kept")
  {
    ShadowRenderer::selectLocalShadows(Vector3::ZERO, lights, LightType::Spot, 2, 100.0f,
                                       candidates);
    REQUIRE(candidates.size() == 2);
    REQUIRE(candidates[1].light == &farBig);
  }

  SECTION("The distance counts from the camera to the edge of the range")
  {
    ShadowRenderer::selectLocalShadows(Vector3::ZERO, lights, LightType::Spot, 8, 15.0f,
                                       candidates);
    REQUIRE(candidates.size() == 2);
    REQUIRE(candidates[1].light == &nearSmall);
  }

  SECTION("Point lights are picked on their own")
  {
    ShadowRenderer::selectLocalShadows(Vector3::ZERO, lights, LightType::Point, 8, 100.0f,
                                       candidates);
    REQUIRE(candidates.size() == 1);
    REQUIRE(candidates[0].light == &point);
  }
}
