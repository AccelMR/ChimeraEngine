/************************************************************************/
/**
 * @file chSceneTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of the scene: transforms, render items, lights, picking, play and updates.
 */
/************************************************************************/
#include "chAngle.h"
#include "chAnimatorComponent.h"
#include "chBox.h"
#include "chCamera.h"
#include "chComponentRegister.h"
#include "chFileStream.h"
#include "chGameObjectSerializer.h"
#include "chGraphicsTypes.h"
#include "chLightComponent.h"
#include "chLinearColor.h"
#include "chMaterial.h"
#include "chMath.h"
#include "chMesh.h"
#include "chModel.h"
#include "chModelAsset.h"
#include "chModelComponent.h"
#include "chQuaternion.h"
#include "chRay.h"
#include "chRotator.h"
#include "chScene.h"
#include "chSceneManager.h"
#include "chShapeComponent.h"
#include "chSkyLightComponent.h"
#include "chSphere.h"
#include "chUUID.h"
#include "chVector3.h"
#include "chVector4.h"

#include "catch.hpp"
#include "chCoreTestHelpers.h"
#include "chSceneTestHelpers.h"
#include "chModelTestHelpers.h"

using namespace chEngineSDK;

TEST_CASE("chCore - Scene transforms", "[Scene]")
{
  Scene scene("Test", chEngineSDK::UUID::createRandom());
  const SPtr<GameObject> parent = scene.createGameObject("Parent");
  const SPtr<GameObject> child = scene.createGameObject("Child", parent.get());
  REQUIRE(scene.getRootGameObjects().size() == 1);
  REQUIRE(child->getParent() == parent.get());
  REQUIRE(child->getScene() == &scene);

  parent->getTransform().setLocalPosition(Vector3(10.0f, 0.0f, 0.0f));
  parent->getTransform().setLocalRotation(Quaternion(Rotator(0.0f, 90.0f, 0.0f)));
  child->getTransform().setLocalPosition(Vector3(1.0f, 0.0f, 0.0f));
  REQUIRE(child->getTransform().isDirty());

  scene.updateTransforms();
  REQUIRE_FALSE(child->getTransform().isDirty());
  // The parent's yaw turns the child's forward offset to the right.
  REQUIRE(child->getTransform().getWorldPosition().nearEqual(Vector3(10.0f, 1.0f, 0.0f),
                                                             1e-5f));

  // Moving only the parent still moves the child.
  parent->getTransform().setLocalPosition(Vector3(0.0f, 0.0f, 5.0f));
  scene.updateTransforms();
  REQUIRE(child->getTransform().getWorldPosition().nearEqual(Vector3(0.0f, 1.0f, 5.0f),
                                                             1e-5f));

  scene.destroyGameObject(*parent);
  REQUIRE(scene.getRootGameObjects().empty());
  REQUIRE(parent->getScene() == nullptr);
  REQUIRE(child->getScene() == nullptr);
}

TEST_CASE("chCore - Scene toString", "[Scene]")
{
  Scene scene("Test", chEngineSDK::UUID::createRandom());
  const SPtr<GameObject> gameObject = scene.createGameObject("Player");
  REQUIRE(gameObject->toString() ==
          "GameObject(Player, " + gameObject->getUUID().toString() + ")");

  Transform& transform = gameObject->getTransform();
  transform.setLocalPosition(Vector3(1.0f, 2.0f, 3.0f));
  transform.setLocalRotation(Quaternion(Rotator(0.0f, 90.0f, 0.0f)));
  transform.setLocalScale(Vector3(2.0f, 2.0f, 2.0f));
  REQUIRE(transform.toString() ==
          "Transform(position (1.000, 2.000, 3.000), "
          "rotation Rotator(pitch 0.00, yaw 90.00, roll 0.00), scale (2.000, 2.000, 2.000))");
}

TEST_CASE("chCore - Scene render items", "[Scene]")
{
  Scene scene("Test", chEngineSDK::UUID::createRandom());
  const SPtr<GameObject> gameObject = scene.createGameObject("Model");
  const SPtr<Model> model = createTriangleModel();
  ModelComponent& modelComponent =
      gameObject->addComponent<ModelComponent>(createTestModelAsset(model));
  REQUIRE(gameObject->getComponent<ModelComponent>() == &modelComponent);
  REQUIRE(modelComponent.isRegistered());
  REQUIRE(scene.getRenderItemCount() == 1);

  const Camera camera = createTestCamera();
  Vector<const RenderItem*> visible;
  scene.gatherRenderItems(camera.getFrustum(), visible);
  REQUIRE(visible.size() == 1);
  REQUIRE(visible[0]->mesh == model->getMeshes()[0].get());

  SECTION("Items follow the object and are culled behind the camera")
  {
    gameObject->getTransform().setLocalPosition(Vector3(-20.0f, 0.0f, 0.0f));
    scene.updateTransforms();
    REQUIRE(visible[0]->worldBounds.center.nearEqual(Vector3(-20.0f, 1.0f, 0.0f)));
    scene.gatherRenderItems(camera.getFrustum(), visible);
    REQUIRE(visible.empty());
    AABox bounds(Vector3::ZERO, Vector3::ZERO);
    REQUIRE(modelComponent.getWorldBounds(bounds));
    REQUIRE(bounds.getCenter().nearEqual(Vector3(-20.0f, 1.0f, 0.0f)));
  }

  SECTION("Disabling unregisters")
  {
    modelComponent.setEnabled(false);
    REQUIRE_FALSE(modelComponent.isRegistered());
    REQUIRE(scene.getRenderItemCount() == 0);
    modelComponent.setEnabled(true);
    REQUIRE(scene.getRenderItemCount() == 1);
  }

  SECTION("Destroying the object removes its items")
  {
    scene.destroyGameObject(*gameObject);
    REQUIRE(scene.getRenderItemCount() == 0);
    REQUIRE_FALSE(modelComponent.isRegistered());

    // A freed slot is reused.
    const SPtr<GameObject> other = scene.createGameObject("Other");
    other->addComponent<ModelComponent>(createTestModelAsset(createTriangleModel()));
    REQUIRE(scene.getRenderItemCount() == 1);
  }
}

TEST_CASE("chCore - Scene raycast", "[Scene]")
{
  Scene scene("Test", chEngineSDK::UUID::createRandom());
  // The triangle lies flat at its object's Z, covering x -1..1 and y 0..2 (times scale).
  const SPtr<GameObject> back = scene.createGameObject("Back");
  back->addComponent<ModelComponent>(createTestModelAsset(createTriangleModel()));
  const SPtr<GameObject> front = scene.createGameObject("Front");
  front->addComponent<ModelComponent>(createTestModelAsset(createTriangleModel()));
  front->getTransform().setLocalPosition(Vector3(3.0f, 0.0f, -2.0f));
  front->getTransform().setLocalScale(Vector3(2.0f, 3.0f, 1.0f));
  scene.updateTransforms();

  SceneRayHit hit;
  REQUIRE(scene.raycast(Ray(Vector3(3.0f, 0.5f, -5.0f), Vector3::UP), hit));
  REQUIRE(hit.gameObject == front.get());
  REQUIRE(hit.distance == Approx(3.0f));
  REQUIRE(hit.point.nearEqual(Vector3(3.0f, 0.5f, -2.0f), 0.001f));
  // Facing the ray, whichever side was hit.
  REQUIRE(hit.normal.nearEqual(-Vector3::UP, 0.001f));
  REQUIRE(scene.raycast(Ray(Vector3(3.0f, 0.5f, 5.0f), -Vector3::UP), hit));
  REQUIRE(hit.normal.nearEqual(Vector3::UP, 0.001f));

  SECTION("Max distance and ignored objects")
  {
    const Ray ray(Vector3(3.0f, 0.5f, -5.0f), Vector3::UP);
    REQUIRE_FALSE(scene.raycast(ray, hit, 2.5f));
    REQUIRE(scene.raycast(ray, hit, 3.5f));
    REQUIRE(hit.gameObject == front.get());

    // Ignoring an object skips its children too.
    const SPtr<GameObject> holder = scene.createGameObject("Holder");
    REQUIRE(scene.setParent(*front, holder.get()));
    scene.updateTransforms();
    REQUIRE_FALSE(scene.raycast(ray, hit, Math::MAX_FLOAT, holder.get()));
    REQUIRE(scene.raycast(Ray(Vector3(0.0f, 0.5f, -5.0f), Vector3::UP), hit, Math::MAX_FLOAT,
                          holder.get()));
    REQUIRE(hit.gameObject == back.get());
  }

  REQUIRE(scene.raycast(Ray(Vector3(0.0f, 0.5f, -5.0f), Vector3::UP), hit));
  REQUIRE(hit.gameObject == back.get());
  REQUIRE(hit.distance == Approx(5.0f));

  // Inside the back triangle's box but outside the triangle, and behind the ray.
  REQUIRE_FALSE(scene.raycast(Ray(Vector3(0.95f, 1.95f, -5.0f), Vector3::UP), hit));
  REQUIRE_FALSE(scene.raycast(Ray(Vector3(3.0f, 0.5f, -5.0f), -Vector3::UP), hit));

  // In front of the back triangle, the front one is picked.
  back->getTransform().setLocalPosition(Vector3(3.0f, 0.0f, 0.0f));
  scene.updateTransforms();
  REQUIRE(scene.raycast(Ray(Vector3(3.0f, 0.5f, -5.0f), Vector3::UP), hit));
  REQUIRE(hit.gameObject == front.get());
}

TEST_CASE("chCore - Scene lights", "[Scene]")
{
  Scene scene("Test", chEngineSDK::UUID::createRandom());
  const Camera camera = createTestCamera();
  Vector<const LightItem*> visible;

  // Positive pitch turns forward up, so -90 makes the sun shine straight down.
  const SPtr<GameObject> sun = scene.createGameObject("Sun");
  sun->getTransform().setLocalRotation(Quaternion(Rotator(-90.0f, 0.0f, 0.0f)));
  DirectionalLightComponent& sunLight = sun->addComponent<DirectionalLightComponent>();
  sunLight.setColor(LinearColor(1.0f, 0.5f, 0.25f, 1.0f));
  sunLight.setIntensity(2.0f);
  scene.updateTransforms();
  REQUIRE(scene.getLightCount() == 1);

  scene.gatherLights(camera.getFrustum(), visible);
  REQUIRE(visible.size() == 1);
  REQUIRE(visible[0]->type == LightType::Directional);
  REQUIRE(visible[0]->owner == sun.get());
  REQUIRE(visible[0]->direction.nearEqual(Vector3(0.0f, 0.0f, -1.0f)));
  REQUIRE(visible[0]->color.r == 2.0f);
  REQUIRE(visible[0]->color.g == 1.0f);
  REQUIRE(visible[0]->color.b == 0.5f);
  // Only directional lights cast shadows by default.
  REQUIRE(visible[0]->bCastShadows);

  const SPtr<GameObject> lamp = scene.createGameObject("Lamp");
  PointLightComponent& lampLight = lamp->addComponent<PointLightComponent>();
  lampLight.setRange(5.0f);
  scene.updateTransforms();
  REQUIRE(scene.getLightCount() == 2);
  REQUIRE_FALSE(lampLight.getCastShadows());

  SECTION("Point lights are culled by their range")
  {
    lamp->getTransform().setLocalPosition(Vector3(-20.0f, 0.0f, 0.0f));
    scene.updateTransforms();
    scene.gatherLights(camera.getFrustum(), visible);
    REQUIRE(visible.size() == 1);

    // Behind the camera, but its range reaches what the camera sees.
    lampLight.setRange(30.0f);
    scene.gatherLights(camera.getFrustum(), visible);
    REQUIRE(visible.size() == 2);
    REQUIRE(visible[1]->position.nearEqual(Vector3(-20.0f, 0.0f, 0.0f)));
    REQUIRE(visible[1]->range == 30.0f);
  }

  SECTION("Spot cone angles are kept in order")
  {
    const SPtr<GameObject> spot = scene.createGameObject("Spot");
    SpotLightComponent& spotLight = spot->addComponent<SpotLightComponent>();
    spotLight.setConeAngles(Degree(60.0f), Degree(30.0f));
    REQUIRE(spotLight.getOuterConeAngle().valueDegree() == 30.0f);
    REQUIRE(spotLight.getInnerConeAngle().valueDegree() == 30.0f);
    spotLight.setConeAngles(Degree(20.0f), Degree(120.0f));
    REQUIRE(spotLight.getOuterConeAngle().valueDegree() == 89.0f);

    scene.updateTransforms();
    scene.gatherLights(camera.getFrustum(), visible);
    REQUIRE(visible.size() == 3);
    REQUIRE(visible[2]->type == LightType::Spot);
    REQUIRE(Math::abs(visible[2]->innerConeCos - Math::cos(Degree(20.0f))) < 1e-5f);
    REQUIRE(Math::abs(visible[2]->outerConeCos - Math::cos(Degree(89.0f))) < 1e-5f);
  }

  SECTION("Disabling or destroying removes the light")
  {
    lampLight.setEnabled(false);
    REQUIRE(scene.getLightCount() == 1);
    lampLight.setEnabled(true);
    REQUIRE(scene.getLightCount() == 2);
    scene.destroyGameObject(*lamp);
    REQUIRE(scene.getLightCount() == 1);
  }
}


TEST_CASE("chCore - Scene play", "[Scene]")
{
  SECTION("A scene that does not play only updates what updates while editing")
  {
    Scene scene("Edit", chEngineSDK::UUID::createRandom());
    REQUIRE_FALSE(scene.isPlaying());
    const SPtr<GameObject> parent = scene.createGameObject("Parent");
    const SPtr<GameObject> child = scene.createGameObject("Child", parent.get());
    TestCounterComponent& game = child->addComponent<TestCounterComponent>();
    TestCounterComponent& preview = parent->addComponent<TestCounterComponent>(true);

    scene.fixedUpdate(1.0f);
    scene.update(1.0f);
    REQUIRE(game.updates == 0);
    REQUIRE(game.fixedUpdates == 0);
    REQUIRE(preview.updates == 1);
    REQUIRE(preview.fixedUpdates == 1);

    scene.setPlaying(true);
    scene.fixedUpdate(1.0f);
    scene.update(1.0f);
    REQUIRE(game.updates == 1);
    REQUIRE(game.fixedUpdates == 1);
    REQUIRE(preview.updates == 2);

    // Disabled components never update, playing or not.
    game.setEnabled(false);
    scene.update(1.0f);
    REQUIRE(game.updates == 1);
  }

  SECTION("Animators animate while editing")
  {
    const AnimatorComponent animator;
    REQUIRE(animator.updatesWhileEditing());
    const TestCounterComponent counter;
    REQUIRE_FALSE(counter.updatesWhileEditing());
  }

  SECTION("The copy has the same objects and changing it leaves the source alone")
  {
    Scene source("Source", chEngineSDK::UUID::createRandom());
    const SPtr<GameObject> sun = source.createGameObject("Sun");
    sun->addComponent<DirectionalLightComponent>().setIntensity(4.0f);
    const SPtr<GameObject> parent = source.createGameObject("Parent");
    parent->getTransform().setLocalPosition(Vector3(1.0f, 2.0f, 3.0f));
    const SPtr<GameObject> child = source.createGameObject("Child", parent.get());
    child->addComponent<PointLightComponent>().setRange(7.0f);

    Scene copy("Source", chEngineSDK::UUID::createRandom());
    REQUIRE(GameObjectSerializer::copyScene(source, copy));
    REQUIRE(copy.getRootGameObjects().size() == 2);
    REQUIRE(copy.getLightCount() == 2);

    const SPtr<GameObject> copiedParent = copy.findGameObject(parent->getUUID());
    REQUIRE(copiedParent);
    REQUIRE(copiedParent != parent);
    REQUIRE(copiedParent->getName() == "Parent");
    REQUIRE(copiedParent->getTransform().getLocalPosition().nearEqual(
        Vector3(1.0f, 2.0f, 3.0f)));
    const SPtr<GameObject> copiedChild = copy.findGameObject(child->getUUID());
    REQUIRE(copiedChild);
    REQUIRE(copiedChild->getParent() == copiedParent.get());
    const PointLightComponent* light = copiedChild->getComponent<PointLightComponent>();
    REQUIRE(light);
    REQUIRE(light->getRange() == 7.0f);
    REQUIRE(copy.findGameObject(sun->getUUID())
                ->getComponent<DirectionalLightComponent>()
                ->getIntensity() == 4.0f);

    copiedParent->getTransform().setLocalPosition(Vector3(9.0f, 9.0f, 9.0f));
    copy.destroyGameObject(*copy.findGameObject(sun->getUUID()));
    REQUIRE(copy.getLightCount() == 1);
    REQUIRE(source.getLightCount() == 2);
    REQUIRE(source.getRootGameObjects().size() == 2);
    REQUIRE(parent->getTransform().getLocalPosition().nearEqual(Vector3(1.0f, 2.0f, 3.0f)));
  }

  SECTION("The scene manager keeps a copy until it is unloaded")
  {
    SceneManager::startUp();
    SceneManager& sceneManager = SceneManager::instance();
    const SPtr<Scene> copy = chMakeShared<Scene>("Copy", chEngineSDK::UUID::createRandom());
    REQUIRE(sceneManager.addScene(copy));
    REQUIRE_FALSE(sceneManager.addScene(copy));
    REQUIRE(sceneManager.setActiveScene(copy));
    REQUIRE(sceneManager.getActiveScene().lock() == copy);
    sceneManager.unloadScene(*copy);
    REQUIRE(sceneManager.getActiveScene().expired());
    SceneManager::shutDown();
  }
}

namespace chEngineSDK {
/**
 * Exists so the tests can see which updates and transform rebuilds a scene runs. Without
 * update flags its update is never called.
 */
class TestHookComponent : public Component
{
 public:
  explicit TestHookComponent(bool bUpdates = true)
  {
    if (bUpdates) {
      setUpdateFlags(ComponentUpdate::Frame);
    }
  }

  void
  update(float deltaTime) override
  {
    CH_PARAMETER_UNUSED(deltaTime);
    ++updates;
    if (onUpdate) {
      onUpdate();
    }
  }

  uint32 updates = 0;
  uint32 transformChanges = 0;
  Function<void()> onUpdate;

 protected:
  void
  onTransformChanged() override
  {
    ++transformChanges;
  }
};
DECLARE_TYPE_TRAITS(TestHookComponent)
} // namespace chEngineSDK

TEST_CASE("chCore - Scene update lists", "[Scene]")
{
  Scene scene("Lists", chEngineSDK::UUID::createRandom());
  scene.setPlaying(true);

  SECTION("Only components with update flags update")
  {
    const SPtr<GameObject> gameObject = scene.createGameObject("A");
    const TestHookComponent& updating = gameObject->addComponent<TestHookComponent>();
    const TestHookComponent& still = gameObject->addComponent<TestHookComponent>(false);
    REQUIRE(updating.getUpdateFlags().isSet(ComponentUpdate::Frame));
    REQUIRE_FALSE(still.getUpdateFlags());
    scene.update(0.1f);
    scene.fixedUpdate(0.1f);
    REQUIRE(updating.updates == 1);
    REQUIRE(still.updates == 0);
  }

  SECTION("Components that register or leave during an update")
  {
    const SPtr<GameObject> first = scene.createGameObject("First");
    const SPtr<GameObject> second = scene.createGameObject("Second");
    TestHookComponent& firstHook = first->addComponent<TestHookComponent>();
    TestHookComponent& secondHook = second->addComponent<TestHookComponent>();
    TestHookComponent* added = nullptr;
    firstHook.onUpdate = [&]() {
      secondHook.setEnabled(false);
      added = &scene.createGameObject("Added")->addComponent<TestHookComponent>();
    };
    scene.update(0.1f);
    // Disabled before its turn, it is skipped; the new one waits for the next update.
    REQUIRE(firstHook.updates == 1);
    REQUIRE(secondHook.updates == 0);
    REQUIRE(added->updates == 0);

    firstHook.onUpdate = nullptr;
    secondHook.setEnabled(true);
    scene.update(0.1f);
    REQUIRE(added->updates == 1);
    REQUIRE(secondHook.updates == 1);

    // Removed during the update: the components after it still run, in the same order.
    firstHook.onUpdate = [&]() { second->removeComponent(secondHook); };
    scene.update(0.1f);
    REQUIRE(second->getComponents().empty());
    REQUIRE(added->updates == 2);
    firstHook.onUpdate = nullptr;
    scene.update(0.1f);
    REQUIRE(firstHook.updates == 4);
    REQUIRE(added->updates == 3);
  }

  SECTION("Only moved objects and what is under them are rebuilt")
  {
    const SPtr<GameObject> parent = scene.createGameObject("Parent");
    const SPtr<GameObject> child = scene.createGameObject("Child", parent.get());
    const SPtr<GameObject> other = scene.createGameObject("Other");
    const TestHookComponent& parentHook = parent->addComponent<TestHookComponent>(false);
    const TestHookComponent& childHook = child->addComponent<TestHookComponent>(false);
    const TestHookComponent& otherHook = other->addComponent<TestHookComponent>(false);

    // New objects start dirty.
    scene.updateTransforms();
    REQUIRE(parentHook.transformChanges == 1);
    REQUIRE(childHook.transformChanges == 1);
    REQUIRE(otherHook.transformChanges == 1);
    scene.updateTransforms();
    REQUIRE(parentHook.transformChanges == 1);

    child->getTransform().setLocalPosition(Vector3(0.0f, 1.0f, 0.0f));
    child->getTransform().setLocalScale(Vector3(2.0f, 2.0f, 2.0f));
    scene.updateTransforms();
    REQUIRE(parentHook.transformChanges == 1);
    REQUIRE(childHook.transformChanges == 2);
    REQUIRE(otherHook.transformChanges == 1);

    // The child is listed before its parent, and is still rebuilt once, after it.
    child->getTransform().setLocalPosition(Vector3(0.0f, 2.0f, 0.0f));
    parent->getTransform().setLocalPosition(Vector3(10.0f, 0.0f, 0.0f));
    scene.updateTransforms();
    REQUIRE(parentHook.transformChanges == 2);
    REQUIRE(childHook.transformChanges == 3);
    REQUIRE(otherHook.transformChanges == 1);
    REQUIRE(child->getTransform().getWorldPosition().nearEqual(Vector3(10.0f, 2.0f, 0.0f)));

    // A parent that moves alone rebuilds its children too.
    parent->getTransform().setLocalPosition(Vector3(20.0f, 0.0f, 0.0f));
    scene.updateTransforms();
    REQUIRE(childHook.transformChanges == 4);
    REQUIRE(child->getTransform().getWorldPosition().nearEqual(Vector3(20.0f, 2.0f, 0.0f)));

    // Moved, then out of the scene before the rebuild, kept alive here or freed.
    other->getTransform().setLocalPosition(Vector3(1.0f, 0.0f, 0.0f));
    scene.destroyGameObject(*other);
    {
      const SPtr<GameObject> freed = scene.createGameObject("Freed");
      scene.destroyGameObject(*freed);
    }
    scene.updateTransforms();
    REQUIRE(otherHook.transformChanges == 1);
    // Out of a scene, a change tells nobody.
    other->getTransform().setLocalPosition(Vector3(2.0f, 0.0f, 0.0f));
    scene.updateTransforms();
    REQUIRE(otherHook.transformChanges == 1);
  }

  SECTION("A destroyed batch keeps the order of what stays")
  {
    Vector<SPtr<GameObject>> roots;
    for (const ANSICHAR* name : {"R0", "R1", "R2", "R3", "R4"}) {
      roots.push_back(scene.createGameObject(name));
    }
    const SPtr<GameObject> parent = scene.createGameObject("Parent");
    Vector<SPtr<GameObject>> kids;
    for (const ANSICHAR* name : {"K0", "K1", "K2", "K3"}) {
      kids.push_back(scene.createGameObject(name, parent.get()));
    }
    const auto namesOf = [](const Vector<SPtr<GameObject>>& objects) {
      String names;
      for (const SPtr<GameObject>& gameObject : objects) {
        names += gameObject->getName();
        names += ' ';
      }
      return names;
    };

    scene.requestDestroy(*roots[3]);
    scene.requestDestroy(*kids[1]);
    scene.requestDestroy(*roots[1]);
    scene.requestDestroy(*kids[2]);
    scene.requestDestroy(*roots[1]);
    scene.update(0.1f);
    REQUIRE(namesOf(scene.getRootGameObjects()) == "R0 R2 R4 Parent ");
    REQUIRE(namesOf(parent->getChildren()) == "K0 K3 ");
    REQUIRE(kids[1]->getParent() == nullptr);
    REQUIRE(kids[1]->getScene() == nullptr);
    REQUIRE(roots[1]->getScene() == nullptr);

    // A child asked for before its parent: the parent leaves with the children it keeps.
    scene.requestDestroy(*kids[0]);
    scene.requestDestroy(*parent);
    scene.update(0.1f);
    REQUIRE(namesOf(scene.getRootGameObjects()) == "R0 R2 R4 ");
    REQUIRE(namesOf(parent->getChildren()) == "K3 ");
    REQUIRE(kids[3]->getScene() == nullptr);
    REQUIRE(kids[0]->getParent() == nullptr);
  }
}

TEST_CASE("chCore - Sky lights", "[Scene]")
{
  Scene scene("Test", chEngineSDK::UUID::createRandom());
  REQUIRE(scene.getActiveSkyLight() == nullptr);

  const SPtr<GameObject> first = scene.createGameObject("First Sky");
  SkyLightComponent& firstSky = first->addComponent<SkyLightComponent>();
  scene.updateTransforms();
  const SkyLightItem* active = scene.getActiveSkyLight();
  REQUIRE(active != nullptr);
  REQUIRE(active->owner == first.get());
  REQUIRE(active->texture == nullptr);
  REQUIRE(active->intensity == 1.0f);
  REQUIRE(active->bShowSky);

  SECTION("The last sky light registered is used")
  {
    const SPtr<GameObject> second = scene.createGameObject("Second Sky");
    second->addComponent<SkyLightComponent>();
    REQUIRE(scene.getActiveSkyLight()->owner == second.get());

    scene.destroyGameObject(*second);
    REQUIRE(scene.getActiveSkyLight()->owner == first.get());

    firstSky.setEnabled(false);
    REQUIRE(scene.getActiveSkyLight() == nullptr);
  }

  SECTION("Turning the object turns the image")
  {
    // Yaw 90 turns local forward (X) to world right (Y), so looking right sees the part of
    // the image that was in front.
    first->getTransform().setLocalRotation(Quaternion(Rotator(0.0f, 90.0f, 0.0f)));
    first->getTransform().setLocalScale(Vector3(3.0f, 3.0f, 3.0f));
    scene.updateTransforms();
    const Matrix4& worldToSky = scene.getActiveSkyLight()->worldToSky;
    REQUIRE(worldToSky.transformVector(Vector3::RIGHT).nearEqual(Vector4(1.0f, 0.0f, 0.0f, 0.0f)));
    REQUIRE(worldToSky.transformVector(Vector3::UP).nearEqual(Vector4(0.0f, 0.0f, 1.0f, 0.0f)));
  }

  SECTION("Values are saved, and a missing image keeps its id")
  {
    firstSky.setIntensity(2.5f);
    firstSky.setShowSky(false);
    REQUIRE(scene.getActiveSkyLight()->intensity == 2.5f);
    REQUIRE_FALSE(scene.getActiveSkyLight()->bShowSky);

    MemoryDataStream stream(256);
    firstSky.serialize(stream);
    stream.seek(0);
    SkyLightComponent loaded;
    REQUIRE(loaded.deserialize(stream));
    REQUIRE(loaded.getIntensity() == 2.5f);
    REQUIRE_FALSE(loaded.getShowSky());
    REQUIRE(loaded.getTextureId().isNull());

    // Version 1 data that names an image no asset manager knows.
    const chEngineSDK::UUID imageId = chEngineSDK::UUID::createRandom();
    MemoryDataStream withImage(256);
    const uint32 version = 1;
    const float intensity = 1.0f;
    const uint32 showSky = 1;
    withImage.write(&version, sizeof(version));
    withImage.write(&imageId, sizeof(imageId));
    withImage.write(&intensity, sizeof(intensity));
    withImage.write(&showSky, sizeof(showSky));
    withImage.seek(0);
    SkyLightComponent missing;
    REQUIRE(missing.deserialize(withImage));
    REQUIRE(missing.getTexture() == nullptr);
    REQUIRE(missing.getTextureId() == imageId);
    Vector<chEngineSDK::UUID> references;
    missing.collectReferences(references);
    REQUIRE(references.size() == 1);
    REQUIRE(references[0] == imageId);
  }
}

TEST_CASE("chCore - Shape component", "[Scene]")
{
  Scene scene("Test", chEngineSDK::UUID::createRandom());
  const SPtr<GameObject> gameObject = scene.createGameObject("Sphere");
  ShapeComponent& shape =
      gameObject->addComponent<ShapeComponent>(ShapeDesc{.type = ShapeType::Sphere});
  gameObject->getTransform().setLocalPosition(Vector3(10.0f, 0.0f, 0.0f));
  scene.updateTransforms();
  REQUIRE(scene.getRenderItemCount() == 1);

  AABox bounds(Vector3::ZERO, Vector3::ZERO);
  REQUIRE(shape.getWorldBounds(bounds));
  REQUIRE(bounds.getCenter().nearEqual(Vector3(10.0f, 0.0f, 0.0f), 1e-4f));

  SECTION("A ray picks it")
  {
    SceneRayHit hit;
    REQUIRE(scene.raycast(Ray(Vector3::ZERO, Vector3::FORWARD), hit));
    REQUIRE(hit.gameObject == gameObject.get());
    REQUIRE(Math::abs(hit.distance - 9.5f) < 0.01f);
  }

  SECTION("Equal shapes share one mesh")
  {
    const SPtr<GameObject> other = scene.createGameObject("Other Sphere");
    ShapeComponent& otherShape =
        other->addComponent<ShapeComponent>(ShapeDesc{.type = ShapeType::Sphere});
    REQUIRE(otherShape.getMesh() == shape.getMesh());

    otherShape.setShape({.type = ShapeType::Sphere, .radius = 2.0f});
    REQUIRE(otherShape.getMesh() != shape.getMesh());
    scene.updateTransforms();
    REQUIRE(other->getHierarchyWorldBounds(bounds));
    REQUIRE(bounds.maxPoint.nearEqual(Vector3(2.0f, 2.0f, 2.0f), 1e-3f));
  }

  SECTION("Changing the shape changes the drawn mesh")
  {
    const Mesh* sphereMesh = shape.getMesh().get();
    shape.setShape({.type = ShapeType::Box, .size = Vector3(4.0f, 4.0f, 4.0f)});
    REQUIRE(shape.getMesh().get() != sphereMesh);
    REQUIRE(shape.getWorldBounds(bounds));
    REQUIRE(bounds.maxPoint.nearEqual(Vector3(12.0f, 2.0f, 2.0f), 1e-4f));
  }

  SECTION("Parents frame their shapes")
  {
    const SPtr<GameObject> child = scene.createGameObject("Child", gameObject.get());
    child->addComponent<ShapeComponent>(ShapeDesc{.type = ShapeType::Box});
    child->getTransform().setLocalPosition(Vector3(0.0f, 5.0f, 0.0f));
    scene.updateTransforms();
    REQUIRE(gameObject->getHierarchyWorldBounds(bounds));
    REQUIRE(bounds.minPoint.nearEqual(Vector3(9.5f, -0.5f, -0.5f), 1e-4f));
    REQUIRE(bounds.maxPoint.nearEqual(Vector3(10.5f, 5.5f, 0.5f), 1e-4f));
  }

  SECTION("Disabling removes the render item")
  {
    shape.setEnabled(false);
    REQUIRE(scene.getRenderItemCount() == 0);
    REQUIRE_FALSE(shape.getWorldBounds(bounds));
  }

  SECTION("Saved with its sizes, and a missing material keeps its id")
  {
    shape.setShape(
        {.type = ShapeType::Cone, .radius = 2.0f, .height = 3.0f, .segments = 12});
    MemoryDataStream stream(256);
    shape.serialize(stream);
    stream.seek(0);
    ShapeComponent loaded;
    REQUIRE(loaded.deserialize(stream));
    REQUIRE(loaded.getShape() == shape.getShape());
    REQUIRE(loaded.getMaterialId().isNull());

    const chEngineSDK::UUID materialId = chEngineSDK::UUID::createRandom();
    const uint32 version = 1;
    const float values[5] = {1.0f, 1.0f, 1.0f, 0.25f, 2.0f};
    const uint32 counts[3] = {8, 4, 1};
    const auto writeShape = [&](MemoryDataStream& out, uint32 type) {
      out.write(&version, sizeof(version));
      out.write(&materialId, sizeof(materialId));
      out.write(&type, sizeof(type));
      out.write(values, sizeof(values));
      out.write(counts, sizeof(counts));
      out.seek(0);
    };
    MemoryDataStream withMaterial(256);
    writeShape(withMaterial, static_cast<uint32>(ShapeType::Cylinder));
    ShapeComponent missing;
    REQUIRE(missing.deserialize(withMaterial));
    REQUIRE(missing.getShape().type == ShapeType::Cylinder);
    REQUIRE(missing.getShape().radius == 0.25f);
    REQUIRE(missing.getShape().segments == 8);
    REQUIRE(missing.getMaterial() == nullptr);
    REQUIRE(missing.getMaterialId() == materialId);
    Vector<chEngineSDK::UUID> references;
    missing.collectReferences(references);
    REQUIRE(references.size() == 1);
    REQUIRE(references[0] == materialId);

    // A type the engine does not know is refused.
    MemoryDataStream badType(256);
    writeShape(badType, static_cast<uint32>(ShapeType::COUNT));
    ShapeComponent refused;
    REQUIRE_FALSE(refused.deserialize(badType));
  }

  SECTION("Made from its saved type id")
  {
    const UniquePtr<Component> created =
        ComponentRegister::create(TypeTraits<ShapeComponent>::getTypeId());
    REQUIRE(created != nullptr);
    REQUIRE(StringView(ComponentRegister::getTypeName(
                TypeTraits<ShapeComponent>::getTypeId())) == "ShapeComponent");
  }
}
