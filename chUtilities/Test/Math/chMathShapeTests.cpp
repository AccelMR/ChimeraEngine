/************************************************************************/
/**
 * @file chMathShapeTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of the shapes, their overlap and ray tests, and the 2D spatial hash.
 */
/************************************************************************/
#include "chAlgorithm.h"
#include "chBox2D.h"
#include "chCapsule.h"
#include "chFrustum.h"
#include "chHash.h"
#include "chMath.h"
#include "chMatrixHelpers.h"
#include "chOrientedBox.h"
#include "chPlane.h"
#include "chQuaternion.h"
#include "chRandom.h"
#include "chRay.h"
#include "chRayCast.h"
#include "chRotator.h"
#include "chShapeOverlap.h"
#include "chSpatialHash2D.h"
#include "chSphereBoxBounds.h"
#include "chVector2.h"
#include "chVector3.h"

#include "catch.hpp"

using namespace chEngineSDK;

TEST_CASE("chUtilities - AABox", "[Math]") {
  REQUIRE(sizeof(AABox) == 12 * 2);

  const AABox UnitBox(Vector3::ZERO, Vector3::UNIT);
  REQUIRE(UnitBox.getCenter() == Vector3(0.5f, 0.5f, 0.5f));
  REQUIRE(UnitBox.getSize() == Vector3::UNIT);

  AABox Movable(UnitBox);
  Movable.moveTo(Vector3::UNIT * 3);
  REQUIRE(Movable.getCenter() == Vector3::UNIT * 3);

  Movable.shiftBy(Vector3(-.5f, -.5f, -.5f));
  REQUIRE(Movable.getCenter() == Vector3(2.5f, 2.5f, 2.5f));

  const Vector3 Half(.5f, .5f, .5f);
  REQUIRE(ShapeOverlap::pointBox(Half, UnitBox));
  REQUIRE_FALSE(ShapeOverlap::pointBox(Vector3::UNIT * 2, UnitBox));

  const AABox Box2(Vector3::UNIT, Vector3::UNIT * 2);
  const AABox FarBox(Vector3::UNIT * 3.1f, Vector3::UNIT * 6);
  REQUIRE(ShapeOverlap::boxBox(Box2, Movable));
  REQUIRE_FALSE(ShapeOverlap::boxBox(Movable, FarBox));

  const Vector<Vector3> ArrayPoints = {{-1.0f, -1.0f, 1.0f}, {7.0f, 8.0f, -2.0f},
                                       {1.0f, 1.1f, 1.6f},   {7.0f, 12.0f, 22.0f},
                                       {4.0f, 2.0f, 1.0f},   {6.0f, 3.1f, 22.6f}};

  const AABox FromPoints(ArrayPoints);
  REQUIRE(FromPoints.minPoint == Vector3(-1.0f, -1.0f, -2.0f));
  REQUIRE(FromPoints.maxPoint == Vector3(7.0f, 12.0f, 22.6f));

  // Points away from the origin: the box used to start at zero and always reach it.
  const Vector<Vector3> FarPoints = {{100.0f, 200.0f, 300.0f}, {110.0f, 205.0f, 301.0f}};
  const AABox FarBox2(FarPoints);
  REQUIRE(FarBox2.minPoint == Vector3(100.0f, 200.0f, 300.0f));
  REQUIRE(FarBox2.maxPoint == Vector3(110.0f, 205.0f, 301.0f));

  const AABox EmptyBox(Vector<Vector3>{});
  REQUIRE(EmptyBox.minPoint == Vector3::ZERO);
  REQUIRE(EmptyBox.maxPoint == Vector3::ZERO);

  const SphereBoxBounds FarBounds(FarPoints);
  REQUIRE(FarBounds.center == Vector3(105.0f, 202.5f, 300.5f));
  REQUIRE(FarBounds.boxExtent == Vector3(5.0f, 2.5f, 0.5f));
  REQUIRE(FarBounds.sphereRadius == Approx(Vector3(5.0f, 2.5f, 0.5f).magnitude()));

  const Sphere FarSphere(FarPoints);
  REQUIRE(FarSphere.center == Vector3(105.0f, 202.5f, 300.5f));
  REQUIRE(ShapeOverlap::pointSphere(FarPoints[0], FarSphere));
  REQUIRE(ShapeOverlap::pointSphere(FarPoints[1], FarSphere));

  const Box2D FarRect(Vector<Vector2>{{100.0f, 200.0f}, {110.0f, 205.0f}});
  REQUIRE(FarRect.minPoint == Vector2(100.0f, 200.0f));
  REQUIRE(FarRect.maxPoint == Vector2(110.0f, 205.0f));
}

TEST_CASE("chUtilities - Plane", "[Math]") {
  REQUIRE(sizeof(Plane) == 16);

  const Plane plane1(Vector3::UNIT, Vector3::UP);
  const float DistanceToZero = plane1.planeDot(Vector3::ZERO);
  const float DistanceToThree = plane1.planeDot(Vector3::UNIT * 3.f);

  REQUIRE(DistanceToZero < 0);
  REQUIRE(DistanceToThree > 0);

  // Plane stores normal . p = w, so a plane at height 1 facing up has w = 1.
  REQUIRE(plane1.normal == Vector3::UP);
  REQUIRE(plane1.w == 1.0f);
  REQUIRE(plane1.planeDot(Vector3(5.0f, -3.0f, 4.0f)) == 3.0f);
  static_assert(Plane(Vector3::UP, 2.0f).planeDot(Vector3::ZERO) == -2.0f);

  // Three points: the normal is (p2 - p1) x (p3 - p1), made unit length.
  const Plane fromPoints(Vector3(0.0f, 0.0f, 2.0f), Vector3(1.0f, 0.0f, 2.0f),
                         Vector3(0.0f, 1.0f, 2.0f));
  REQUIRE(fromPoints.normal == Vector3::UP);
  REQUIRE(fromPoints.w == 2.0f);

  const AABox Aabox(Vector3::ZERO, Vector3::UNIT);
  const Plane Plane2AABoxTrue(Vector3::UNIT * .5f, Vector3::RIGHT);
  const Plane Plane2AABoxFalse(Vector3::UNIT * 5.f, Vector3::RIGHT);
  REQUIRE(ShapeOverlap::boxPlane(Aabox, Plane2AABoxTrue));
  REQUIRE_FALSE(ShapeOverlap::boxPlane(Aabox, Plane2AABoxFalse));

  // Planes away from the origin: w used to be subtracted twice.
  REQUIRE(ShapeOverlap::boxPlane(Aabox, Plane(Vector3(0.0f, 0.9f, 0.0f), Vector3::RIGHT)));
  REQUIRE(ShapeOverlap::boxPlane(Aabox, Plane(Vector3(0.0f, 1.0f, 0.0f), Vector3::RIGHT)));
  REQUIRE_FALSE(
      ShapeOverlap::boxPlane(Aabox, Plane(Vector3(0.0f, 1.1f, 0.0f), Vector3::RIGHT)));
  REQUIRE_FALSE(
      ShapeOverlap::boxPlane(Aabox, Plane(Vector3(0.0f, -0.1f, 0.0f), Vector3::RIGHT)));
  REQUIRE(ShapeOverlap::boxPlane(Aabox, Plane(Vector3(0.5f, 0.0f, 0.0f), Vector3::FORWARD)));
}

TEST_CASE("chUtilities - Sphere", "[Math]") {
  REQUIRE(sizeof(Sphere) == 16);

  const Vector<Vector3> ArrayPoints = {
      {-100.0f, -1.0f, 1.0f}, {7.0f, 8.0f, -2.0f}, {1.0f, 1.1f, 1.6f},
      {7.0f, 10.0f, 22.0f},   {4.0f, 2.0f, 1.0f},  {6.0f, 3.1f, 22.6f},
      {-99.0f, -1.0f, 1.0f},  {7.0f, 8.0f, -2.0f}, {1.0f, 1.1f, 1.6f},
      {7.0f, 10.0f, 22.0f},   {4.0f, 2.0f, 1.0f},  {6.0f, 3.1f, 22.6f}};

  const Sphere FromPoints(ArrayPoints);
  REQUIRE(FromPoints.radius == Approx(55.0949364f));
  REQUIRE(FromPoints.center == Vector3(-46.5000000f, 4.5f, 10.30000002f));

  const Sphere Center2(Vector3::UNIT * 3, 1);
  REQUIRE_FALSE(ShapeOverlap::pointSphere(Vector3::ZERO, Center2));
  REQUIRE(ShapeOverlap::pointSphere(Vector3::UNIT * 2.5, Center2));

  const Sphere Center3(Vector3::UNIT * 3, 2);
  const Sphere Center1(Vector3::UNIT, .5f);
  REQUIRE(ShapeOverlap::sphereSphere(Center2, Center3));
  REQUIRE_FALSE(ShapeOverlap::sphereSphere(Center3, Center1));

  const AABox Aabox(Vector3::ZERO, Vector3::UNIT);
  REQUIRE(ShapeOverlap::boxSphere(Aabox, Center1));
  REQUIRE_FALSE(ShapeOverlap::boxSphere(Aabox, Center3));
}

TEST_CASE("chUtilities - Box2D", "[Math]") {
  REQUIRE(sizeof(Sphere) == 16);

  const Box2D UnityBox(Vector2::ZERO, Vector2::UNIT);
  REQUIRE(UnityBox.getCenter() == Vector2(.5f, .5f));
  REQUIRE(UnityBox.getSize() == Vector2(1.f, 1.f));
  REQUIRE(UnityBox.getExtent() == Vector2(.5f, .5f));

  const Box2D AnotherBox(Vector2::UNIT, Vector2::UNIT * 3);
  REQUIRE(AnotherBox.getCenter() == Vector2(2.f, 2.f));
  REQUIRE(AnotherBox.getSize() == Vector2(2.f, 2.f));
  REQUIRE(AnotherBox.getExtent() == Vector2(1.f, 1.f));
}

TEST_CASE("chUtilities - SphereBoxBounds", "[Math]") {
  REQUIRE(sizeof(SphereBoxBounds) == 28);

  const Vector<Vector3> ArrayPoints = {
      {-100.0f, -1.0f, 1.0f}, {7.0f, 8.0f, -2.0f}, {1.0f, 1.1f, 1.6f},
      {7.0f, 10.0f, 22.0f},   {4.0f, 2.0f, 1.0f},  {6.0f, 3.1f, 22.6f},
      {-99.0f, -1.0f, 1.0f},  {7.0f, 8.0f, -2.0f}, {1.0f, 1.1f, 1.6f},
      {7.0f, 10.0f, 22.0f},   {4.0f, 2.0f, 1.0f},  {6.0f, 3.1f, 22.6f}};

  const SphereBoxBounds FromPoints(ArrayPoints);
  const SphereBoxBounds FromSphereBox(Vector3::UNIT, Vector3::UNIT * 5, 6.f);
  const SphereBoxBounds FromSphereBoxTRUE(Vector3::UNIT * -46.f, Vector3::UNIT * 5, 6.f);
  REQUIRE(FromPoints.center == Vector3(-46.5000000f, 4.5f, 10.30000002f));
  REQUIRE(FromPoints.boxExtent == Vector3(53.5f, 5.5f, 12.30000002f));

  // The centers are about 48.5 apart and the radii add up to about 60.6. The old code
  // compared the squared distance with sqrt of the radii, so it said they did not touch.
  REQUIRE(ShapeOverlap::sphereSphere(FromPoints, FromSphereBox));
  REQUIRE_FALSE(ShapeOverlap::boxBox(FromPoints, FromSphereBoxTRUE));

  // Radii 1 and 2 with centers 3 apart touch; one unit further they do not.
  const SphereBoxBounds Touching1(Vector3::ZERO, Vector3::UNIT, 1.0f);
  const SphereBoxBounds Touching2(Vector3(3.0f, 0.0f, 0.0f), Vector3::UNIT, 2.0f);
  const SphereBoxBounds Apart(Vector3(4.0f, 0.0f, 0.0f), Vector3::UNIT, 2.0f);
  REQUIRE(ShapeOverlap::sphereSphere(Touching1, Touching2));
  REQUIRE_FALSE(ShapeOverlap::sphereSphere(Touching1, Apart));
}

TEST_CASE("chUtilities - AABox getTransformed", "[Math]") {
  const AABox unitBox(-Vector3::UNIT, Vector3::UNIT);

  // Turning 45 degrees around Z makes the box sqrt(2) wide in X and Y.
  const AABox turned =
      unitBox.getTransformed(RotationTranslationMatrix(Rotator(0.0f, 45.0f, 0.0f),
                                                       Vector3(10.0f, 0.0f, 0.0f)));
  const float halfDiagonal = Math::sqrt(2.0f);
  REQUIRE(turned.getCenter().nearEqual(Vector3(10.0f, 0.0f, 0.0f), 1e-5f));
  REQUIRE(turned.getExtent().nearEqual(Vector3(halfDiagonal, halfDiagonal, 1.0f), 1e-5f));

  const AABox scaled = AABox(Vector3(1.0f, 1.0f, 1.0f), Vector3(2.0f, 3.0f, 4.0f))
                           .getTransformed(ScaleRotationTranslationMatrix(
                               Vector3(2.0f, 1.0f, 1.0f), Rotator::ZERO, Vector3::ZERO));
  REQUIRE(scaled.minPoint.nearEqual(Vector3(2.0f, 1.0f, 1.0f)));
  REQUIRE(scaled.maxPoint.nearEqual(Vector3(4.0f, 3.0f, 4.0f)));
}

TEST_CASE("chUtilities - RayCast", "[Math]") {
  float distance = -1.0f;
  const Ray alongX(Vector3(-5.0f, 0.5f, 0.5f), Vector3::FORWARD);

  // Axis-aligned box.
  const AABox unitBox(Vector3::ZERO, Vector3::UNIT);
  REQUIRE(RayCast::box(alongX, unitBox, distance));
  REQUIRE(distance == Approx(5.0f));
  REQUIRE(alongX.getPoint(distance).nearEqual(Vector3(0.0f, 0.5f, 0.5f), 1e-5f));
  REQUIRE_FALSE(RayCast::box(Ray(alongX.origin, Vector3::BACKWARD), unitBox, distance));
  REQUIRE_FALSE(RayCast::box(Ray(Vector3(-5.0f, 2.0f, 0.5f), Vector3::FORWARD), unitBox,
                             distance));
  REQUIRE(RayCast::box(Ray(Vector3(0.5f, 0.5f, 0.5f), Vector3::UP), unitBox, distance));
  REQUIRE(distance == 0.0f);

  // Oriented box: turned 90 degrees around Z, its local Y now runs along world X.
  const OrientedBox turnedBox(Vector3(10.0f, 0.0f, 0.0f), Vector3(1.0f, 2.0f, 3.0f),
                              Quaternion(Rotator(0.0f, 90.0f, 0.0f)));
  REQUIRE(RayCast::orientedBox(Ray(Vector3::ZERO, Vector3::FORWARD), turnedBox, distance));
  REQUIRE(distance == Approx(8.0f));
  REQUIRE_FALSE(RayCast::orientedBox(Ray(Vector3(0.0f, 1.5f, 0.0f), Vector3::FORWARD),
                                     turnedBox, distance));

  // Sphere.
  const Sphere ball(Vector3(10.0f, 0.0f, 0.0f), 2.0f);
  REQUIRE(RayCast::sphere(Ray(Vector3::ZERO, Vector3::FORWARD), ball, distance));
  REQUIRE(distance == Approx(8.0f));
  REQUIRE(RayCast::sphere(Ray(Vector3(10.0f, 0.5f, 0.0f), Vector3::UP), ball, distance));
  REQUIRE(distance == 0.0f);
  REQUIRE_FALSE(RayCast::sphere(Ray(Vector3(0.0f, 3.0f, 0.0f), Vector3::FORWARD), ball,
                                distance));
  REQUIRE_FALSE(RayCast::sphere(Ray(Vector3::ZERO, Vector3::BACKWARD), ball, distance));

  // Plane at height 2 facing up, hit from either side.
  const Plane floor(Vector3::UP, 2.0f);
  REQUIRE(RayCast::plane(Ray(Vector3::ZERO, Vector3::UP), floor, distance));
  REQUIRE(distance == Approx(2.0f));
  REQUIRE(RayCast::plane(Ray(Vector3(0.0f, 0.0f, 5.0f), Vector3::DOWN), floor, distance));
  REQUIRE(distance == Approx(3.0f));
  REQUIRE_FALSE(RayCast::plane(Ray(Vector3(0.0f, 0.0f, 5.0f), Vector3::UP), floor, distance));
  REQUIRE_FALSE(RayCast::plane(Ray(Vector3::ZERO, Vector3::FORWARD), floor, distance));

  // Triangle in the plane x = 5, hit from either side.
  const Vector3 a(5.0f, -1.0f, -1.0f);
  const Vector3 b(5.0f, 1.0f, -1.0f);
  const Vector3 c(5.0f, 0.0f, 1.0f);
  REQUIRE(RayCast::triangle(Ray(Vector3::ZERO, Vector3::FORWARD), a, b, c, distance));
  REQUIRE(distance == Approx(5.0f));
  REQUIRE(RayCast::triangle(Ray(Vector3::ZERO, Vector3::FORWARD), a, c, b, distance));
  REQUIRE(RayCast::triangle(Ray(Vector3(10.0f, 0.0f, 0.0f), Vector3::BACKWARD), a, b, c,
                            distance));
  REQUIRE(distance == Approx(5.0f));
  REQUIRE_FALSE(RayCast::triangle(Ray(Vector3(0.0f, 5.0f, 0.0f), Vector3::FORWARD), a, b, c,
                                  distance));
  REQUIRE_FALSE(RayCast::triangle(Ray(Vector3::ZERO, Vector3::BACKWARD), a, b, c, distance));
}

TEST_CASE("chUtilities - OrientedBox", "[Math]") {
  REQUIRE(sizeof(OrientedBox) == 40);

  // 2 long in X, turned 90 degrees around Z, so it spans y in [-2, 2] and x in [-1, 1].
  const OrientedBox turned(Vector3::ZERO, Vector3(2.0f, 1.0f, 1.0f),
                           Quaternion(Rotator(0.0f, 90.0f, 0.0f)));
  REQUIRE(turned.getAxis(0).nearEqual(Vector3::RIGHT, 1e-6f));
  REQUIRE(ShapeOverlap::pointOrientedBox(Vector3(0.0f, 1.5f, 0.0f), turned));
  REQUIRE_FALSE(ShapeOverlap::pointOrientedBox(Vector3(1.5f, 0.0f, 0.0f), turned));
  REQUIRE(turned.getClosestPoint(Vector3(0.0f, 5.0f, 0.0f))
              .nearEqual(Vector3(0.0f, 2.0f, 0.0f), 1e-5f));

  REQUIRE(ShapeOverlap::sphereOrientedBox(Sphere(Vector3(0.0f, 3.0f, 0.0f), 1.1f), turned));
  REQUIRE_FALSE(
      ShapeOverlap::sphereOrientedBox(Sphere(Vector3(0.0f, 3.0f, 0.0f), 0.9f), turned));

  // A cube turned 45 degrees reaches sqrt(2) from its center along X, where its AABox
  // would also reach; at 2.5 from a unit cube the corner falls short, at 2.3 it enters.
  const Quaternion yaw45(Rotator(0.0f, 45.0f, 0.0f));
  const OrientedBox cube(Vector3::ZERO, Vector3::UNIT, Quaternion::IDENTITY);
  const OrientedBox farDiamond(Vector3(2.5f, 0.0f, 0.0f), Vector3::UNIT, yaw45);
  const OrientedBox nearDiamond(Vector3(2.3f, 0.0f, 0.0f), Vector3::UNIT, yaw45);
  REQUIRE_FALSE(ShapeOverlap::orientedBoxOrientedBox(cube, farDiamond));
  REQUIRE(ShapeOverlap::orientedBoxOrientedBox(cube, nearDiamond));
  REQUIRE(ShapeOverlap::orientedBoxOrientedBox(nearDiamond, cube));

  const AABox unitCube(-Vector3::UNIT, Vector3::UNIT);
  REQUIRE_FALSE(ShapeOverlap::boxOrientedBox(unitCube, farDiamond));
  REQUIRE(ShapeOverlap::boxOrientedBox(unitCube, nearDiamond));

  // Two long bars crossed in an X, one above the other: apart when their heights do not
  // meet, touching when they do.
  const OrientedBox barA(Vector3::ZERO, Vector3(5.0f, 0.2f, 0.2f), yaw45);
  const OrientedBox barB(Vector3(0.0f, 0.0f, 0.5f), Vector3(5.0f, 0.2f, 0.2f),
                         Quaternion(Rotator(0.0f, -45.0f, 0.0f)));
  const OrientedBox barC(Vector3(0.0f, 0.0f, 0.3f), Vector3(5.0f, 0.2f, 0.2f),
                         Quaternion(Rotator(0.0f, -45.0f, 0.0f)));
  REQUIRE_FALSE(ShapeOverlap::orientedBoxOrientedBox(barA, barB));
  REQUIRE(ShapeOverlap::orientedBoxOrientedBox(barA, barC));

  // Random pairs: the test must be symmetric, must say true when a corner of one box is
  // inside the other, and false when their bounding spheres do not touch.
  uint32 seed = 12345u;
  const auto random = [&seed](float low, float high) {
    seed = seed * 1664525u + 1013904223u;
    return low + (high - low) * static_cast<float>(seed >> 8) / 16777216.0f;
  };
  const auto randomBox = [&random]() {
    return OrientedBox(
        Vector3(random(-3.0f, 3.0f), random(-3.0f, 3.0f), random(-3.0f, 3.0f)),
        Vector3(random(0.2f, 2.0f), random(0.2f, 2.0f), random(0.2f, 2.0f)),
        Quaternion(Rotator(random(-180.0f, 180.0f), random(-180.0f, 180.0f),
                           random(-180.0f, 180.0f))));
  };
  const auto cornerInside = [](const OrientedBox& corners, const OrientedBox& target) {
    for (int32 corner = 0; corner < 8; ++corner) {
      const Vector3 point = corners.center +
                            corners.getAxis(0) * (corner & 1 ? 1.0f : -1.0f) *
                                corners.extent.x +
                            corners.getAxis(1) * (corner & 2 ? 1.0f : -1.0f) *
                                corners.extent.y +
                            corners.getAxis(2) * (corner & 4 ? 1.0f : -1.0f) *
                                corners.extent.z;
      if (ShapeOverlap::pointOrientedBox(point, target)) {
        return true;
      }
    }
    return false;
  };
  for (int32 pair = 0; pair < 2000; ++pair) {
    const OrientedBox first = randomBox();
    const OrientedBox second = randomBox();
    const bool overlaps = ShapeOverlap::orientedBoxOrientedBox(first, second);
    REQUIRE(overlaps == ShapeOverlap::orientedBoxOrientedBox(second, first));
    if (cornerInside(first, second) || cornerInside(second, first)) {
      REQUIRE(overlaps);
    }
    if (!ShapeOverlap::sphereSphere(Sphere(first.center, first.extent.magnitude()),
                                    Sphere(second.center, second.extent.magnitude()))) {
      REQUIRE_FALSE(overlaps);
    }
  }
}

TEST_CASE("chUtilities - Capsule", "[Math]") {
  REQUIRE(sizeof(Capsule) == 28);

  const Capsule standing(Vector3::ZERO, Vector3(0.0f, 0.0f, 10.0f), 1.0f);
  REQUIRE(standing.getClosestAxisPoint(Vector3(3.0f, 0.0f, 5.0f)) ==
          Vector3(0.0f, 0.0f, 5.0f));
  REQUIRE(standing.getClosestAxisPoint(Vector3(0.0f, 0.0f, 20.0f)) == standing.end);

  REQUIRE(ShapeOverlap::pointCapsule(Vector3(0.5f, 0.0f, 5.0f), standing));
  REQUIRE_FALSE(ShapeOverlap::pointCapsule(Vector3(2.0f, 0.0f, 5.0f), standing));
  REQUIRE(ShapeOverlap::pointCapsule(Vector3(0.0f, 0.0f, 10.9f), standing));
  REQUIRE_FALSE(ShapeOverlap::pointCapsule(Vector3(0.0f, 0.0f, 11.1f), standing));

  REQUIRE(ShapeOverlap::sphereCapsule(Sphere(Vector3(3.0f, 0.0f, 5.0f), 2.0f), standing));
  REQUIRE_FALSE(
      ShapeOverlap::sphereCapsule(Sphere(Vector3(3.0f, 0.0f, 5.0f), 1.9f), standing));

  // Side by side, crossed and end to end.
  const Capsule beside(Vector3(1.5f, 0.0f, 0.0f), Vector3(1.5f, 0.0f, 10.0f), 1.0f);
  const Capsule apart(Vector3(2.5f, 0.0f, 0.0f), Vector3(2.5f, 0.0f, 10.0f), 1.0f);
  const Capsule crossing(Vector3(-5.0f, 1.5f, 5.0f), Vector3(5.0f, 1.5f, 5.0f), 1.0f);
  const Capsule aboveFar(Vector3(0.0f, 0.0f, 12.5f), Vector3(0.0f, 0.0f, 20.0f), 1.0f);
  const Capsule aboveNear(Vector3(0.0f, 0.0f, 11.5f), Vector3(0.0f, 0.0f, 20.0f), 1.0f);
  REQUIRE(ShapeOverlap::capsuleCapsule(standing, beside));
  REQUIRE_FALSE(ShapeOverlap::capsuleCapsule(standing, apart));
  REQUIRE(ShapeOverlap::capsuleCapsule(standing, crossing));
  REQUIRE_FALSE(ShapeOverlap::capsuleCapsule(standing, aboveFar));
  REQUIRE(ShapeOverlap::capsuleCapsule(standing, aboveNear));

  // A capsule of zero length is a sphere.
  const Capsule point(Vector3(0.0f, 0.0f, 12.0f), Vector3(0.0f, 0.0f, 12.0f), 1.0f);
  REQUIRE(ShapeOverlap::capsuleCapsule(standing, point));

  const Plane atFive(Vector3::UP, 5.0f);
  REQUIRE(ShapeOverlap::capsulePlane(standing, atFive));
  REQUIRE_FALSE(ShapeOverlap::capsulePlane(
      Capsule(Vector3(0.0f, 0.0f, 6.5f), Vector3(0.0f, 0.0f, 10.0f), 1.0f), atFive));
  REQUIRE(ShapeOverlap::capsulePlane(
      Capsule(Vector3(0.0f, 0.0f, 6.5f), Vector3(0.0f, 0.0f, 10.0f), 2.0f), atFive));

  REQUIRE(ShapeOverlap::spherePlane(Sphere(Vector3(0.0f, 0.0f, 6.0f), 1.0f), atFive));
  REQUIRE_FALSE(ShapeOverlap::spherePlane(Sphere(Vector3(0.0f, 0.0f, 6.0f), 0.5f), atFive));
}

TEST_CASE("chUtilities - Frustum", "[Math]") {
  REQUIRE(sizeof(Frustum) == 96);

  // Orthographic with no view: x and y in [-10, 10], z (view forward) in [1, 100].
  const Frustum box(OrthographicMatrix(10.0f, 10.0f, 1.0f, 100.0f));
  for (const Plane& plane : box.planes) {
    REQUIRE(plane.normal.magnitude() == Approx(1.0f));
  }
  REQUIRE(box.getPlane(FrustumSide::Near).normal.nearEqual(Vector3(0.0f, 0.0f, 1.0f)));

  REQUIRE(ShapeOverlap::frustumPoint(box, Vector3(0.0f, 0.0f, 50.0f)));
  REQUIRE_FALSE(ShapeOverlap::frustumPoint(box, Vector3(0.0f, 0.0f, 0.5f)));
  REQUIRE_FALSE(ShapeOverlap::frustumPoint(box, Vector3(0.0f, 0.0f, 101.0f)));
  REQUIRE_FALSE(ShapeOverlap::frustumPoint(box, Vector3(11.0f, 0.0f, 50.0f)));
  REQUIRE_FALSE(ShapeOverlap::frustumPoint(box, Vector3(0.0f, -11.0f, 50.0f)));

  REQUIRE(ShapeOverlap::frustumSphere(box, Sphere(Vector3(11.0f, 0.0f, 50.0f), 2.0f)));
  REQUIRE_FALSE(ShapeOverlap::frustumSphere(box, Sphere(Vector3(13.0f, 0.0f, 50.0f), 2.0f)));

  REQUIRE(ShapeOverlap::frustumBox(
      box, AABox(Vector3(9.0f, 0.0f, 50.0f), Vector3(12.0f, 1.0f, 51.0f))));
  REQUIRE_FALSE(ShapeOverlap::frustumBox(
      box, AABox(Vector3(12.0f, 0.0f, 50.0f), Vector3(14.0f, 1.0f, 51.0f))));

  // Bounds are outside when either shape is: here the sphere reaches inside the x = 10
  // plane but the box does not, then the other way around.
  REQUIRE(ShapeOverlap::frustumBounds(
      box, SphereBoxBounds(Vector3(0.0f, 0.0f, 50.0f), Vector3::UNIT, 1.5f)));
  REQUIRE(ShapeOverlap::frustumBounds(
      box, SphereBoxBounds(Vector3(10.5f, 0.0f, 50.0f), Vector3::UNIT, 1.5f)));
  REQUIRE_FALSE(ShapeOverlap::frustumBounds(
      box, SphereBoxBounds(Vector3(11.5f, 0.0f, 50.0f), Vector3(0.4f, 0.4f, 0.4f), 2.0f)));
  REQUIRE_FALSE(ShapeOverlap::frustumBounds(
      box, SphereBoxBounds(Vector3(11.5f, 0.0f, 50.0f), Vector3(2.0f, 2.0f, 2.0f), 1.0f)));
}

TEST_CASE("chUtilities - SphereBoxBounds getTransformed", "[Math]") {
  const SphereBoxBounds bounds(Vector3(1.0f, 0.0f, 0.0f), Vector3(1.0f, 2.0f, 3.0f), 3.5f);
  const ScaleRotationTranslationMatrix matrix(Vector3(2.0f, 1.0f, 1.0f),
                                              Rotator(0.0f, 90.0f, 0.0f),
                                              Vector3(0.0f, 0.0f, 5.0f));

  // The box matches AABox::getTransformed and the sphere grows by the largest scale.
  const SphereBoxBounds transformed = bounds.getTransformed(matrix);
  const AABox expectedBox = bounds.getBox().getTransformed(matrix);
  REQUIRE(transformed.center.nearEqual(expectedBox.getCenter(), 1e-5f));
  REQUIRE(transformed.boxExtent.nearEqual(expectedBox.getExtent(), 1e-5f));
  REQUIRE(transformed.sphereRadius == Approx(7.0f));
  REQUIRE(transformed.center.nearEqual(Vector3(0.0f, 2.0f, 5.0f), 1e-5f));
}

namespace {
/*
 * Small and fixed, so every run tests the same shapes.
 */
struct TestRandom
{
  uint32 state = 12345u;

  float
  next(float low, float high)
  {
    state = state * 1664525u + 1013904223u;
    const float unit = static_cast<float>(state >> 8) / static_cast<float>(1u << 24);
    return low + (high - low) * unit;
  }
};

struct TestShape
{
  bool bCircle;
  Vector2 center;
  float radius;
  Vector2 minPoint;
  Vector2 maxPoint;
};

float
squaredDistanceToTestBox(const Vector2& point, const TestShape& box)
{
  const float dx =
      Math::max(box.minPoint.x - point.x, Math::max(0.0f, point.x - box.maxPoint.x));
  const float dy =
      Math::max(box.minPoint.y - point.y, Math::max(0.0f, point.y - box.maxPoint.y));
  return dx * dx + dy * dy;
}

bool
circleTouches(const TestShape& shape, const Vector2& center, float radius)
{
  if (shape.bCircle) {
    const float reach = shape.radius + radius;
    return (shape.center - center).sqrMagnitude() <= reach * reach;
  }
  return squaredDistanceToTestBox(center, shape) <= radius * radius;
}

bool
boxTouches(const TestShape& shape, const Vector2& low, const Vector2& high)
{
  const TestShape query{false, Vector2::ZERO, 0.0f, low, high};
  if (shape.bCircle) {
    return squaredDistanceToTestBox(shape.center, query) <= shape.radius * shape.radius;
  }
  return shape.minPoint.x <= high.x && shape.maxPoint.x >= low.x &&
         shape.minPoint.y <= high.y && shape.maxPoint.y >= low.y;
}
} // namespace

TEST_CASE("chUtilities - Spatial hash 2D", "[Math]")
{
  SpatialHash2D hash(2.0f);
  Vector<uint32> found;

  SECTION("Queries find the same shapes as testing every one")
  {
    TestRandom random;
    Vector<TestShape> shapes;
    for (uint32 i = 0; i < 2000; ++i) {
      TestShape shape{};
      shape.bCircle = i % 3 != 0;
      const Vector2 center(random.next(-60.0f, 60.0f), random.next(-60.0f, 60.0f));
      if (shape.bCircle) {
        shape.center = center;
        // Some cover many cells, so they are kept apart.
        shape.radius = i % 97 == 0 ? 30.0f : random.next(0.05f, 3.0f);
        hash.addCircle(shape.center, shape.radius, i);
      }
      else {
        const Vector2 half(random.next(0.05f, 4.0f), random.next(0.05f, 4.0f));
        shape.minPoint = center - half;
        shape.maxPoint = center + half;
        // Corners given in any order.
        hash.addBox(shape.maxPoint, shape.minPoint, i);
      }
      shapes.push_back(shape);
    }
    hash.build();
    REQUIRE(hash.getShapeCount() == 2000);

    for (uint32 query = 0; query < 300; ++query) {
      const Vector2 center(random.next(-70.0f, 70.0f), random.next(-70.0f, 70.0f));
      const float radius = query % 50 == 0 ? 200.0f : random.next(0.0f, 6.0f);
      Vector<uint32> expected;
      for (uint32 i = 0; i < shapes.size(); ++i) {
        if (circleTouches(shapes[i], center, radius)) {
          expected.push_back(i);
        }
      }
      hash.queryCircle(center, radius, found);
      Algorithm::sort(found);
      REQUIRE(found == expected);

      const Vector2 half(random.next(0.0f, 5.0f), random.next(0.0f, 5.0f));
      expected.clear();
      for (uint32 i = 0; i < shapes.size(); ++i) {
        if (boxTouches(shapes[i], center - half, center + half)) {
          expected.push_back(i);
        }
      }
      hash.queryBox(center - half, center + half, found);
      Algorithm::sort(found);
      REQUIRE(found == expected);
    }
  }

  SECTION("Touching counts, and a shape over several cells is reported once")
  {
    hash.addCircle(Vector2(0.0f, 0.0f), 1.0f, 7);
    hash.addBox(Vector2(-1.0f, -1.0f), Vector2(9.0f, 1.0f), 8);
    hash.build();
    hash.queryPoint(Vector2(1.0f, 0.0f), found);
    Algorithm::sort(found);
    REQUIRE(found == Vector<uint32>{7, 8});
    hash.queryCircle(Vector2(3.0f, 0.0f), 1.0f, found);
    REQUIRE(found == Vector<uint32>{8});
    hash.queryCircle(Vector2(0.0f, 3.0f), 1.0f, found);
    REQUIRE(found.empty());
    hash.queryBox(Vector2(-20.0f, -20.0f), Vector2(20.0f, 20.0f), found);
    REQUIRE(found.size() == 2);
  }

  SECTION("Negative cells are their own cells")
  {
    hash.addCircle(Vector2(-0.5f, -0.5f), 0.1f, 1);
    hash.addCircle(Vector2(0.5f, 0.5f), 0.1f, 2);
    hash.addCircle(Vector2(-3.5f, 1.5f), 0.1f, 3);
    hash.build();
    hash.queryPoint(Vector2(-0.5f, -0.5f), found);
    REQUIRE(found == Vector<uint32>{1});
    hash.queryCircle(Vector2(-3.5f, 1.5f), 0.2f, found);
    REQUIRE(found == Vector<uint32>{3});
  }

  SECTION("It can be filled again")
  {
    hash.addCircle(Vector2::ZERO, 1.0f, 1);
    hash.build();
    hash.clear();
    REQUIRE(hash.getShapeCount() == 0);
    hash.build();
    hash.queryPoint(Vector2::ZERO, found);
    REQUIRE(found.empty());

    hash.addCircle(Vector2(5.0f, 5.0f), 1.0f, 2);
    hash.build();
    hash.queryPoint(Vector2(5.0f, 5.0f), found);
    REQUIRE(found == Vector<uint32>{2});

    hash.setCellSize(0.0f);
    REQUIRE(hash.getCellSize() == SpatialHash2D::MIN_CELL_SIZE);
    REQUIRE(hash.getShapeCount() == 0);
  }
}
