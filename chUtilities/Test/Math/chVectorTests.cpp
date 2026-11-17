/************************************************************************/
/**
 * @file chVectorTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of the vectors and LinearColor.
 */
/************************************************************************/
#include "chLinearColor.h"
#include "chMath.h"
#include "chVector2.h"
#include "chVector2I.h"
#include "chVector3.h"
#include "chVector3I.h"
#include "chVector4.h"

#include "catch.hpp"

using namespace chEngineSDK;

/************************************************************************/
/*
 * Vectors
 */
/************************************************************************/
TEST_CASE("chUtilities - Vector2", "[Math]") {
  // Vector2{} value-initializes to zero; a plain declaration stays uninitialized on purpose.
  const Vector2 zeroed{};
  REQUIRE(zeroed == Vector2::ZERO);

  // The constants are constexpr.
  static_assert(Vector2::UNIT_X.dot(Vector2::UNIT_Y) == 0.0f);
  static_assert(Vector2::UNIT_X + Vector2::UNIT_Y == Vector2::UNIT);

  const float values[2] = {1.0f, 20.0f};
  REQUIRE(Vector2(values) == Vector2(1.0f, 20.0f));

  REQUIRE(Vector2::UNIT_X.cross(Vector2::UNIT_Y) == 1.0f);
  REQUIRE(Vector2::UNIT_Y.cross(Vector2::UNIT_X) == -1.0f);
  REQUIRE(Vector2(2.0f, 3.0f).dot(Vector2(4.0f, 5.0f)) == 23.0f);

  const Vector2 threeFour(3.0f, 4.0f);
  REQUIRE(threeFour.sqrMagnitude() == 25.0f);
  REQUIRE(threeFour.magnitude() == 5.0f);

  Vector2 toNormalize(15.0f, 0.0f);
  const Vector2 normalized = toNormalize.getNormalized();
  REQUIRE(toNormalize.normalize());
  REQUIRE(toNormalize == normalized);
  REQUIRE(toNormalize == Vector2::UNIT_X);
  REQUIRE(Vector2(1.0f, 1.0f).getNormalized().magnitude() == Approx(1.0f));

  // Squared length 1e-8 is under the default tolerance: normalize refuses and leaves the
  // vector as it was, getNormalized gives ZERO.
  Vector2 tooShort(1.0e-4f, 0.0f);
  REQUIRE_FALSE(tooShort.normalize());
  REQUIRE(tooShort == Vector2(1.0e-4f, 0.0f));
  REQUIRE(tooShort.getNormalized() == Vector2::ZERO);
  REQUIRE(Vector2::ZERO.getNormalized() == Vector2::ZERO);

  const Vector2 projected = Vector2(6.0f, 5.0f).projection(Vector2(10.0f, 3.0f));
  REQUIRE(projected.nearEqual(Vector2(6.88073397f, 2.06422019f)));

  REQUIRE(Vector2(1.0f, 2.0f).nearEqual(Vector2(1.0f + 1.0e-7f, 2.0f)));
  REQUIRE_FALSE(Vector2(1.0f, 2.0f).nearEqual(Vector2(1.001f, 2.0f)));

  REQUIRE(Vector2(1.0f, 2.0f) + Vector2(3.0f, 4.0f) == Vector2(4.0f, 6.0f));
  REQUIRE(Vector2(1.0f, 2.0f) - Vector2(3.0f, 4.0f) == Vector2(-2.0f, -2.0f));
  REQUIRE(-Vector2(1.0f, -2.0f) == Vector2(-1.0f, 2.0f));
  REQUIRE(Vector2(1.0f, 2.0f) * 3.0f == Vector2(3.0f, 6.0f));
  REQUIRE(3.0f * Vector2(1.0f, 2.0f) == Vector2(3.0f, 6.0f));

  Vector2 compound(1.0f, 2.0f);
  compound += Vector2(1.0f, 1.0f);
  REQUIRE(compound == Vector2(2.0f, 3.0f));
  compound -= Vector2(2.0f, 2.0f);
  REQUIRE(compound == Vector2(0.0f, 1.0f));
  compound *= 4.0f;
  REQUIRE(compound == Vector2(0.0f, 4.0f));
}

TEST_CASE("chUtilities - Vector3", "[Math]") {
  const Vector3 zeroed{};
  REQUIRE(zeroed == Vector3::ZERO);

  static_assert(Vector3::FORWARD.cross(Vector3::RIGHT) == Vector3::UP);
  static_assert((Vector3::UP * 2.0f).z == 2.0f);

  // World axes: X forward, Y right, Z up, left-handed.
  REQUIRE(Vector3::FORWARD == Vector3(1.0f, 0.0f, 0.0f));
  REQUIRE(Vector3::RIGHT == Vector3(0.0f, 1.0f, 0.0f));
  REQUIRE(Vector3::UP == Vector3(0.0f, 0.0f, 1.0f));
  REQUIRE(Vector3::BACKWARD == -Vector3::FORWARD);
  REQUIRE(Vector3::LEFT == -Vector3::RIGHT);
  REQUIRE(Vector3::DOWN == -Vector3::UP);
  REQUIRE(Vector3::UNIT == Vector3(1.0f, 1.0f, 1.0f));
  REQUIRE(Vector3::RIGHT.cross(Vector3::UP) == Vector3::FORWARD);
  REQUIRE(Vector3::UP.cross(Vector3::FORWARD) == Vector3::RIGHT);

  const float values[3] = {1.0f, 20.0f, 3.0f};
  REQUIRE(Vector3(values) == Vector3(1.0f, 20.0f, 3.0f));

  REQUIRE(Vector3(1.0f, 2.0f, 3.0f).dot(Vector3(4.0f, 5.0f, 6.0f)) == 32.0f);
  REQUIRE(Vector3(-1.0f, 2.0f, -3.0f).getAbs() == Vector3(1.0f, 2.0f, 3.0f));

  const Vector3 twoThreeSix(2.0f, 3.0f, 6.0f);
  REQUIRE(twoThreeSix.sqrMagnitude() == 49.0f);
  REQUIRE(twoThreeSix.magnitude() == 7.0f);
  REQUIRE(Vector3::ZERO.sqrDistance(twoThreeSix) == 49.0f);
  REQUIRE(twoThreeSix.distance(Vector3::ZERO) == 7.0f);

  Vector3 toNormalize(15.0f, 0.0f, 0.0f);
  const Vector3 normalized = toNormalize.getNormalized();
  REQUIRE(toNormalize.normalize());
  REQUIRE(toNormalize == normalized);
  REQUIRE(toNormalize == Vector3::FORWARD);
  REQUIRE(twoThreeSix.getNormalized().nearEqual(Vector3(2.0f, 3.0f, 6.0f) / 7.0f));

  Vector3 tooShort(1.0e-4f, 0.0f, 0.0f);
  REQUIRE_FALSE(tooShort.normalize());
  REQUIRE(tooShort == Vector3(1.0e-4f, 0.0f, 0.0f));
  REQUIRE(tooShort.getNormalized() == Vector3::ZERO);
  REQUIRE(Vector3::ZERO.getNormalized() == Vector3::ZERO);

  const Vector3 projected =
      Vector3(6.0f, 5.0f, 0.0f).projection(Vector3(10.0f, 3.0f, 0.0f));
  REQUIRE(projected.nearEqual(Vector3(6.88073394f, 2.06422018f, 0.0f)));

  REQUIRE(Vector3::UNIT.nearEqual(Vector3(1.0f, 1.0f + 1.0e-7f, 1.0f)));
  REQUIRE_FALSE(Vector3::UNIT.nearEqual(Vector3(1.0f, 1.0f, 1.001f)));

  REQUIRE(Vector3(1.0f, 2.0f, 3.0f) + Vector3::UNIT == Vector3(2.0f, 3.0f, 4.0f));
  REQUIRE(Vector3(1.0f, 2.0f, 3.0f) - Vector3::UNIT == Vector3(0.0f, 1.0f, 2.0f));
  REQUIRE(Vector3(1.0f, 2.0f, 3.0f) * 2.0f == Vector3(2.0f, 4.0f, 6.0f));
  REQUIRE(2.0f * Vector3(1.0f, 2.0f, 3.0f) == Vector3(2.0f, 4.0f, 6.0f));
  REQUIRE(Vector3(2.0f, 4.0f, 6.0f) / 2.0f == Vector3(1.0f, 2.0f, 3.0f));

  Vector3 compound(1.0f, 2.0f, 3.0f);
  compound += Vector3::UNIT;
  REQUIRE(compound == Vector3(2.0f, 3.0f, 4.0f));
  compound -= Vector3(2.0f, 2.0f, 2.0f);
  REQUIRE(compound == Vector3(0.0f, 1.0f, 2.0f));
  compound *= 3.0f;
  REQUIRE(compound == Vector3(0.0f, 3.0f, 6.0f));
}

TEST_CASE("chUtilities - Vector2I Vector3I", "[Math]") {
  const Vector2I zeroed2{};
  const Vector3I zeroed3{};
  REQUIRE(zeroed2 == Vector2I::ZERO);
  REQUIRE(zeroed3 == Vector3I::ZERO);

  static_assert(Vector2I::UNIT_X + Vector2I::UNIT_Y == Vector2I::UNIT);
  static_assert(Vector3I::UNIT_X + Vector3I::UNIT_Y + Vector3I::UNIT_Z == Vector3I::UNIT);

  // != used to need every component to differ.
  REQUIRE(Vector2I(1, 2) != Vector2I(1, 3));
  REQUIRE(Vector3I(1, 2, 3) != Vector3I(1, 2, 4));
  REQUIRE_FALSE(Vector2I(1, 2) != Vector2I(1, 2));
  REQUIRE_FALSE(Vector3I(1, 2, 3) != Vector3I(1, 2, 3));

  REQUIRE(Vector2I(1, 2) + Vector2I(3, 4) == Vector2I(4, 6));
  REQUIRE(Vector2I(1, 2) - Vector2I(3, 4) == Vector2I(-2, -2));
  REQUIRE(-Vector2I(1, -2) == Vector2I(-1, 2));
  REQUIRE(Vector2I(1, 2) * 3 == Vector2I(3, 6));
  REQUIRE(3 * Vector2I(1, 2) == Vector2I(3, 6));

  REQUIRE(Vector3I(1, 2, 3) + Vector3I::UNIT == Vector3I(2, 3, 4));
  REQUIRE(Vector3I(1, 2, 3) - Vector3I::UNIT == Vector3I(0, 1, 2));
  REQUIRE(-Vector3I(1, -2, 3) == Vector3I(-1, 2, -3));
  REQUIRE(Vector3I(1, 2, 3) * 2 == Vector3I(2, 4, 6));
  REQUIRE(2 * Vector3I(1, 2, 3) == Vector3I(2, 4, 6));

  Vector3I compound(1, 2, 3);
  compound += Vector3I::UNIT;
  compound -= Vector3I(2, 2, 2);
  compound *= 3;
  REQUIRE(compound == Vector3I(0, 3, 6));
}

TEST_CASE("chUtilities - Vector4", "[Math]") {
  const Vector4 zeroed{};
  REQUIRE(zeroed == Vector4::ZERO);

  static_assert(Vector4::UNIT.dot(Vector4::UNIT) == 4.0f);

  REQUIRE(Vector4::UNIT == Vector4(1.0f, 1.0f, 1.0f, 1.0f));

  const float values[4] = {1.0f, 20.0f, 0.0f, 1.0f};
  REQUIRE(Vector4(values) == Vector4(1.0f, 20.0f, 0.0f, 1.0f));

  REQUIRE(Vector4(1.0f, 2.0f, 3.0f, 4.0f).dot(Vector4(5.0f, 6.0f, 7.0f, 8.0f)) == 70.0f);
  REQUIRE(Vector4(-1.0f, 2.0f, -3.0f, -4.0f).getAbs() == Vector4(1.0f, 2.0f, 3.0f, 4.0f));

  // Cross product of xyz; w ends up 0 whatever the inputs held.
  const Vector4 crossed =
      Vector4(0.0f, 1.0f, 0.0f, 1.0f).cross(Vector4(0.0f, 0.0f, 1.0f, 1.0f));
  REQUIRE(crossed == Vector4(1.0f, 0.0f, 0.0f, 0.0f));

  // Every operation uses all four components, w included.
  const Vector4 allOnes = Vector4::UNIT;
  REQUIRE(allOnes.sqrMagnitude() == 4.0f);
  REQUIRE(allOnes.magnitude() == 2.0f);
  REQUIRE(allOnes.getNormalized() == Vector4(0.5f, 0.5f, 0.5f, 0.5f));

  Vector4 point(15.0f, 0.0f, 0.0f, 1.0f);
  REQUIRE(point.sqrMagnitude() == 226.0f);
  const Vector4 normalized = point.getNormalized();
  REQUIRE(point.normalize());
  REQUIRE(point == normalized);
  REQUIRE(point.magnitude() == Approx(1.0f));
  REQUIRE(point.w == Approx(1.0f / Math::sqrt(226.0f)));

  Vector4 tooShort(1.0e-4f, 0.0f, 0.0f, 0.0f);
  REQUIRE_FALSE(tooShort.normalize());
  REQUIRE(tooShort == Vector4(1.0e-4f, 0.0f, 0.0f, 0.0f));
  REQUIRE(tooShort.getNormalized() == Vector4::ZERO);

  REQUIRE(Vector4::UNIT.nearEqual(Vector4(1.0f, 1.0f, 1.0f, 1.0f + 1.0e-7f)));
  REQUIRE_FALSE(Vector4::UNIT.nearEqual(Vector4(1.0f, 1.0f, 1.0f, 1.001f)));

  const Vector4 a(1.0f, 2.0f, 3.0f, 4.0f);
  REQUIRE(a + Vector4::UNIT == Vector4(2.0f, 3.0f, 4.0f, 5.0f));
  REQUIRE(a - Vector4::UNIT == Vector4(0.0f, 1.0f, 2.0f, 3.0f));
  REQUIRE(-a == Vector4(-1.0f, -2.0f, -3.0f, -4.0f));
  REQUIRE(a * 2.0f == Vector4(2.0f, 4.0f, 6.0f, 8.0f));
  REQUIRE(2.0f * a == Vector4(2.0f, 4.0f, 6.0f, 8.0f));

  Vector4 compound = a;
  compound += Vector4::UNIT;
  REQUIRE(compound == Vector4(2.0f, 3.0f, 4.0f, 5.0f));
  compound -= Vector4(2.0f, 2.0f, 2.0f, 2.0f);
  REQUIRE(compound == Vector4(0.0f, 1.0f, 2.0f, 3.0f));
  compound *= 2.0f;
  REQUIRE(compound == Vector4(0.0f, 2.0f, 4.0f, 6.0f));
}

/************************************************************************/
/*
 * LinearColor.
 */
/************************************************************************/
TEST_CASE("chUtilities - LinearColor", "[Math]") {
  static_assert(LinearColor::WHITE * 0.5f == LinearColor(0.5f, 0.5f, 0.5f, 0.5f));
  static_assert(LinearColor::BLACK.a == 1.0f && LinearColor::TRANSPARENT_BLACK.a == 0.0f);

  const LinearColor zeroed{};
  REQUIRE(zeroed == LinearColor(0.0f, 0.0f, 0.0f, 0.0f));
  REQUIRE(LinearColor(0.25f, 0.5f, 0.75f) == LinearColor(0.25f, 0.5f, 0.75f, 1.0f));

  const LinearColor color(0.25f, 0.5f, 0.75f, 0.5f);
  REQUIRE(color + LinearColor::WHITE == LinearColor(1.25f, 1.5f, 1.75f, 1.5f));
  REQUIRE(color - color == LinearColor::TRANSPARENT_BLACK);
  REQUIRE(color * LinearColor::RED == LinearColor(0.25f, 0.0f, 0.0f, 0.5f));
  REQUIRE(color * 2.0f == LinearColor(0.5f, 1.0f, 1.5f, 1.0f));

  LinearColor compound = color;
  compound += LinearColor::WHITE;
  compound -= LinearColor::WHITE;
  REQUIRE(compound == color);
  compound *= LinearColor::GREEN;
  REQUIRE(compound == LinearColor(0.0f, 0.5f, 0.0f, 0.5f));
  compound *= 0.5f;
  REQUIRE(compound == LinearColor(0.0f, 0.25f, 0.0f, 0.25f));

  REQUIRE(color.equals(LinearColor(0.25f, 0.5f, 0.75f, 0.5f + 1.0e-5f)));
  REQUIRE_FALSE(color.equals(LinearColor(0.25f, 0.5f, 0.75f, 0.51f)));

  REQUIRE(LinearColor::lerp(LinearColor::BLACK, LinearColor::WHITE, 0.0f) ==
          LinearColor::BLACK);
  REQUIRE(LinearColor::lerp(LinearColor::BLACK, LinearColor::WHITE, 1.0f) ==
          LinearColor::WHITE);
  REQUIRE(LinearColor::lerp(LinearColor::RED, LinearColor::BLUE, 0.5f) ==
          LinearColor(0.5f, 0.0f, 0.5f, 1.0f));

  LinearColor edited = LinearColor::BLACK;
  edited.getData()[1] = 1.0f;
  REQUIRE(edited == LinearColor::GREEN);
}
