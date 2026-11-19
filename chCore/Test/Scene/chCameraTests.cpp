/************************************************************************/
/**
 * @file chCameraTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of Camera and CameraComponent.
 */
/************************************************************************/
#include "chAngle.h"
#include "chBox.h"
#include "chCamera.h"
#include "chCameraComponent.h"
#include "chComponentRegister.h"
#include "chFileStream.h"
#include "chMath.h"
#include "chQuaternion.h"
#include "chRay.h"
#include "chRotator.h"
#include "chScene.h"
#include "chShapeOverlap.h"
#include "chSphere.h"
#include "chUUID.h"
#include "chVector2.h"
#include "chVector3.h"
#include "chVector4.h"

#include "catch.hpp"
#include "chCoreTestHelpers.h"

using namespace chEngineSDK;

namespace {
constexpr float kTolerance = 0.0001f;
} // namespace

TEST_CASE("chCore - Camera view", "[Camera]")
{
  const Camera camera = createTestCamera();

  // View space is X right, Y up, Z forward.
  const Vector4 eye = camera.getViewMatrix().transformPosition(camera.getPosition());
  REQUIRE(eye.x == Approx(0.0f).margin(kTolerance));
  REQUIRE(eye.y == Approx(0.0f).margin(kTolerance));
  REQUIRE(eye.z == Approx(0.0f).margin(kTolerance));

  const Vector4 target = camera.getViewMatrix().transformPosition(Vector3::ZERO);
  REQUIRE(target.x == Approx(0.0f).margin(kTolerance));
  REQUIRE(target.y == Approx(0.0f).margin(kTolerance));
  REQUIRE(target.z == Approx(5.0f).margin(kTolerance));

  const Vector4 right = camera.getViewMatrix().transformPosition(Vector3(0.0f, 1.0f, 0.0f));
  REQUIRE(right.x == Approx(1.0f).margin(kTolerance));

  const Vector4 up = camera.getViewMatrix().transformPosition(Vector3(0.0f, 0.0f, 1.0f));
  REQUIRE(up.y == Approx(1.0f).margin(kTolerance));

  const Vector3 forward = camera.getForwardVector();
  REQUIRE(forward.x == Approx(1.0f).margin(kTolerance));
  REQUIRE(forward.y == Approx(0.0f).margin(kTolerance));
  REQUIRE(forward.z == Approx(0.0f).margin(kTolerance));
}

TEST_CASE("chCore - Camera projection", "[Camera]")
{
  const Camera camera = createTestCamera();
  const Matrix4 viewProjection = camera.getViewProjectionMatrix();

  // Depth goes from 0 at the near plane to 1 at the far plane.
  const Vector4 nearPoint = viewProjection.transformPosition(Vector3(-4.9f, 0.0f, 0.0f));
  REQUIRE(nearPoint.z / nearPoint.w == Approx(0.0f).margin(kTolerance));

  const Vector4 farPoint = viewProjection.transformPosition(Vector3(95.0f, 0.0f, 0.0f));
  REQUIRE(farPoint.z / farPoint.w == Approx(1.0f).margin(kTolerance));

  // The center of the view lands in the middle of the screen; screen Y grows downwards.
  Vector2 center;
  REQUIRE(camera.worldToScreenPoint(Vector3::ZERO, center));
  REQUIRE(center.x == Approx(0.5f).margin(kTolerance));
  REQUIRE(center.y == Approx(0.5f).margin(kTolerance));

  Vector2 right;
  REQUIRE(camera.worldToScreenPoint(Vector3(0.0f, 1.0f, 0.0f), right));
  REQUIRE(right.x > 0.5f);

  Vector2 up;
  REQUIRE(camera.worldToScreenPoint(Vector3(0.0f, 0.0f, 1.0f), up));
  REQUIRE(up.y < 0.5f);

  // A point behind the camera has no place on screen.
  Vector2 behind;
  REQUIRE_FALSE(camera.worldToScreenPoint(Vector3(-10.0f, 0.0f, 0.0f), behind));

  // The ray through the center of the screen starts on the near plane and looks forward.
  const Ray centerRay = camera.screenToWorldRay(Vector2(0.5f, 0.5f));
  REQUIRE(centerRay.origin.nearEqual(Vector3(-4.9f, 0.0f, 0.0f), 1e-3f));
  REQUIRE(centerRay.direction.nearEqual(Vector3::FORWARD, 1e-4f));
}

TEST_CASE("chCore - Camera frustum", "[Camera]")
{
  const Camera camera = createTestCamera();
  const Frustum& frustum = camera.getFrustum();

  REQUIRE(ShapeOverlap::frustumPoint(frustum, Vector3::ZERO));
  REQUIRE(ShapeOverlap::frustumPoint(frustum, Vector3(90.0f, 0.0f, 0.0f)));

  // Behind the camera, closer than the near plane and past the far plane.
  REQUIRE_FALSE(ShapeOverlap::frustumPoint(frustum, Vector3(-10.0f, 0.0f, 0.0f)));
  REQUIRE_FALSE(ShapeOverlap::frustumPoint(frustum, Vector3(-4.95f, 0.0f, 0.0f)));
  REQUIRE_FALSE(ShapeOverlap::frustumPoint(frustum, Vector3(100.0f, 0.0f, 0.0f)));

  // Far to a side, outside the 90 degree field of view.
  REQUIRE_FALSE(ShapeOverlap::frustumPoint(frustum, Vector3(0.0f, 50.0f, 0.0f)));
  REQUIRE_FALSE(ShapeOverlap::frustumPoint(frustum, Vector3(0.0f, 0.0f, 50.0f)));

  // A sphere outside the side plane but close enough to touch it is still visible.
  REQUIRE(ShapeOverlap::frustumSphere(frustum, Sphere(Vector3(0.0f, 12.0f, 0.0f), 5.0f)));
  REQUIRE_FALSE(
      ShapeOverlap::frustumSphere(frustum, Sphere(Vector3(0.0f, 50.0f, 0.0f), 1.0f)));

  const AABox visibleBox(Vector3(-1.0f, -1.0f, -1.0f), Vector3(1.0f, 1.0f, 1.0f));
  REQUIRE(ShapeOverlap::frustumBox(frustum, visibleBox));

  const AABox boxBehind(Vector3(-20.0f, -1.0f, -1.0f), Vector3(-15.0f, 1.0f, 1.0f));
  REQUIRE_FALSE(ShapeOverlap::frustumBox(frustum, boxBehind));
}

TEST_CASE("chCore - Camera updates", "[Camera]")
{
  Camera camera = createTestCamera();

  // Moving must also move the frustum; it used to keep the planes of the old position.
  REQUIRE_FALSE(ShapeOverlap::frustumPoint(camera.getFrustum(), Vector3(-10.0f, 0.0f, 0.0f)));
  camera.moveForward(-10.0f);
  REQUIRE(camera.getPosition().nearEqual(Vector3(-15.0f, 0.0f, 0.0f)));
  REQUIRE(camera.getLookAt().nearEqual(Vector3(-10.0f, 0.0f, 0.0f)));
  REQUIRE(ShapeOverlap::frustumPoint(camera.getFrustum(), Vector3(-10.0f, 0.0f, 0.0f)));

  camera.moveRight(2.0f);
  REQUIRE(camera.getPosition().nearEqual(Vector3(-15.0f, 2.0f, 0.0f)));
  camera.moveUp(3.0f);
  REQUIRE(camera.getPosition().nearEqual(Vector3(-15.0f, 2.0f, 3.0f)));
  REQUIRE(camera.getRightVector().nearEqual(Vector3::RIGHT, kTolerance));
  REQUIRE(camera.getUpVector().nearEqual(Vector3::UP, kTolerance));

  // Switching to orthographic takes effect without any other call.
  const Matrix4 perspective = camera.getProjectionMatrix();
  camera.setProjectionType(CameraProjectionType::Orthographic);
  REQUIRE_FALSE(camera.getProjectionMatrix().nearEqual(perspective, kTolerance));
  REQUIRE(camera.getProjectionMatrix()[3][3] == 1.0f);

  // Orbiting keeps the distance to the look at point and stops pitch at 89 degrees.
  Camera orbiting = createTestCamera();
  orbiting.rotate(0.0f, 90.0f);
  REQUIRE(orbiting.getPosition().nearEqual(Vector3(0.0f, -5.0f, 0.0f), 1e-4f));
  REQUIRE(orbiting.getForwardVector().nearEqual(Vector3::RIGHT, 1e-4f));
  orbiting.rotate(200.0f, 0.0f);
  REQUIRE(orbiting.getPosition().distance(Vector3::ZERO) == Approx(5.0f));
  REQUIRE(orbiting.getRotator().pitch.valueDegree() == Approx(89.0f).margin(1e-2f));

  // The rotation read back from the view matches where the camera looks.
  REQUIRE(camera.getRotation().rotateVector(Vector3::FORWARD)
              .nearEqual(camera.getForwardVector(), 1e-5f));
}

TEST_CASE("chCore - Camera component", "[Camera]")
{
  Scene scene("Test", chEngineSDK::UUID::createRandom());
  REQUIRE(scene.getActiveCamera() == nullptr);

  const SPtr<GameObject> first = scene.createGameObject("First Camera");
  CameraComponent& firstCamera = first->addComponent<CameraComponent>();
  REQUIRE(scene.getActiveCamera() == &firstCamera);

  SECTION("The view follows the object")
  {
    // Yaw 90 turns local forward (X) to world right (Y); the scale must not matter.
    first->getTransform().setLocalPosition(Vector3(1.0f, 2.0f, 3.0f));
    first->getTransform().setLocalRotation(Quaternion(Rotator(0.0f, 90.0f, 0.0f)));
    first->getTransform().setLocalScale(Vector3(2.0f, 2.0f, 2.0f));
    scene.updateTransforms();

    Camera camera;
    firstCamera.fillCamera(camera, 1600.0f, 900.0f);
    REQUIRE(camera.getPosition().nearEqual(Vector3(1.0f, 2.0f, 3.0f)));
    REQUIRE(camera.getForwardVector().nearEqual(Vector3::RIGHT, 1e-4f));
    REQUIRE(camera.getAspectRatio() == Approx(1600.0f / 900.0f));
    REQUIRE(camera.getFieldOfView().valueDegree() == Approx(90.0f));
    REQUIRE(camera.getNearClipPlane() == Approx(0.1f));
    REQUIRE(camera.getFarClipPlane() == Approx(10000.0f));
  }

  SECTION("Looking straight down keeps the yaw and stops at 89 degrees")
  {
    first->getTransform().setLocalRotation(Quaternion(Rotator(-90.0f, 90.0f, 0.0f)));
    scene.updateTransforms();

    Camera camera;
    firstCamera.fillCamera(camera, 800.0f, 600.0f);
    const Vector3 forward = camera.getForwardVector();
    REQUIRE(forward.z == Approx(-Math::sin(Radian(Degree(89.0f)))).margin(1e-4f));
    REQUIRE(forward.y > 0.0f);
    REQUIRE(forward.x == Approx(0.0f).margin(1e-4f));
  }

  SECTION("Values are kept in range")
  {
    firstCamera.setFieldOfView(Degree(500.0f));
    REQUIRE(firstCamera.getFieldOfView().valueDegree() == Approx(170.0f));
    firstCamera.setClipPlanes(0.0f, 0.0f);
    REQUIRE(firstCamera.getNearClipPlane() == Approx(0.001f));
    REQUIRE(firstCamera.getFarClipPlane() == Approx(0.002f));
    firstCamera.setOrthographicSize(-1.0f);
    REQUIRE(firstCamera.getOrthographicSize() > 0.0f);
  }

  SECTION("The last camera registered is used")
  {
    const SPtr<GameObject> second = scene.createGameObject("Second Camera");
    CameraComponent& secondCamera = second->addComponent<CameraComponent>();
    REQUIRE(scene.getActiveCamera() == &secondCamera);

    secondCamera.setEnabled(false);
    REQUIRE(scene.getActiveCamera() == &firstCamera);

    secondCamera.setEnabled(true);
    REQUIRE(scene.getActiveCamera() == &secondCamera);

    scene.destroyGameObject(*second);
    REQUIRE(scene.getActiveCamera() == &firstCamera);

    scene.destroyGameObject(*first);
    REQUIRE(scene.getActiveCamera() == nullptr);
  }

  SECTION("Priority and a camera picked by code")
  {
    const SPtr<GameObject> second = scene.createGameObject("Second Camera");
    CameraComponent& secondCamera = second->addComponent<CameraComponent>();
    firstCamera.setPriority(2);
    REQUIRE(scene.getActiveCamera() == &firstCamera);
    secondCamera.setPriority(2);
    REQUIRE(scene.getActiveCamera() == &secondCamera);
    secondCamera.setPriority(-1);
    REQUIRE(scene.getActiveCamera() == &firstCamera);

    REQUIRE(scene.setActiveCamera(&secondCamera));
    REQUIRE(scene.getActiveCamera() == &secondCamera);
    REQUIRE(scene.setActiveCamera(nullptr));
    REQUIRE(scene.getActiveCamera() == &firstCamera);

    // Unregistering the picked camera goes back to the priorities, and a camera that is
    // not registered cannot be picked.
    REQUIRE(scene.setActiveCamera(&secondCamera));
    secondCamera.setEnabled(false);
    REQUIRE(scene.getActiveCamera() == &firstCamera);
    secondCamera.setEnabled(true);
    REQUIRE(scene.getActiveCamera() == &firstCamera);
    CameraComponent outside;
    REQUIRE_FALSE(scene.setActiveCamera(&outside));
    REQUIRE(scene.getActiveCamera() == &firstCamera);
  }

  SECTION("Values are saved")
  {
    firstCamera.setPriority(-3);
    firstCamera.setProjectionType(CameraProjectionType::Orthographic);
    firstCamera.setFieldOfView(Degree(60.0f));
    firstCamera.setClipPlanes(0.5f, 250.0f);
    firstCamera.setOrthographicSize(12.0f);

    MemoryDataStream stream(256);
    firstCamera.serialize(stream);
    stream.seek(0);
    CameraComponent loaded;
    REQUIRE(loaded.deserialize(stream));
    REQUIRE(loaded.getProjectionType() == CameraProjectionType::Orthographic);
    REQUIRE(loaded.getFieldOfView().valueDegree() == Approx(60.0f));
    REQUIRE(loaded.getNearClipPlane() == Approx(0.5f));
    REQUIRE(loaded.getFarClipPlane() == Approx(250.0f));
    REQUIRE(loaded.getOrthographicSize() == Approx(12.0f));
    REQUIRE(loaded.getPriority() == -3);

    // A cut file and an unknown projection are refused.
    MemoryDataStream cut;
    const uint32 version = CameraComponent::SERIALIZATION_VERSION;
    cut.write(&version, sizeof(version));
    cut.seek(0);
    CameraComponent fromCut;
    REQUIRE_FALSE(fromCut.deserialize(cut));

    MemoryDataStream badProjection;
    const uint32 projection = 7;
    const float values[4] = {90.0f, 0.1f, 100.0f, 5.0f};
    const int32 priority = 0;
    badProjection.write(&version, sizeof(version));
    badProjection.write(&projection, sizeof(projection));
    badProjection.write(values, sizeof(values));
    badProjection.write(&priority, sizeof(priority));
    badProjection.seek(0);
    CameraComponent fromBad;
    REQUIRE_FALSE(fromBad.deserialize(badProjection));
  }

  SECTION("Made from its saved type id")
  {
    const UniquePtr<Component> created =
        ComponentRegister::create(TypeTraits<CameraComponent>::getTypeId());
    REQUIRE(created != nullptr);
    REQUIRE(StringView(ComponentRegister::getTypeName(
                TypeTraits<CameraComponent>::getTypeId())) == "CameraComponent");
  }
}
