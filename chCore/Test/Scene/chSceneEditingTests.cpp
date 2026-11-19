/************************************************************************/
/**
 * @file chSceneEditingTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of scene serialization, prefabs, undo, reparenting and model expansion.
 */
/************************************************************************/
#include "chAngle.h"
#include "chAssetMeta.h"
#include "chBox.h"
#include "chComponentRegister.h"
#include "chFileStream.h"
#include "chGameObjectSerializer.h"
#include "chGraphicsTypes.h"
#include "chLightComponent.h"
#include "chLinearColor.h"
#include "chMaterial.h"
#include "chMaterialAsset.h"
#include "chMatrixHelpers.h"
#include "chMesh.h"
#include "chModel.h"
#include "chModelAsset.h"
#include "chModelComponent.h"
#include "chModelExpand.h"
#include "chPrefabAsset.h"
#include "chQuaternion.h"
#include "chRay.h"
#include "chRotator.h"
#include "chScene.h"
#include "chSceneHistory.h"
#include "chStringUtils.h"
#include "chUUID.h"
#include "chVector3.h"

#include "catch.hpp"
#include "chModelTestHelpers.h"

using namespace chEngineSDK;

TEST_CASE("chCore - Model expand", "[Scene]")
{
  // Root (x + 10, one mesh) > Child (y + 5, two meshes) and Root > Empty > Leaf (z + 3).
  // Every mesh is the test triangle, centered at (0, 1, 0) in its node.
  const auto createMesh = [](uint32 slot) {
    SPtr<Mesh> mesh = createTriangleModel()->getMeshes()[0];
    mesh->setMaterialSlot(slot);
    return mesh;
  };
  const SPtr<Model> model = chMakeShared<Model>();
  const uint32 rootNode =
      addTestNode(*model, "Root", ModelNode::NO_PARENT,
                  TranslationMatrix(Vector3(10.0f, 0.0f, 0.0f)), {model->addMesh(createMesh(0))});
  const uint32 childMesh0 = model->addMesh(createMesh(0));
  const uint32 childMesh1 = model->addMesh(createMesh(1));
  addTestNode(*model, "Child", rootNode, TranslationMatrix(Vector3(0.0f, 5.0f, 0.0f)),
              {childMesh0, childMesh1});
  const uint32 emptyNode = addTestNode(*model, "Empty", rootNode, Matrix4::IDENTITY, {});
  addTestNode(*model, "Leaf", emptyNode, TranslationMatrix(Vector3(0.0f, 0.0f, 3.0f)),
              {model->addMesh(createMesh(1))});

  const SPtr<MaterialAsset> body = createTestMaterialAsset("M_Body");
  const SPtr<MaterialAsset> sword = createTestMaterialAsset("M_Sword");
  model->addMaterialSlot({.name = "Body", .materialId = body->getUUID(), .material = body});
  model->addMaterialSlot({.name = "Sword", .materialId = sword->getUUID(), .material = sword});

  Scene scene("Test", chEngineSDK::UUID::createRandom());
  const SPtr<GameObject> gameObject = scene.createGameObject("Model");
  gameObject->getTransform().setLocalPosition(Vector3(0.0f, 0.0f, 1.0f));
  ModelComponent& whole =
      gameObject->addComponent<ModelComponent>(createTestModelAsset(model));
  REQUIRE(whole.isWholeModel());
  const SPtr<MaterialAsset> gold = createTestMaterialAsset("M_Gold");
  whole.setMaterial(1, gold);
  scene.updateTransforms();
  REQUIRE(scene.getRenderItemCount() == 4);

  AABox wholeBounds(Vector3::ZERO, Vector3::ZERO);
  REQUIRE(gameObject->getHierarchyWorldBounds(wholeBounds));

  const auto findChild = [](const GameObject& parent, const String& name) -> GameObject* {
    for (const SPtr<GameObject>& child : parent.getChildren()) {
      if (child->getName() == name) {
        return child.get();
      }
    }
    return nullptr;
  };
  const auto partCenter = [](const GameObject& part) {
    AABox bounds(Vector3::ZERO, Vector3::ZERO);
    REQUIRE(part.getComponent<ModelComponent>()->getWorldBounds(bounds));
    return bounds.getCenter();
  };

  REQUIRE_FALSE(ModelExpand::expand(scene, *gameObject, ModelExpandMode::Single));

  SECTION("Per node keeps the hierarchy and where every mesh is drawn")
  {
    REQUIRE(ModelExpand::expand(scene, *gameObject, ModelExpandMode::PerNode));
    REQUIRE(gameObject->getComponent<ModelComponent>() == nullptr);
    REQUIRE(scene.getRenderItemCount() == 4);
    scene.updateTransforms();

    REQUIRE(gameObject->getChildren().size() == 1);
    GameObject* root = findChild(*gameObject, "Root");
    REQUIRE(root != nullptr);
    GameObject* child = findChild(*root, "Child");
    GameObject* empty = findChild(*root, "Empty");
    REQUIRE(child != nullptr);
    REQUIRE(empty != nullptr);
    REQUIRE(empty->getComponent<ModelComponent>() == nullptr);
    GameObject* leaf = findChild(*empty, "Leaf");
    REQUIRE(leaf != nullptr);

    REQUIRE(partCenter(*root).nearEqual(Vector3(10.0f, 1.0f, 1.0f)));
    REQUIRE(partCenter(*child).nearEqual(Vector3(10.0f, 6.0f, 1.0f)));
    REQUIRE(partCenter(*leaf).nearEqual(Vector3(10.0f, 1.0f, 4.0f)));

    const ModelComponent& childPart = *child->getComponent<ModelComponent>();
    REQUIRE(childPart.getUsedMaterialSlots().size() == 2);
    REQUIRE(childPart.getMaterial(0) == body);
    REQUIRE(childPart.getMaterial(1) == gold);
    REQUIRE(root->getComponent<ModelComponent>()->getUsedMaterialSlots().size() == 1);

    AABox expandedBounds(Vector3::ZERO, Vector3::ZERO);
    REQUIRE(gameObject->getHierarchyWorldBounds(expandedBounds));
    REQUIRE(expandedBounds.minPoint.nearEqual(wholeBounds.minPoint));
    REQUIRE(expandedBounds.maxPoint.nearEqual(wholeBounds.maxPoint));

    // Already split: nothing left to expand.
    REQUIRE_FALSE(ModelExpand::expand(scene, *gameObject, ModelExpandMode::PerMesh));
  }

  SECTION("Per mesh puts every mesh in its own child, standing at its center")
  {
    REQUIRE(ModelExpand::expand(scene, *gameObject, ModelExpandMode::PerMesh));
    scene.updateTransforms();
    REQUIRE(gameObject->getChildren().size() == 4);
    REQUIRE(scene.getRenderItemCount() == 4);

    GameObject* root = findChild(*gameObject, "Root");
    GameObject* childBody = findChild(*gameObject, "Child - Body");
    GameObject* childSword = findChild(*gameObject, "Child - Sword");
    GameObject* leaf = findChild(*gameObject, "Leaf");
    REQUIRE(root != nullptr);
    REQUIRE(childBody != nullptr);
    REQUIRE(childSword != nullptr);
    REQUIRE(leaf != nullptr);

    REQUIRE(root->getTransform().getWorldPosition().nearEqual(Vector3(10.0f, 1.0f, 1.0f)));
    REQUIRE(partCenter(*root).nearEqual(Vector3(10.0f, 1.0f, 1.0f)));
    REQUIRE(childSword->getTransform().getWorldPosition().nearEqual(
        Vector3(10.0f, 6.0f, 1.0f)));
    REQUIRE(partCenter(*childSword).nearEqual(Vector3(10.0f, 6.0f, 1.0f)));
    REQUIRE(partCenter(*leaf).nearEqual(Vector3(10.0f, 1.0f, 4.0f)));
    REQUIRE(childSword->getComponent<ModelComponent>()->getMaterial(1) == gold);
    REQUIRE(childBody->getComponent<ModelComponent>()->getUsedMaterialSlots().size() == 1);

    // A click picks the part, and moving a part moves only its mesh.
    SceneRayHit hit;
    REQUIRE(scene.raycast(Ray(Vector3(10.0f, 0.5f, -5.0f), Vector3::UP), hit));
    REQUIRE(hit.gameObject == root);
    leaf->getTransform().setLocalPosition(Vector3(0.0f, 0.0f, 20.0f));
    scene.updateTransforms();
    REQUIRE(partCenter(*leaf).nearEqual(Vector3(0.0f, 0.0f, 21.0f)));
    REQUIRE(partCenter(*root).nearEqual(Vector3(10.0f, 1.0f, 1.0f)));
  }

  SECTION("Hidden models stay hidden")
  {
    whole.setEnabled(false);
    REQUIRE(ModelExpand::expand(scene, *gameObject, ModelExpandMode::PerMesh));
    REQUIRE(scene.getRenderItemCount() == 0);
  }
}

namespace chEngineSDK {
/**
 * Exists so the tests have a component type that is not registered unless they register it.
 */
class TestNoteComponent : public Component
{
 public:
  void
  serialize(DataStream& stream) const override
  {
    stream.write(&note, sizeof(note));
  }

  NODISCARD bool
  deserialize(DataStream& stream) override
  {
    return stream.read(&note, sizeof(note)) == sizeof(note);
  }

  uint32 note = 0;
};
DECLARE_TYPE_TRAITS(TestNoteComponent)
} // namespace chEngineSDK

TEST_CASE("chCore - GameObject serializer", "[Scene]")
{
  const SPtr<Model> model = createTriangleModel();
  const SPtr<MaterialAsset> body = createTestMaterialAsset("M_Body");
  model->addMaterialSlot({.name = "Body", .materialId = body->getUUID(), .material = body});
  const SPtr<ModelAsset> modelAsset = createTestModelAsset(model);
  const SPtr<MaterialAsset> gold = createTestMaterialAsset("M_Gold");

  Scene source("Source", chEngineSDK::UUID::createRandom());
  const SPtr<GameObject> sun = source.createGameObject("Sun");
  sun->getTransform().setLocalRotation(Quaternion(Rotator(-50.0f, 30.0f, 0.0f)));
  DirectionalLightComponent& sunLight = sun->addComponent<DirectionalLightComponent>();
  sunLight.setColor(LinearColor(1.0f, 0.5f, 0.25f, 1.0f));
  sunLight.setIntensity(4.0f);

  const SPtr<GameObject> parent = source.createGameObject("Parent");
  parent->getTransform().setLocalPosition(Vector3(1.0f, 2.0f, 3.0f));
  parent->getTransform().setLocalScale(Vector3(2.0f, 2.0f, 2.0f));
  // Not registered yet: reading must skip it and still read what follows.
  parent->addComponent<TestNoteComponent>().note = 42;
  ModelComponent& part =
      parent->addComponent<ModelComponent>(modelAsset, 0, 0, Vector3(0.0f, 1.0f, 0.0f));
  part.setMaterial(0, gold);

  const SPtr<GameObject> lamp = source.createGameObject("Lamp", parent.get());
  PointLightComponent& lampLight = lamp->addComponent<PointLightComponent>();
  lampLight.setRange(25.0f);
  lampLight.setEnabled(false);
  const SPtr<GameObject> spot = source.createGameObject("Spot", parent.get());
  SpotLightComponent& spotLight = spot->addComponent<SpotLightComponent>();
  spotLight.setConeAngles(Degree(10.0f), Degree(20.0f));
  spotLight.setCastShadows(true);
  source.createGameObject("Empty", spot.get());
  // A second user of the same model, which must be referenced once.
  source.createGameObject("Copy")->addComponent<ModelComponent>(modelAsset);

  Vector<chEngineSDK::UUID> references;
  GameObjectSerializer::collectReferences(source.getRootGameObjects(), references);
  REQUIRE(references.size() == 2);
  REQUIRE(references[0] == modelAsset->getUUID());
  REQUIRE(references[1] == gold->getUUID());

  MemoryDataStream stream(64 * 1024);
  GameObjectSerializer::write(stream, source.getRootGameObjects());
  const SIZE_T written = stream.tell();

  Scene loaded("Loaded", chEngineSDK::UUID::createRandom());
  stream.seek(0);
  REQUIRE(GameObjectSerializer::read(stream, loaded, nullptr));
  REQUIRE(stream.tell() == written);

  const Vector<SPtr<GameObject>>& roots = loaded.getRootGameObjects();
  REQUIRE(roots.size() == 3);
  REQUIRE(roots[0]->getName() == "Sun");
  REQUIRE(roots[0]->getUUID() == sun->getUUID());
  REQUIRE(roots[0]->getTransform().getLocalRotation() ==
          sun->getTransform().getLocalRotation());
  const DirectionalLightComponent* loadedSun =
      roots[0]->getComponent<DirectionalLightComponent>();
  REQUIRE(loadedSun != nullptr);
  REQUIRE(loadedSun->isRegistered());
  REQUIRE(loadedSun->getColor().g == 0.5f);
  REQUIRE(loadedSun->getIntensity() == 4.0f);
  REQUIRE(loadedSun->getCastShadows());

  const GameObject& loadedParent = *roots[1];
  REQUIRE(loadedParent.getUUID() == parent->getUUID());
  REQUIRE(loadedParent.getTransform().getLocalPosition() == Vector3(1.0f, 2.0f, 3.0f));
  REQUIRE(loadedParent.getTransform().getLocalScale() == Vector3(2.0f, 2.0f, 2.0f));
  REQUIRE(loadedParent.getComponents().size() == 1);
  const ModelComponent* loadedPart = loadedParent.getComponent<ModelComponent>();
  REQUIRE(loadedPart != nullptr);
  REQUIRE(loadedPart->getNodeIndex() == 0);
  REQUIRE(loadedPart->getMeshIndex() == 0);
  REQUIRE(loadedPart->getPivot() == Vector3(0.0f, 1.0f, 0.0f));
  // There is no AssetManager here, so the model is not found, but its reference is kept for
  // the next save.
  REQUIRE(loadedPart->getModelAsset() == nullptr);
  Vector<chEngineSDK::UUID> loadedReferences;
  GameObjectSerializer::collectReferences(roots, loadedReferences);
  REQUIRE(loadedReferences.size() == 1);
  REQUIRE(loadedReferences[0] == modelAsset->getUUID());

  REQUIRE(loadedParent.getChildren().size() == 2);
  const GameObject& loadedLamp = *loadedParent.getChildren()[0];
  REQUIRE(loadedLamp.getName() == "Lamp");
  REQUIRE(loadedLamp.getParent() == &loadedParent);
  const PointLightComponent* loadedLampLight = loadedLamp.getComponent<PointLightComponent>();
  REQUIRE(loadedLampLight != nullptr);
  REQUIRE(loadedLampLight->getRange() == 25.0f);
  REQUIRE_FALSE(loadedLampLight->isEnabled());
  REQUIRE_FALSE(loadedLampLight->isRegistered());

  const GameObject& loadedSpot = *loadedParent.getChildren()[1];
  const SpotLightComponent* loadedSpotLight = loadedSpot.getComponent<SpotLightComponent>();
  REQUIRE(loadedSpotLight != nullptr);
  REQUIRE(loadedSpotLight->getInnerConeAngle().valueDegree() == Approx(10.0f));
  REQUIRE(loadedSpotLight->getOuterConeAngle().valueDegree() == Approx(20.0f));
  REQUIRE(loadedSpotLight->getCastShadows());
  REQUIRE(loadedSpot.getChildren().size() == 1);
  REQUIRE(loadedSpot.getChildren()[0]->getName() == "Empty");
  REQUIRE(loadedSpot.getChildren()[0]->getComponents().empty());
  REQUIRE(roots[2]->getName() == "Copy");
  // The sun and the spot; the lamp is disabled.
  REQUIRE(loaded.getLightCount() == 2);

  // Once its type is registered, the same bytes bring the component back.
  ComponentRegister::registerType<TestNoteComponent>();
  Scene again("Again", chEngineSDK::UUID::createRandom());
  stream.seek(0);
  REQUIRE(GameObjectSerializer::read(stream, again, nullptr));
  const TestNoteComponent* note =
      again.getRootGameObjects()[1]->getComponent<TestNoteComponent>();
  REQUIRE(note != nullptr);
  REQUIRE(note->note == 42);

  // A file cut short fails instead of reading past its end.
  MemoryDataStream cut(stream.getStartPtr(), written - 10, false);
  Scene broken("Broken", chEngineSDK::UUID::createRandom());
  REQUIRE_FALSE(GameObjectSerializer::read(cut, broken, nullptr));

  // So does a version from a newer engine.
  MemoryDataStream future(64);
  const uint32 futureVersion = GameObjectSerializer::SERIALIZATION_VERSION + 1;
  const uint32 noObjects = 0;
  future.write(&futureVersion, sizeof(futureVersion));
  future.write(&noObjects, sizeof(noObjects));
  future.seek(0);
  REQUIRE_FALSE(GameObjectSerializer::read(future, broken, nullptr));
}

TEST_CASE("chCore - Prefabs", "[Scene]")
{
  AssetMetadata metadata;
  metadata.uuid = chEngineSDK::UUID::createRandom();
  metadata.assetType = AssetTypeTraits<PrefabAsset>::getTypeId();
  StringUtils::copyToBuffer(metadata.name, "PF_LampPost");
  PrefabAsset prefab(metadata);
  REQUIRE(prefab.getContent().empty());

  // Lamp Post (point light, moved away) > Bulb (spot light)
  Scene source("Source", chEngineSDK::UUID::createRandom());
  const SPtr<GameObject> post = source.createGameObject("Lamp Post");
  post->getTransform().setLocalPosition(Vector3(50.0f, 0.0f, 0.0f));
  post->getTransform().setLocalScale(Vector3(5.0f, 5.0f, 5.0f));
  post->getTransform().setLocalRotation(Quaternion(Rotator(0.0f, 90.0f, 0.0f)));
  post->addComponent<PointLightComponent>().setRange(12.0f);
  const SPtr<GameObject> bulb = source.createGameObject("Bulb", post.get());
  bulb->getTransform().setLocalPosition(Vector3(0.0f, 0.0f, 3.0f));
  bulb->addComponent<SpotLightComponent>();
  REQUIRE(prefab.setContent(*post));
  REQUIRE_FALSE(prefab.getContent().empty());

  Scene scene("Scene", chEngineSDK::UUID::createRandom());
  const SPtr<GameObject> first = GameObjectSerializer::instantiate(prefab, scene, nullptr);
  const SPtr<GameObject> second = GameObjectSerializer::instantiate(prefab, scene, nullptr);
  second->getTransform().setLocalPosition(Vector3(0.0f, 10.0f, 0.0f));
  REQUIRE(first->getName() == "PF_LampPost");
  REQUIRE(first->getPrefab() == prefab.getUUID());
  REQUIRE(first->getOutermostPrefabInstance() == first.get());
  // An instance stands where it is placed, not where the prefab's root was.
  REQUIRE(first->getTransform().getLocalPosition() == Vector3::ZERO);
  // It starts with the rotation and scale of the prefab's root.
  REQUIRE(first->getTransform().getLocalScale() == Vector3(5.0f, 5.0f, 5.0f));
  REQUIRE(first->getTransform().getLocalRotation() ==
          post->getTransform().getLocalRotation());
  const PointLightComponent* firstLight = first->getComponent<PointLightComponent>();
  REQUIRE(firstLight != nullptr);
  REQUIRE(firstLight->getRange() == 12.0f);
  REQUIRE(first->getChildren().size() == 1);
  const SPtr<GameObject>& firstBulb = first->getChildren()[0];
  REQUIRE(firstBulb->getName() == "Bulb");
  REQUIRE(firstBulb->getTransform().getLocalPosition() == Vector3(0.0f, 0.0f, 3.0f));
  REQUIRE(firstBulb->getOutermostPrefabInstance() == first.get());
  REQUIRE(firstBulb->getPrefab().isNull());
  // Every instance gets its own UUIDs.
  REQUIRE(firstBulb->getUUID() != bulb->getUUID());
  REQUIRE(firstBulb->getUUID() != second->getChildren()[0]->getUUID());
  REQUIRE(scene.getLightCount() == 4);

  SECTION("A scene stores an instance as its link alone")
  {
    Vector<chEngineSDK::UUID> references;
    GameObjectSerializer::collectReferences(scene.getRootGameObjects(), references);
    REQUIRE(references.size() == 1);
    REQUIRE(references[0] == prefab.getUUID());

    MemoryDataStream stream;
    GameObjectSerializer::write(stream, scene.getRootGameObjects());
    stream.seek(0);
    Scene loaded("Loaded", chEngineSDK::UUID::createRandom());
    REQUIRE(GameObjectSerializer::read(stream, loaded, nullptr));
    REQUIRE(loaded.getRootGameObjects().size() == 2);
    // Without an AssetManager the prefab is not found: the instance is empty but keeps the
    // link and its own transform.
    const GameObject& loadedSecond = *loaded.getRootGameObjects()[1];
    REQUIRE(loadedSecond.getPrefab() == prefab.getUUID());
    REQUIRE(loadedSecond.getUUID() == second->getUUID());
    REQUIRE(loadedSecond.getTransform().getLocalPosition() == Vector3(0.0f, 10.0f, 0.0f));
    REQUIRE(loadedSecond.getComponents().empty());
    REQUIRE(loadedSecond.getChildren().empty());
  }

  SECTION("The content can be read into a plain object, to edit the prefab")
  {
    const SPtr<GameObject> editable = scene.createGameObject("Editable");
    REQUIRE(GameObjectSerializer::readPrefabContent(prefab, *editable));
    REQUIRE(editable->getTransform().getLocalScale() == Vector3(5.0f, 5.0f, 5.0f));
    REQUIRE(editable->getTransform().getLocalPosition() == Vector3::ZERO);
    REQUIRE(editable->getPrefab().isNull());
    REQUIRE(editable->getOutermostPrefabInstance() == nullptr);
    REQUIRE(editable->getComponent<PointLightComponent>() != nullptr);
    REQUIRE(editable->getChildren().size() == 1);
  }

  SECTION("A prefab cannot hold an instance of itself")
  {
    const SPtr<GameObject> holder = scene.createGameObject("Holder");
    GameObjectSerializer::instantiate(prefab, scene, holder.get());
    const SIZE_T sizeBefore = prefab.getContent().size();
    REQUIRE_FALSE(prefab.setContent(*holder));
    REQUIRE_FALSE(prefab.setContent(*first));
    REQUIRE(prefab.getContent().size() == sizeBefore);
    REQUIRE(prefab.dependsOn(prefab.getUUID()));
  }

  SECTION("Unpacking keeps the content as plain objects")
  {
    first->setPrefab(chEngineSDK::UUID::null());
    REQUIRE(firstBulb->getOutermostPrefabInstance() == nullptr);
    MemoryDataStream stream;
    GameObjectSerializer::write(stream, scene.getRootGameObjects());
    stream.seek(0);
    Scene loaded("Loaded", chEngineSDK::UUID::createRandom());
    REQUIRE(GameObjectSerializer::read(stream, loaded, nullptr));
    REQUIRE(loaded.getRootGameObjects()[0]->getChildren().size() == 1);
    REQUIRE(loaded.getRootGameObjects()[0]->getComponent<PointLightComponent>() != nullptr);
  }
}

TEST_CASE("chCore - Scene history", "[Scene]")
{
  Scene scene("Test", chEngineSDK::UUID::createRandom());
  const SPtr<GameObject> a = scene.createGameObject("A");
  a->addComponent<PointLightComponent>().setRange(10.0f);
  const SPtr<GameObject> c = scene.createGameObject("C", a.get());
  const SPtr<GameObject> b = scene.createGameObject("B");
  b->addComponent<DirectionalLightComponent>();
  const SPtr<GameObject> d = scene.createGameObject("D");
  const chEngineSDK::UUID bId = b->getUUID();
  const chEngineSDK::UUID cId = c->getUUID();
  REQUIRE(scene.getLightCount() == 2);

  SceneHistory history;
  chEngineSDK::UUID focus = chEngineSDK::UUID::null();
  REQUIRE_FALSE(history.canUndo());
  REQUIRE_FALSE(history.undo(scene, focus));

  SECTION("A drag is one step, undone in place")
  {
    history.recordChange(*a, "Move");
    a->getTransform().setLocalPosition(Vector3(1.0f, 0.0f, 0.0f));
    history.recordChange(*a, "Move");
    a->getTransform().setLocalPosition(Vector3(2.0f, 0.0f, 0.0f));
    REQUIRE(history.isEntryOpen());
    REQUIRE(history.getUndoName() == "Move A");
    history.commit(scene);
    REQUIRE(history.getEntryCount() == 1);

    REQUIRE(history.undo(scene, focus));
    REQUIRE(focus == a->getUUID());
    // The same object, so whatever holds it keeps a live one.
    REQUIRE(scene.getRootGameObjects()[0] == a);
    REQUIRE(a->getTransform().getLocalPosition() == Vector3::ZERO);
    REQUIRE(a->getComponent<PointLightComponent>()->getRange() == 10.0f);
    REQUIRE(a->getChildren().size() == 1);
    REQUIRE(a->getChildren()[0]->getUUID() == cId);
    REQUIRE(scene.getLightCount() == 2);
    REQUIRE(history.canRedo());
    REQUIRE(history.getRedoName() == "Move A");

    REQUIRE(history.redo(scene, focus));
    REQUIRE(a->getTransform().getLocalPosition() == Vector3(2.0f, 0.0f, 0.0f));
    REQUIRE_FALSE(history.canRedo());
  }

  SECTION("A deleted object comes back with its UUID at its place")
  {
    history.recordChange(*b, "Delete");
    scene.destroyGameObject(*b);
    history.commit(scene);
    REQUIRE(scene.getRootGameObjects().size() == 2);
    REQUIRE(scene.getLightCount() == 1);

    REQUIRE(history.undo(scene, focus));
    REQUIRE(focus == bId);
    REQUIRE(scene.getRootGameObjects().size() == 3);
    REQUIRE(scene.getRootGameObjects()[1]->getUUID() == bId);
    REQUIRE(scene.getRootGameObjects()[2] == d);
    REQUIRE(scene.getRootGameObjects()[1]->getComponent<DirectionalLightComponent>());
    REQUIRE(scene.getLightCount() == 2);
    REQUIRE(scene.findGameObject(bId) == scene.getRootGameObjects()[1]);

    REQUIRE(history.redo(scene, focus));
    REQUIRE(scene.findGameObject(bId) == nullptr);
    REQUIRE(scene.getLightCount() == 1);
  }

  SECTION("A created object goes away and comes back")
  {
    const SPtr<GameObject> e = scene.createGameObject("E", a.get());
    e->addComponent<SpotLightComponent>();
    history.recordCreated(*e, "Create");
    // Recorded already through its creation, so this adds nothing.
    history.recordChange(*e, "Move");
    e->getTransform().setLocalPosition(Vector3(0.0f, 0.0f, 5.0f));
    history.commit(scene);
    const chEngineSDK::UUID eId = e->getUUID();

    REQUIRE(history.undo(scene, focus));
    REQUIRE(a->getChildren().size() == 1);
    REQUIRE(scene.findGameObject(eId) == nullptr);
    REQUIRE(scene.getLightCount() == 2);

    REQUIRE(history.redo(scene, focus));
    const SPtr<GameObject> again = scene.findGameObject(eId);
    REQUIRE(again != nullptr);
    REQUIRE(again->getParent() == a.get());
    REQUIRE(again->getTransform().getLocalPosition() == Vector3(0.0f, 0.0f, 5.0f));
    REQUIRE(again->getComponent<SpotLightComponent>());
    REQUIRE(scene.getLightCount() == 3);
  }

  SECTION("A parent recorded after its child undoes both")
  {
    history.recordChange(*c, "Rename");
    c->setName("Renamed");
    history.recordChange(*a, "Delete Child");
    scene.destroyGameObject(*c);
    history.commit(scene);
    REQUIRE(a->getChildren().empty());

    REQUIRE(history.undo(scene, focus));
    REQUIRE(focus == cId);
    REQUIRE(a->getChildren().size() == 1);
    REQUIRE(a->getChildren()[0]->getUUID() == cId);
    REQUIRE(a->getChildren()[0]->getName() == "C");

    REQUIRE(history.redo(scene, focus));
    REQUIRE(a->getChildren().empty());
  }

  SECTION("Nothing changed, nothing to undo")
  {
    history.recordChange(*d, "Move");
    history.commit(scene);
    REQUIRE(history.getEntryCount() == 0);
    REQUIRE_FALSE(history.canUndo());
  }

  SECTION("Saved state")
  {
    REQUIRE(history.isAtSavedState());
    history.recordChange(*d, "Rename");
    d->setName("D2");
    REQUIRE_FALSE(history.isAtSavedState());
    history.commit(scene);
    history.markSaved();
    REQUIRE(history.isAtSavedState());

    REQUIRE(history.undo(scene, focus));
    REQUIRE(d->getName() == "D");
    REQUIRE_FALSE(history.isAtSavedState());
    REQUIRE(history.redo(scene, focus));
    REQUIRE(history.isAtSavedState());

    // A new step after an undo makes the saved state unreachable.
    REQUIRE(history.undo(scene, focus));
    history.recordChange(*d, "Rename");
    d->setName("D3");
    history.commit(scene);
    REQUIRE_FALSE(history.canRedo());
    REQUIRE_FALSE(history.isAtSavedState());
  }

  SECTION("The oldest steps are dropped past the limit")
  {
    SceneHistory shortHistory(2);
    for (uint32 i = 1; i <= 3; ++i) {
      shortHistory.recordChange(*d, "Move");
      d->getTransform().setLocalPosition(Vector3(static_cast<float>(i), 0.0f, 0.0f));
      shortHistory.commit(scene);
    }
    REQUIRE(shortHistory.getEntryCount() == 2);
    REQUIRE(shortHistory.undo(scene, focus));
    REQUIRE(shortHistory.undo(scene, focus));
    REQUIRE_FALSE(shortHistory.undo(scene, focus));
    REQUIRE(d->getTransform().getLocalPosition() == Vector3(1.0f, 0.0f, 0.0f));
  }
}

TEST_CASE("chCore - Scene reparent and duplicate", "[Scene]")
{
  Scene scene("Test", chEngineSDK::UUID::createRandom());
  const SPtr<GameObject> parent = scene.createGameObject("Parent");
  parent->getTransform().setLocalPosition(Vector3(10.0f, 0.0f, 0.0f));
  parent->getTransform().setLocalRotation(Quaternion(Rotator(0.0f, 90.0f, 0.0f)));
  parent->getTransform().setLocalScale(Vector3(2.0f, 2.0f, 2.0f));
  const SPtr<GameObject> lamp = scene.createGameObject("Lamp");
  lamp->getTransform().setLocalPosition(Vector3(0.0f, 4.0f, 1.0f));
  lamp->addComponent<PointLightComponent>().setRange(7.0f);
  const SPtr<GameObject> bulb = scene.createGameObject("Bulb", lamp.get());
  const SPtr<GameObject> last = scene.createGameObject("Last");
  scene.updateTransforms();

  SECTION("Reparenting keeps the world placement")
  {
    REQUIRE(scene.setParent(*lamp, parent.get()));
    REQUIRE(lamp->getParent() == parent.get());
    REQUIRE(parent->getChildren().back() == lamp);
    REQUIRE(scene.getRootGameObjects().size() == 2);
    REQUIRE(lamp->getTransform().getLocalScale().nearEqual(Vector3(0.5f, 0.5f, 0.5f), 1e-5f));
    scene.updateTransforms();
    REQUIRE(lamp->getTransform().getWorldPosition().nearEqual(Vector3(0.0f, 4.0f, 1.0f),
                                                              1e-4f));
    REQUIRE(bulb->getParent() == lamp.get());

    // Back to the root, last among the roots.
    REQUIRE(scene.setParent(*lamp, nullptr));
    REQUIRE(scene.getRootGameObjects().back() == lamp);
    scene.updateTransforms();
    REQUIRE(lamp->getTransform().getWorldPosition().nearEqual(Vector3(0.0f, 4.0f, 1.0f),
                                                              1e-4f));
  }

  SECTION("An object cannot go under itself or its children")
  {
    REQUIRE_FALSE(scene.setParent(*lamp, lamp.get()));
    REQUIRE_FALSE(scene.setParent(*lamp, bulb.get()));
    Scene other("Other", chEngineSDK::UUID::createRandom());
    const SPtr<GameObject> stranger = other.createGameObject("Stranger");
    REQUIRE_FALSE(scene.setParent(*lamp, stranger.get()));
    REQUIRE(lamp->getParent() == nullptr);
    REQUIRE(scene.getSiblingIndex(*lamp) == 1);
  }

  SECTION("Undo puts a reparented object back in its place")
  {
    SceneHistory history;
    history.recordChange(*lamp, "Reparent");
    REQUIRE(scene.setParent(*lamp, parent.get()));
    history.commit(scene);
    chEngineSDK::UUID focus = chEngineSDK::UUID::null();
    REQUIRE(history.undo(scene, focus));
    const SPtr<GameObject> restored = scene.findGameObject(focus);
    REQUIRE(restored != nullptr);
    REQUIRE(restored->getParent() == nullptr);
    REQUIRE(scene.getSiblingIndex(*restored) == 1);
    REQUIRE(restored->getTransform().getLocalPosition() == Vector3(0.0f, 4.0f, 1.0f));
    REQUIRE(parent->getChildren().empty());
    REQUIRE(history.redo(scene, focus));
    REQUIRE(scene.findGameObject(focus)->getParent() == parent.get());
  }

  SECTION("A duplicate goes right after its source, with new UUIDs")
  {
    const SPtr<GameObject> copy = GameObjectSerializer::duplicate(*lamp);
    REQUIRE(copy != nullptr);
    REQUIRE(scene.getRootGameObjects().size() == 4);
    REQUIRE(scene.getRootGameObjects()[2] == copy);
    REQUIRE(scene.getRootGameObjects()[3] == last);
    REQUIRE(copy->getName() == "Lamp");
    REQUIRE(copy->getUUID() != lamp->getUUID());
    REQUIRE(copy->getTransform().getLocalPosition() == Vector3(0.0f, 4.0f, 1.0f));
    const PointLightComponent* light = copy->getComponent<PointLightComponent>();
    REQUIRE(light != nullptr);
    REQUIRE(light->getRange() == 7.0f);
    REQUIRE(scene.getLightCount() == 2);
    REQUIRE(copy->getChildren().size() == 1);
    REQUIRE(copy->getChildren()[0]->getUUID() != bulb->getUUID());

    // A child is copied under the same parent.
    const SPtr<GameObject> bulbCopy = GameObjectSerializer::duplicate(*bulb);
    REQUIRE(bulbCopy != nullptr);
    REQUIRE(bulbCopy->getParent() == lamp.get());
    REQUIRE(lamp->getChildren().size() == 2);
  }

  SECTION("Copies read back get new UUIDs, as a paste")
  {
    MemoryDataStream stream;
    const SPtr<GameObject> copied[] = {lamp, last};
    GameObjectSerializer::write(stream, Span<const SPtr<GameObject>>(copied));
    stream.seek(0);
    REQUIRE(GameObjectSerializer::readCopies(stream, scene, parent.get()));
    REQUIRE(parent->getChildren().size() == 2);
    REQUIRE(parent->getChildren()[0]->getName() == "Lamp");
    REQUIRE(parent->getChildren()[0]->getUUID() != lamp->getUUID());
    REQUIRE(parent->getChildren()[0]->getChildren().size() == 1);
    REQUIRE(parent->getChildren()[1]->getName() == "Last");
    REQUIRE(scene.getLightCount() == 2);
  }

  SECTION("Undo puts reordered siblings back in their order")
  {
    SceneHistory history;
    history.recordChange(*last, "Reparent");
    scene.setSiblingIndex(*last, 0);
    history.recordChange(*lamp, "Reparent");
    scene.setSiblingIndex(*lamp, 0);
    history.setEntryName("Reparent 2 objects");
    REQUIRE(history.getUndoName() == "Reparent 2 objects");
    history.commit(scene);
    REQUIRE(scene.getRootGameObjects()[0] == lamp);
    REQUIRE(scene.getRootGameObjects()[1] == last);

    chEngineSDK::UUID focus = chEngineSDK::UUID::null();
    REQUIRE(history.undo(scene, focus));
    REQUIRE(scene.getRootGameObjects()[0] == parent);
    REQUIRE(scene.getRootGameObjects()[1] == lamp);
    REQUIRE(scene.getRootGameObjects()[2] == last);
    REQUIRE(history.redo(scene, focus));
    REQUIRE(scene.getRootGameObjects()[0] == lamp);
    REQUIRE(scene.getRootGameObjects()[1] == last);
  }
}
