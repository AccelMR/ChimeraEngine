/************************************************************************/
/**
 * @file chRotationTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of Rotator, Matrix4 and Quaternion.
 */
/************************************************************************/
#include <cmath>
#include <limits>

#include "chAngle.h"
#include "chMath.h"
#include "chMatrix4.h"
#include "chMatrixHelpers.h"
#include "chPlane.h"
#include "chQuaternion.h"
#include "chRotator.h"
#include "chVector3.h"
#include "chVector4.h"

#include "catch.hpp"

using namespace chEngineSDK;

namespace {

// Vectors do not convert into each other, and the matrix transforms return a Vector4.
Vector3
xyzOf(const Vector4& vector)
{
  return {vector.x, vector.y, vector.z};
}

bool
isNear(float a, float b, float epsilon = 0.0001f)
{
  return std::abs(a - b) <= epsilon;
}

} // namespace

/************************************************************************/
/*
 * Rotator.
 */
/************************************************************************/
TEST_CASE("chUtilities - Rotator", "[Math]") {
  REQUIRE(sizeof(Rotator) == 12);
  static_assert(Rotator::ZERO == Rotator(0.0f, 0.0f, 0.0f));

  const float nan = std::numeric_limits<float>::quiet_NaN();
  REQUIRE(Rotator(nan, 0.0f, 0.0f).containsNaN());
  REQUIRE(Rotator(0.0f, 0.0f, nan).containsNaN());
  REQUIRE_FALSE(Rotator(10.0f, 20.0f, 30.0f).containsNaN());

  // A tiny negative angle plus 360 rounds to exactly 360, which is outside [0, 360).
  REQUIRE(Rotator::clampAxis(Degree(-1.0e-8f)).valueDegree() == 0.0f);
  REQUIRE(Rotator::clampAxis(Degree(-90.0f)).valueDegree() == 270.0f);

  REQUIRE(Rotator::normalizeAxis(Degree(545.0f)).valueDegree() == Approx(-175.0f));
  REQUIRE(Rotator::normalizeAxis(Degree(720.0f)).valueDegree() == Approx(0.0f));

  REQUIRE(Rotator::clampAxis(Degree(540.f)).valueDegree() == Approx(180.0f));
  REQUIRE(Rotator::clampAxis(Degree(720.f)).valueDegree() == Approx(0.0f));

  Rotator NormalizeRot(720.f, 365.f, 182.f);
  Rotator NormalizedRot = NormalizeRot.getNormalized();
  REQUIRE(NormalizedRot == Rotator(0.f, 5.f, -178.f));

  NormalizeRot.normalize();
  REQUIRE(NormalizedRot == NormalizeRot);

  Rotator DenormalizeRot(720.f, 450.f, -545.f);
  Rotator DenormalizedRot = DenormalizeRot.getDenormalized();
  REQUIRE(DenormalizedRot == Rotator(0.f, 90.f, 175.f));

  DenormalizeRot.normalize();
  REQUIRE(DenormalizeRot == DenormalizeRot);
}

/************************************************************************/
/*
 * Matrix
 */
/************************************************************************/
namespace {
// Covers every branch of Quaternion(Matrix4): positive trace and each largest diagonal.
const Rotator kTestRotators[] = {
    Rotator(0.0f, 0.0f, 0.0f),      Rotator(30.0f, 0.0f, 0.0f),
    Rotator(0.0f, 30.0f, 0.0f),     Rotator(0.0f, 0.0f, 30.0f),
    Rotator(20.0f, 40.0f, 60.0f),   Rotator(0.0f, 180.0f, 0.0f),
    Rotator(0.0f, 180.0f, 180.0f),  Rotator(170.0f, 10.0f, 0.0f),
    Rotator(10.0f, 20.0f, 175.0f),  Rotator(80.0f, -120.0f, 33.0f),
    Rotator(-45.0f, 135.0f, -90.0f), Rotator(10.0f, 170.0f, 175.0f)};

bool
isSameRotation(const Matrix4& matrix, const Quaternion& quaternion)
{
  for (const Vector3& axis : {Vector3::FORWARD, Vector3::RIGHT, Vector3::UP}) {
    if (!xyzOf(matrix.transformVector(axis)).nearEqual(quaternion.rotateVector(axis), 1e-5f)) {
      return false;
    }
  }
  return true;
}

Matrix4
multiplyReference(const Matrix4& a, const Matrix4& b)
{
  Matrix4 result = Matrix4::ZERO;
  for (int32 row = 0; row < 4; ++row) {
    for (int32 column = 0; column < 4; ++column) {
      for (int32 k = 0; k < 4; ++k) {
        result[row][column] += a[row][k] * b[k][column];
      }
    }
  }
  return result;
}
} // namespace

TEST_CASE("chUtilities - Matrix4", "[Math]") {
  static_assert(sizeof(Matrix4) == 16 * 4);
  static_assert(alignof(Matrix4) == 16);
  static_assert(std::is_trivially_copyable_v<Matrix4>);
  static_assert(Matrix4::IDENTITY.data()[0] == 1.0f && Matrix4::IDENTITY.data()[1] == 0.0f);

  const Matrix4 Identity(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                         0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
  REQUIRE(Identity == Matrix4::IDENTITY);

  Matrix4 Temporal1(Identity);
  REQUIRE(Identity == Temporal1);

  Matrix4 Temporal2(9.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 1.0f);
  Temporal1.at(0, 0) = 9.0f;
  REQUIRE(Temporal1 == Temporal2);

  const Matrix4 ActualResult(9.0f, 9.0f, 9.0f, 9.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                             1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
  REQUIRE(Temporal1 * Matrix4::UNITY == ActualResult);

  Temporal1 *= Matrix4::UNITY;
  REQUIRE(Temporal1 == ActualResult);

  Matrix4 AdditionResult = Temporal1 + ActualResult;
  const Matrix4 RealAdditionResult(18.0f, 18.0f, 18.0f, 18.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f,
                                   2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f);
  REQUIRE(AdditionResult == RealAdditionResult);
  REQUIRE(AdditionResult - ActualResult == ActualResult);

  const Matrix4 ValMulFixedresult(4.0f, 0.0f, 0.0f, 0.0f, 0.0f, 4.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                  4.0f, 0.0f, 0.0f, 0.0f, 0.0f, 4.0f);
  REQUIRE(Matrix4::IDENTITY * 4.0f == ValMulFixedresult);

  Temporal2 *= 0.0f;
  REQUIRE(Temporal2 == Matrix4::ZERO);

  Temporal1.setIdentity();
  REQUIRE(Temporal1 == Matrix4::IDENTITY);

  Temporal2 = RealAdditionResult.getTransposed();
  const Matrix4 TransposedResult(18.0f, 2.0f, 2.0f, 2.0f, 18.0f, 2.0f, 2.0f, 2.0f, 18.0f, 2.0f,
                                 2.0f, 2.0f, 18.0f, 2.0f, 2.0f, 2.0f);
  REQUIRE(Temporal2 == TransposedResult);

  Temporal2.transpose();
  REQUIRE(Temporal2 == RealAdditionResult);

  // SIMD multiplication against a plain triple loop.
  const Matrix4 A(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f,
                  13.0f, 14.0f, 15.0f, 16.0f);
  const Matrix4 B(0.5f, -1.0f, 2.0f, 0.0f, 3.0f, 0.25f, -2.0f, 1.0f, -1.5f, 4.0f, 1.0f, 2.0f,
                  0.0f, 1.0f, -3.0f, 0.75f);
  REQUIRE(A * B == multiplyReference(A, B));
  REQUIRE(B * A == multiplyReference(B, A));
  Matrix4 selfMultiplied = A;
  selfMultiplied *= selfMultiplied;
  REQUIRE(selfMultiplied == multiplyReference(A, A));

  // Determinant and inverse.
  REQUIRE(RealAdditionResult.getDeterminant() == Approx(0.0f));
  REQUIRE(RealAdditionResult.getInverse() == Matrix4::IDENTITY);

  const Matrix4 ToInverse(1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 3.0f, 1.0f, 2.0f, 2.0f, 3.0f, 1.0f,
                          0.0f, 1.0f, 0.0f, 2.0f, 1.0f);
  const Matrix4 KnownInverse(-3.0f, -0.5f, 1.5f, 1.0f, 1.0f, 0.25f, -0.25f, -0.5f, 3.0f, 0.25f,
                             -1.25f, -0.5f, -3.0f, 0.0f, 1.0f, 1.0f);
  REQUIRE(ToInverse.getInverse().nearEqual(KnownInverse, 1e-5f));
  REQUIRE(ToInverse.getDeterminant() == Approx(-4.0f));
  REQUIRE((ToInverse * ToInverse.getInverse()).nearEqual(Matrix4::IDENTITY, 1e-5f));
  REQUIRE(Vector4(ToInverse.getRow(1)) == Vector4(0.0f, 3.0f, 1.0f, 2.0f));

  // Small scales used to be taken as singular and returned IDENTITY.
  for (const Vector3& scale : {Vector3(1.0f, 1.0f, 1.0f), Vector3(2.0f, 3.0f, 0.5f),
                               Vector3(0.001f, 0.002f, 0.001f), Vector3(-1.0f, 1.0f, 4.0f)}) {
    const ScaleRotationTranslationMatrix srt(scale, Rotator(20.0f, 40.0f, 60.0f),
                                             Vector3(5.0f, -2.0f, 7.0f));
    REQUIRE(srt.getDeterminant() == Approx(scale.x * scale.y * scale.z));

    const Matrix4 inverse = srt.getInverse();
    REQUIRE((inverse * srt).nearEqual(Matrix4::IDENTITY, 1e-4f));
    // With scale 0.001 the inverse moves about 5000 units, and float rounding of numbers
    // that big cancelling each other is around 1e-3.
    REQUIRE((srt * inverse).nearEqual(Matrix4::IDENTITY, 1e-2f));
    REQUIRE(srt.getInverseAffine().nearEqual(inverse, 1e-2f));
    REQUIRE((srt.getInverseAffine() * srt).nearEqual(Matrix4::IDENTITY, 1e-4f));
  }
  const ScaleRotationTranslationMatrix flat(Vector3(0.0f, 1.0f, 1.0f), Rotator::ZERO,
                                            Vector3::ZERO);
  REQUIRE(flat.getInverseAffine() == Matrix4::IDENTITY);

  // Transforms.
  const TranslationMatrix T(Vector3(10.0f, 2.0f, 1.0f));
  REQUIRE(T.transformPosition(Vector3(1.8f, 52.f, 26.6f)) == Vector4(11.8f, 54.f, 27.6f, 1.0f));
  REQUIRE(T.transformVector(Vector3(1.8f, 52.f, 26.6f)) == Vector4(1.8f, 52.f, 26.6f, 0.0f));
  REQUIRE(A.transformVector4(Vector4(1.0f, 2.0f, 3.0f, 4.0f)) ==
          Vector4(90.0f, 100.0f, 110.0f, 120.0f));

  REQUIRE(sizeof(TranslationMatrix) == 16 * 4);
  const TranslationMatrix PositionMat(Vector3(2.0f, 3.0f, 150.0f));
  REQUIRE(Vector4(PositionMat.getRow(3)) == Vector4(2.0f, 3.0f, 150.0f, 1.0f));

  // X forward, Y right, Z up: positive pitch turns forward up, positive yaw turns it right,
  // positive roll turns right down.
  const float c30 = Math::cos(Degree(30.0f));
  const float s30 = Math::sin(Degree(30.0f));
  REQUIRE(xyzOf(RotationMatrix(Rotator(30.0f, 0.0f, 0.0f)).transformVector(Vector3::FORWARD))
              .nearEqual(Vector3(c30, 0.0f, s30), 1e-6f));
  REQUIRE(xyzOf(RotationMatrix(Rotator(0.0f, 30.0f, 0.0f)).transformVector(Vector3::FORWARD))
              .nearEqual(Vector3(c30, s30, 0.0f), 1e-6f));
  REQUIRE(xyzOf(RotationMatrix(Rotator(0.0f, 0.0f, 30.0f)).transformVector(Vector3::RIGHT))
              .nearEqual(Vector3(0.0f, c30, -s30), 1e-6f));

  for (const Rotator& rotator : kTestRotators) {
    const RotationMatrix pure(rotator);
    REQUIRE((pure * pure.getTransposed()).nearEqual(Matrix4::IDENTITY, 1e-5f));
    REQUIRE(pure.getDeterminant() == Approx(1.0f));
    REQUIRE(Vector4(pure.getRow(3)) == Vector4(0.0f, 0.0f, 0.0f, 1.0f));
    REQUIRE(RotationMatrix(pure.rotator()).nearEqual(pure, 1e-4f));
    REQUIRE(isSameRotation(pure, pure.toQuaternion()));

    const RotationTranslationMatrix moved(rotator, Vector3(1.0f, 2.0f, 3.0f));
    REQUIRE(moved == pure * TranslationMatrix(Vector3(1.0f, 2.0f, 3.0f)));
  }
  REQUIRE(RotationMatrix(Rotator(30.0f, 0.0f, 0.0f))
              .rotator()
              .nearEqual(Rotator(30.0f, 0.0f, 0.0f), 1e-3f));
  REQUIRE(RotationMatrix(Rotator(20.0f, 40.0f, 60.0f))
              .rotator()
              .nearEqual(Rotator(20.0f, 40.0f, 60.0f), 1e-3f));

  // Scale, then rotation, then translation.
  const Vector3 scale(2.0f, 3.0f, 0.5f);
  const Rotator rotation(20.0f, 40.0f, 60.0f);
  const Vector3 origin(5.0f, -2.0f, 7.0f);
  const Matrix4 scaleMatrix(scale.x, 0.0f, 0.0f, 0.0f, 0.0f, scale.y, 0.0f, 0.0f, 0.0f, 0.0f,
                            scale.z, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
  REQUIRE(ScaleRotationTranslationMatrix(scale, rotation, origin)
              .nearEqual(scaleMatrix * RotationMatrix(rotation) * TranslationMatrix(origin),
                         1e-5f));

  // The quaternion version gives the same matrix as the rotator it comes from, also for
  // angles past 90 degrees.
  for (const Rotator& testRotation : {rotation, Rotator(-75.0f, 170.0f, -120.0f),
                                      Rotator(89.0f, -30.0f, 10.0f), Rotator::ZERO}) {
    REQUIRE(ScaleRotationTranslationMatrix(scale, Quaternion(testRotation), origin)
                .nearEqual(ScaleRotationTranslationMatrix(scale, testRotation, origin),
                           1e-5f));
  }

  // decompose gives back the parts, a mirror included, and refuses a zero scale.
  {
    Vector3 outScale;
    Quaternion outRotation;
    Vector3 outTranslation;
    const ScaleRotationTranslationMatrix srt(scale, Quaternion(rotation), origin);
    REQUIRE(srt.decompose(outScale, outRotation, outTranslation));
    REQUIRE(outScale.nearEqual(scale, 1e-4f));
    REQUIRE(outTranslation.nearEqual(origin, 1e-4f));
    REQUIRE(ScaleRotationTranslationMatrix(outScale, outRotation, outTranslation)
                .nearEqual(srt, 1e-4f));

    const Vector3 mirrored(-2.0f, 3.0f, 0.5f);
    const ScaleRotationTranslationMatrix mirror(mirrored, Quaternion(rotation), origin);
    REQUIRE(mirror.decompose(outScale, outRotation, outTranslation));
    REQUIRE(outScale.x < 0.0f);
    REQUIRE(ScaleRotationTranslationMatrix(outScale, outRotation, outTranslation)
                .nearEqual(mirror, 1e-4f));

    const ScaleRotationTranslationMatrix zeroScale(Vector3(1.0f, 0.0f, 1.0f), Rotator::ZERO,
                                                   origin);
    REQUIRE_FALSE(zeroScale.decompose(outScale, outRotation, outTranslation));
  }

  // Test PerspectiveMatrix
  Radian halfFOV(Math::PI / 4.0f); // 45 degrees
  PerspectiveMatrix perspective(halfFOV, 800.0f, 600.0f, 0.1f, 1000.0f);

  // Check key elements of the perspective matrix
  float expectedValue = 1.0f / Math::tan(halfFOV);
  REQUIRE(perspective.at(0, 0) == Approx(expectedValue));
  REQUIRE(perspective.at(1, 1) == Approx((800.0f / Math::tan(halfFOV)) / 600.0f));
  REQUIRE(perspective.at(2, 2) == Approx(1000.0f / 999.9f));
  REQUIRE(perspective.at(2, 3) == 1.0f);
  REQUIRE(perspective.at(3, 2) == Approx(-0.1f * 1000.0f / 999.9f));


  float width = 1920.0f;
  float height = 1080.0f;
  float nearPlane = 0.5f;
  float farPlane = 500.0f;
  Radian halfFOV2(Math::PI / 3.0f); // 60 degrees
  perspective = PerspectiveMatrix(halfFOV2, width, height, nearPlane, farPlane);

  float expectedX = 1.0f / Math::tan(halfFOV2);
  float expectedY = (width / Math::tan(halfFOV2)) / height;
  float expectedZ = farPlane / (farPlane - nearPlane);
  float expectedW = -(nearPlane * farPlane) / (farPlane - nearPlane);

  // Pruebas usando isNear para permitir pequeñas diferencias de precisión
  REQUIRE(isNear(perspective.at(0, 0), expectedX));
  REQUIRE(isNear(perspective.at(1, 1), expectedY));
  REQUIRE(isNear(perspective.at(2, 2), expectedZ));
  REQUIRE(isNear(perspective.at(2, 3), 1.0f));
  REQUIRE(isNear(perspective.at(3, 2), expectedW));
  REQUIRE(isNear(perspective.at(3, 3), 0.0f));

  // Asegurarse de que otros elementos que deben ser cero realmente lo sean
  REQUIRE(isNear(perspective.at(0, 1), 0.0f));
  REQUIRE(isNear(perspective.at(0, 2), 0.0f));
  REQUIRE(isNear(perspective.at(0, 3), 0.0f));
  REQUIRE(isNear(perspective.at(1, 0), 0.0f));
  REQUIRE(isNear(perspective.at(1, 2), 0.0f));
  REQUIRE(isNear(perspective.at(1, 3), 0.0f));
  REQUIRE(isNear(perspective.at(2, 0), 0.0f));
  REQUIRE(isNear(perspective.at(2, 1), 0.0f));
  REQUIRE(isNear(perspective.at(3, 0), 0.0f));
  REQUIRE(isNear(perspective.at(3, 1), 0.0f));

  // View space is X right, Y up, Z forward, like D3DXMatrixLookAtLH.
  const LookAtMatrix lookForward(Vector3::ZERO, Vector3::FORWARD * 10.0f, Vector3::UP);
  REQUIRE(lookForward.transformPosition(Vector3(10.0f, 5.0f, 0.0f)) ==
          Vector4(5.0f, 0.0f, 10.0f, 1.0f));
  REQUIRE(lookForward.transformPosition(Vector3(10.0f, 0.0f, 5.0f)) ==
          Vector4(0.0f, 5.0f, 10.0f, 1.0f));

  const Vector3 eyePos(3.0f, 2.0f, 1.0f);
  const Vector3 target(-1.0f, 4.0f, 2.0f);
  const LookAtMatrix lookAtMatrix(eyePos, target, Vector3::UP);
  REQUIRE(xyzOf(lookAtMatrix.transformPosition(eyePos)).nearEqual(Vector3::ZERO, 1e-5f));
  REQUIRE(xyzOf(lookAtMatrix.transformPosition(target))
              .nearEqual(Vector3(0.0f, 0.0f, (target - eyePos).magnitude()), 1e-5f));
  REQUIRE((lookAtMatrix * lookAtMatrix.getInverseAffine()).nearEqual(Matrix4::IDENTITY, 1e-5f));
  REQUIRE(lookAtMatrix.getDeterminant() == Approx(1.0f));

  // Looking straight up still gives a valid view.
  const LookAtMatrix lookUp(Vector3::ZERO, Vector3::UP, Vector3::UP);
  REQUIRE(lookUp.getDeterminant() == Approx(1.0f));
  REQUIRE(xyzOf(lookUp.transformPosition(Vector3::UP)).nearEqual(Vector3(0.0f, 0.0f, 1.0f),
                                                                     1e-5f));

  // Clip space is X right, Y up, depth 0 at near and 1 at far.
  const Matrix4 viewProj = lookForward * PerspectiveMatrix(Radian(Math::PI * 0.25f), 800.0f,
                                                           600.0f, 1.0f, 100.0f);
  Vector4 clip = viewProj.transformPosition(Vector3(10.0f, 5.0f, 5.0f));
  REQUIRE(clip.x / clip.w > 0.0f);
  REQUIRE(clip.y / clip.w > 0.0f);
  clip = viewProj.transformPosition(Vector3(1.0f, 0.0f, 0.0f));
  REQUIRE(clip.z / clip.w == Approx(0.0f).margin(1e-6f));
  clip = viewProj.transformPosition(Vector3(100.0f, 0.0f, 0.0f));
  REQUIRE(clip.z / clip.w == Approx(1.0f));

  const OrthographicMatrix ortho(4.0f, 2.0f, 1.0f, 11.0f);
  REQUIRE(ortho.transformPosition(Vector3(4.0f, 2.0f, 1.0f)) == Vector4(1.0f, 1.0f, 0.0f, 1.0f));
  REQUIRE(ortho.transformPosition(Vector3(-4.0f, -2.0f, 11.0f)) ==
          Vector4(-1.0f, -1.0f, 1.0f, 1.0f));
}

/************************************************************************/
/*
 * Quaternion.
 */
/************************************************************************/
TEST_CASE("chUtilities - Quaternion", "[Math]") {
  REQUIRE(sizeof(Quaternion) == 4 * 4);

  // The default constructor leaves the values uninitialized on purpose; no rotation is
  // IDENTITY.
  static_assert(Quaternion::IDENTITY == Quaternion(0.0f, 0.0f, 0.0f, 1.0f));

  // Positive pitch turns forward up, positive yaw turns it right, positive roll turns
  // right down, the same as RotationMatrix.
  const Quaternion quatPitch90(Rotator(90.0f, 0.0f, 0.0f));
  const Quaternion quatYaw90(Rotator(0.0f, 90.0f, 0.0f));
  const Quaternion quatRoll90(Rotator(0.0f, 0.0f, 90.0f));
  REQUIRE(quatPitch90.rotateVector(Vector3::FORWARD).nearEqual(Vector3::UP, 1e-5f));
  REQUIRE(quatYaw90.rotateVector(Vector3::FORWARD).nearEqual(Vector3::RIGHT, 1e-5f));
  REQUIRE(quatRoll90.rotateVector(Vector3::RIGHT).nearEqual(Vector3::DOWN, 1e-5f));
  REQUIRE(quatPitch90.nearEqual(Quaternion(0.0f, -0.707106769f, 0.0f, 0.707106769f), 1e-6f));
  REQUIRE(quatYaw90.nearEqual(Quaternion(0.0f, 0.0f, 0.707106769f, 0.707106769f), 1e-6f));
  REQUIRE(quatRoll90.nearEqual(Quaternion(-0.707106769f, 0.0f, 0.0f, 0.707106769f), 1e-6f));
  REQUIRE(Rotator(20.0f, 40.0f, 60.0f).toQuaternion() ==
          Quaternion(Rotator(20.0f, 40.0f, 60.0f)));

  REQUIRE(Quaternion(Rotator(45.0f, 45.0f, 0.0f))
              .rotateVector(Vector3::FORWARD)
              .nearEqual(Vector3(0.5f, 0.5f, 0.707106769f), 1e-5f));

  for (const Rotator& rotator : kTestRotators) {
    const RotationMatrix matrix(rotator);
    const Quaternion fromRotator(rotator);
    REQUIRE(isSameRotation(matrix, fromRotator));
    REQUIRE(isSameRotation(matrix, Quaternion(static_cast<const Matrix4&>(matrix))));

    // Euler angles have more than one answer for the same rotation, so the round trip
    // compares rotations, not angles.
    REQUIRE(RotationMatrix(fromRotator.toRotator()).nearEqual(matrix, 1e-4f));
  }

  // At pitch +-90 only yaw minus roll can be recovered, but the rotation must match.
  for (const Rotator& rotator : {Rotator(90.0f, 30.0f, 10.0f), Rotator(-90.0f, 30.0f, 10.0f)}) {
    const Rotator back = Quaternion(rotator).toRotator();
    REQUIRE(RotationMatrix(back).nearEqual(RotationMatrix(rotator), 1e-3f));
  }

  // Axis-angle follows the right-hand formula, so +90 around the right axis turns forward
  // down, which is pitch -90.
  const Quaternion axisAngleQuat(Vector3::RIGHT, Degree(90.0f));
  REQUIRE(axisAngleQuat.rotateVector(Vector3::FORWARD).nearEqual(Vector3::DOWN, 1e-5f));
  REQUIRE(axisAngleQuat.isRotationEqual(Quaternion(Rotator(-90.0f, 0.0f, 0.0f)), 1e-6f));

  Quaternion conjugated = quatYaw90;
  conjugated.conjugate();
  REQUIRE(conjugated.nearEqual(quatYaw90.getConjugated()));
  REQUIRE(conjugated.nearEqual(quatYaw90.getInverse(), 1e-6f));

  Quaternion nonUnit(1.0f, 2.0f, 3.0f, 4.0f);
  const Quaternion normalized = nonUnit.getNormalized();
  nonUnit.normalize();
  REQUIRE(nonUnit.nearEqual(normalized));
  REQUIRE(normalized.length() == Approx(1.0f));

  // Too short to normalize: normalize refuses, getNormalized gives IDENTITY.
  Quaternion tiny(1.0e-4f, 0.0f, 0.0f, 0.0f);
  REQUIRE_FALSE(tiny.normalize());
  REQUIRE(tiny == Quaternion(1.0e-4f, 0.0f, 0.0f, 0.0f));
  REQUIRE(tiny.getNormalized() == Quaternion::IDENTITY);

  // An axis of length zero has no direction to turn around.
  REQUIRE(Quaternion(Vector3::ZERO, Degree(90.0f)) == Quaternion::IDENTITY);
  // The axis does not need to be unit length.
  REQUIRE(Quaternion(Vector3::RIGHT * 5.0f, Degree(90.0f)).nearEqual(axisAngleQuat, 1e-6f));

  REQUIRE(Quaternion(std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 1.0f).containsNaN());
  REQUIRE_FALSE(Quaternion::IDENTITY.containsNaN());

  static_assert(Quaternion::IDENTITY.rotateVector(Vector3::FORWARD) == Vector3::FORWARD);

  const Vector3 Right = quatYaw90.rotateVector(Vector3::FORWARD);
  const Vector3 Backwards = quatYaw90.rotateVector(Right);
  REQUIRE(Right.nearEqual(Vector3::RIGHT, Math::KINDA_SMALL_NUMBER));
  REQUIRE(Backwards.nearEqual(-Vector3::FORWARD, Math::KINDA_SMALL_NUMBER));

  Quaternion unitQuat = Quaternion::IDENTITY;
  REQUIRE(unitQuat.squaredLength() == Approx(1.0f));
  REQUIRE(unitQuat.length() == Approx(1.0f));

  Quaternion twoQuat(2.0f, 0.0f, 0.0f, 0.0f);
  REQUIRE(twoQuat.squaredLength() == Approx(4.0f));
  REQUIRE(twoQuat.length() == Approx(2.0f));

  Quaternion q1(1.0f, 2.0f, 3.0f, 4.0f);
  Quaternion q2(5.0f, 6.0f, 7.0f, 8.0f);
  Quaternion sum = q1 + q2;
  REQUIRE(sum.x == 6.0f);
  REQUIRE(sum.y == 8.0f);
  REQUIRE(sum.z == 10.0f);
  REQUIRE(sum.w == 12.0f);

  Quaternion scaled = q1 * 2.0f;
  REQUIRE(scaled.x == 2.0f);
  REQUIRE(scaled.y == 4.0f);
  REQUIRE(scaled.z == 6.0f);
  REQUIRE(scaled.w == 8.0f);

  Quaternion inPlace = q1;
  inPlace *= 2.0f;
  REQUIRE(inPlace.x == 2.0f);
  REQUIRE(inPlace.y == 4.0f);
  REQUIRE(inPlace.z == 6.0f);
  REQUIRE(inPlace.w == 8.0f);

  // A * B applies B first, then A.
  Quaternion qRoll45(Rotator(0.0f, 0.0f, 45.0f));
  Quaternion qPitch45(Rotator(45.0f, 0.0f, 0.0f));
  Quaternion combined = qRoll45 * qPitch45;
  Vector3 testVec(1.0f, 0.0f, 0.0f);
  Vector3 rotatedTwice = qRoll45.rotateVector(qPitch45.rotateVector(testVec));
  REQUIRE(rotatedTwice.nearEqual(combined.rotateVector(testVec), Math::SMALL_NUMBER));

  Quaternion arbitrary(0.1f, 0.2f, 0.3f, 0.4f);
  arbitrary.normalize();
  Quaternion shouldBeIdentity = arbitrary * arbitrary.getInverse();
  REQUIRE(shouldBeIdentity.nearEqual(Quaternion::IDENTITY, Math::SMALL_NUMBER));

  Vector3 originalVec(1.0f, 2.0f, 3.0f);
  Vector3 unrotated = arbitrary.unrotateVector(arbitrary.rotateVector(originalVec));
  REQUIRE(unrotated.nearEqual(originalVec, Math::SMALL_NUMBER));

  // rotateVector must match the rotation matrix of the same quaternion.
  const Matrix4 arbitraryMatrix = RotationMatrix(arbitrary.toRotator());
  REQUIRE(xyzOf(arbitraryMatrix.transformVector(originalVec))
              .nearEqual(arbitrary.rotateVector(originalVec), 1e-4f));
}

/**********************************************************************/
/*
 *                            Shapes
 */
/***********************************************************************/
TEST_CASE("chUtilities - Quaternion interpolation", "[Math]")
{
  const Quaternion start = Quaternion::IDENTITY;
  const Quaternion end(Vector3::UP, Degree(120.0f));

  SECTION("Slerp turns at constant speed")
  {
    REQUIRE(Quaternion::slerp(start, end, 0.0f).isRotationEqual(start, 1e-6f));
    REQUIRE(Quaternion::slerp(start, end, 1.0f).isRotationEqual(end, 1e-6f));
    for (const float alpha : {0.25f, 0.5f, 0.75f}) {
      const Quaternion expected(Vector3::UP, Degree(120.0f * alpha));
      const Quaternion result = Quaternion::slerp(start, end, alpha);
      REQUIRE(result.isRotationEqual(expected, 1e-6f));
      REQUIRE(Math::abs(result.length() - 1.0f) < 1e-5f);
    }
  }

  SECTION("Both take the short way when the keys have opposite signs")
  {
    const Quaternion negatedEnd = end * -1.0f;
    const Quaternion expected(Vector3::UP, Degree(60.0f));
    REQUIRE(Quaternion::slerp(start, negatedEnd, 0.5f).isRotationEqual(expected, 1e-6f));
    REQUIRE(Quaternion::nlerp(start, negatedEnd, 0.5f).isRotationEqual(expected, 1e-6f));
  }

  SECTION("Nlerp follows the same path, close to slerp for near keys")
  {
    // Halfway the two agree exactly; elsewhere nlerp is off by a little.
    REQUIRE(Quaternion::nlerp(start, end, 0.5f)
                .isRotationEqual(Quaternion::slerp(start, end, 0.5f), 1e-6f));
    const Quaternion nearEnd(Vector3::UP, Degree(12.0f));
    const Quaternion nlerped = Quaternion::nlerp(start, nearEnd, 0.3f);
    REQUIRE(Math::abs(nlerped.length() - 1.0f) < 1e-5f);
    const Vector3 expected =
        Quaternion(Vector3::UP, Degree(3.6f)).rotateVector(Vector3::FORWARD);
    REQUIRE(nlerped.rotateVector(Vector3::FORWARD).nearEqual(expected, 1e-3f));
  }

  SECTION("Almost equal keys do not divide by zero")
  {
    const Quaternion almost(Vector3::UP, Degree(0.01f));
    REQUIRE_FALSE(Quaternion::slerp(start, almost, 0.5f).containsNaN());
  }
}
