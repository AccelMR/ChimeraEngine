/************************************************************************/
/**
 * @file chModelTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of materials, models, their serialization and skeletons.
 */
/************************************************************************/
#include <cstring>

#include "chAngle.h"
#include "chCamera.h"
#include "chFileStream.h"
#include "chGraphicsTypes.h"
#include "chLinearColor.h"
#include "chMaterial.h"
#include "chMaterialAsset.h"
#include "chMatrixHelpers.h"
#include "chMesh.h"
#include "chModel.h"
#include "chModelAsset.h"
#include "chModelComponent.h"
#include "chModelSerializationHeaders.h"
#include "chModelSerializer.h"
#include "chQuaternion.h"
#include "chScene.h"
#include "chSkeleton.h"
#include "chUUID.h"
#include "chVector2.h"
#include "chVector3.h"
#include "chVector4.h"
#include "chVertexLayout.h"

#include "catch.hpp"
#include "chCoreTestHelpers.h"
#include "chModelTestHelpers.h"

using namespace chEngineSDK;

TEST_CASE("chCore - Material", "[Resources]")
{
  Material material;
  REQUIRE(material.getBaseColorFactor().r == 1.0f);
  REQUIRE(material.getMetallicFactor() == 0.0f);
  REQUIRE(material.getRoughnessFactor() == 0.5f);
  REQUIRE(material.getEmissiveFactor().r == 0.0f);
  REQUIRE(material.getAlphaMode() == MaterialAlphaMode::Opaque);
  REQUIRE(material.getAlphaCutoff() == 0.5f);
  REQUIRE_FALSE(material.isTwoSided());
  for (uint32 i = 0; i < static_cast<uint32>(MaterialTexture::COUNT); ++i) {
    REQUIRE(material.getTextureId(static_cast<MaterialTexture>(i)).isNull());
    REQUIRE(material.getGpuTexture(static_cast<MaterialTexture>(i)) == nullptr);
  }

  Vector<chEngineSDK::UUID> references;
  material.collectReferences(references);
  REQUIRE(references.empty());

  // A texture that is missing keeps its id, so saving again does not lose it.
  const chEngineSDK::UUID baseColorId = chEngineSDK::UUID::createRandom();
  const chEngineSDK::UUID normalId = chEngineSDK::UUID::createRandom();
  material.setBaseColorFactor(LinearColor(0.25f, 0.5f, 0.75f, 1.0f));
  material.setMetallicFactor(0.75f);
  material.setRoughnessFactor(0.125f);
  material.setNormalScale(2.0f);
  material.setOcclusionStrength(0.5f);
  material.setEmissiveFactor(LinearColor(4.0f, 2.0f, 1.0f, 1.0f));
  material.setAlphaMode(MaterialAlphaMode::Mask);
  material.setAlphaCutoff(0.25f);
  material.setTwoSided(true);
  material.setTexture(MaterialTexture::BaseColor, baseColorId, nullptr);
  material.setTexture(MaterialTexture::Normal, normalId, nullptr);
  material.collectReferences(references);
  REQUIRE(references.size() == 2);
  REQUIRE(references[0] == baseColorId);
  REQUIRE(references[1] == normalId);

  MemoryDataStream stream(256);
  material.serialize(stream);
  const SIZE_T writtenSize = stream.tell();
  stream.seek(0);

  Material loaded;
  REQUIRE(loaded.deserialize(stream));
  REQUIRE(loaded.getBaseColorFactor().g == 0.5f);
  REQUIRE(loaded.getBaseColorFactor().b == 0.75f);
  REQUIRE(loaded.getMetallicFactor() == 0.75f);
  REQUIRE(loaded.getRoughnessFactor() == 0.125f);
  REQUIRE(loaded.getNormalScale() == 2.0f);
  REQUIRE(loaded.getOcclusionStrength() == 0.5f);
  REQUIRE(loaded.getEmissiveFactor().r == 4.0f);
  REQUIRE(loaded.getEmissiveFactor().b == 1.0f);
  REQUIRE(loaded.getTextureId(MaterialTexture::BaseColor) == baseColorId);
  REQUIRE(loaded.getTextureId(MaterialTexture::Normal) == normalId);
  REQUIRE(loaded.getTextureId(MaterialTexture::MetallicRoughness).isNull());
  REQUIRE(loaded.getTexture(MaterialTexture::BaseColor) == nullptr);
  REQUIRE(loaded.getAlphaMode() == MaterialAlphaMode::Mask);
  REQUIRE(loaded.getAlphaCutoff() == 0.25f);
  REQUIRE(loaded.isTwoSided());

  SECTION("A newer version is rejected")
  {
    stream.seek(0);
    const uint32 otherVersion = Material::SERIALIZATION_VERSION + 1;
    stream.write(&otherVersion, sizeof(otherVersion));
    stream.seek(0);
    REQUIRE_FALSE(loaded.deserialize(stream));
  }

  SECTION("An unknown alpha mode is rejected")
  {
    const uint32 badMode = static_cast<uint32>(MaterialAlphaMode::COUNT);
    // The mode follows the cutoff at the end of the data.
    stream.seek(writtenSize - sizeof(uint32) * 2);
    stream.write(&badMode, sizeof(badMode));
    stream.seek(0);
    REQUIRE_FALSE(loaded.deserialize(stream));
  }

  SECTION("Clearing a texture clears only its id")
  {
    material.setTexture(MaterialTexture::BaseColor, nullptr);
    REQUIRE(material.getTextureId(MaterialTexture::BaseColor).isNull());
    REQUIRE(material.getTextureId(MaterialTexture::Normal) == normalId);
  }
}

TEST_CASE("chCore - Model material slots", "[Resources]")
{
  const SPtr<Model> model = createTriangleModel();
  const SPtr<Mesh> mesh = model->getMeshes()[0];
  REQUIRE(mesh->getMaterialSlot() == 0);

  // A second mesh on another node uses slot 1.
  SPtr<Mesh> secondMesh = chMakeShared<Mesh>();
  secondMesh->setVertexData(Vector<VertexStatic>{
      {Vector3(1.0f, 0.0f, 0.0f), Vector3::UP, Vector2(0.0f, 0.0f),
       Vector4(1.0f, 0.0f, 0.0f, 1.0f)},
      {Vector3(-1.0f, 0.0f, 0.0f), Vector3::UP, Vector2(1.0f, 0.0f),
       Vector4(1.0f, 0.0f, 0.0f, 1.0f)},
      {Vector3(0.0f, 1.0f, 0.0f), Vector3::UP, Vector2(0.0f, 1.0f),
       Vector4(1.0f, 0.0f, 0.0f, 1.0f)}});
  secondMesh->setIndexData(Vector<uint16>{0, 1, 2});
  secondMesh->setMaterialSlot(1);
  addTestNode(*model, "Sword", 0, Matrix4::IDENTITY, {model->addMesh(secondMesh)});

  const SPtr<MaterialAsset> body = createTestMaterialAsset("M_Body");
  const SPtr<MaterialAsset> sword = createTestMaterialAsset("M_Sword");
  REQUIRE(model->addMaterialSlot({.name = "Body", .materialId = body->getUUID(),
                                  .material = body}) == 0);
  REQUIRE(model->addMaterialSlot({.name = "Sword", .materialId = sword->getUUID(),
                                  .material = sword}) == 1);

  Scene scene("Test", chEngineSDK::UUID::createRandom());
  const SPtr<GameObject> gameObject = scene.createGameObject("Skeleton");
  ModelComponent& modelComponent =
      gameObject->addComponent<ModelComponent>(createTestModelAsset(model));
  REQUIRE(modelComponent.getUsedMaterialSlots().size() == 2);
  REQUIRE(scene.getRenderItemCount() == 2);

  const auto materialOf = [&scene](const Mesh* itemMesh) -> const Material* {
    const Camera camera = createTestCamera();
    Vector<const RenderItem*> visible;
    scene.gatherRenderItems(camera.getFrustum(), visible);
    for (const RenderItem* item : visible) {
      if (item->mesh == itemMesh) {
        return item->material;
      }
    }
    return nullptr;
  };
  REQUIRE(materialOf(mesh.get()) == &body->getMaterial());
  REQUIRE(materialOf(secondMesh.get()) == &sword->getMaterial());

  SECTION("An override changes only its slot and this object")
  {
    const SPtr<MaterialAsset> gold = createTestMaterialAsset("M_Gold");
    modelComponent.setMaterial(1, gold);
    REQUIRE(modelComponent.getMaterialOverride(1) == gold);
    REQUIRE(modelComponent.getMaterial(1) == gold);
    REQUIRE(modelComponent.getMaterial(0) == body);
    REQUIRE(materialOf(secondMesh.get()) == &gold->getMaterial());
    REQUIRE(materialOf(mesh.get()) == &body->getMaterial());

    modelComponent.setMaterial(1, nullptr);
    REQUIRE(materialOf(secondMesh.get()) == &sword->getMaterial());
  }

  SECTION("Overrides set while unregistered apply on register")
  {
    const SPtr<MaterialAsset> gold = createTestMaterialAsset("M_Gold");
    modelComponent.setEnabled(false);
    modelComponent.setMaterial(0, gold);
    modelComponent.setEnabled(true);
    REQUIRE(materialOf(mesh.get()) == &gold->getMaterial());
  }

  SECTION("Slots past the end have no material")
  {
    REQUIRE(modelComponent.getMaterial(2) == nullptr);
    modelComponent.setMaterial(5, body);
    secondMesh->setMaterialSlot(7);
    modelComponent.setModel(modelComponent.getModelAsset());
    REQUIRE(materialOf(secondMesh.get()) == nullptr);
  }

  SECTION("Editing a material shows in the items that use it")
  {
    body->getMaterial().setBaseColorFactor(LinearColor(1.0f, 0.0f, 0.0f, 1.0f));
    REQUIRE(materialOf(mesh.get())->getBaseColorFactor().g == 0.0f);
  }
}

TEST_CASE("chCore - Model serializer", "[Resources]")
{
  // Root (moved and turned) > Child with both meshes; Shared is a second root that draws
  // mesh 0 again.
  const SPtr<Model> model = chMakeShared<Model>();
  const SPtr<Mesh> triangle = createTriangleModel()->getMeshes()[0];
  SPtr<Mesh> second = chMakeShared<Mesh>();
  second->setVertexData(triangle->getVertexData<VertexStatic>());
  second->setIndexData(Vector<uint32>{2, 1, 0});
  second->setMaterialSlot(1);
  // The second mesh follows Root and Child.
  MeshSkin skin;
  skin.boneNodes = {0, 1};
  skin.inverseBindMatrices = {Matrix4::IDENTITY, TranslationMatrix(Vector3(0.0f, 0.0f, -5.0f))};
  skin.vertices = {MeshSkin::packVertex(Vector<BoneInfluence>{{0, 1.0f}}),
                   MeshSkin::packVertex(Vector<BoneInfluence>{{0, 0.5f}, {1, 0.5f}}),
                   MeshSkin::packVertex(Vector<BoneInfluence>{{1, 1.0f}})};
  second->setSkin(skin);
  const uint32 mesh0 = model->addMesh(triangle);
  const uint32 mesh1 = model->addMesh(second);
  const Quaternion turn(Vector3::UP, Degree(90.0f));
  const uint32 root = model->addNode("Root", ModelNode::NO_PARENT, Vector3(1.0f, 2.0f, 3.0f),
                                     turn, Vector3(2.0f, 2.0f, 2.0f), {});
  addTestNode(*model, "Child", root, TranslationMatrix(Vector3(0.0f, 0.0f, 5.0f)),
              {mesh0, mesh1});
  addTestNode(*model, "Shared", ModelNode::NO_PARENT, Matrix4::IDENTITY, {mesh0});
  const chEngineSDK::UUID bodyId = chEngineSDK::UUID::createRandom();
  model->addMaterialSlot({.name = "Body", .materialId = bodyId, .material = nullptr});
  model->addMaterialSlot({.name = "Empty", .materialId = chEngineSDK::UUID::null(),
                          .material = nullptr});
  const chEngineSDK::UUID skeletonId = chEngineSDK::UUID::createRandom();
  model->setSkeleton(skeletonId, nullptr);
  model->setNodeBone(0, 0);
  model->setNodeBone(1, 1);

  REQUIRE(model->getNodeRestMatrix(1).nearEqual(
      TranslationMatrix(Vector3(0.0f, 0.0f, 5.0f)) * model->getNodeRestMatrix(0)));

  MemoryDataStream stream;
  ModelSerializer::write(stream, *model);
  const Vector<uint8> bytes = readAllBytes(stream);

  SECTION("Everything comes back in the same order")
  {
    const SPtr<Model> read = ModelSerializer::read(stream);
    REQUIRE(read);
    REQUIRE(read->getNodeCount() == 3);
    REQUIRE(read->getMeshCount() == 2);
    for (uint32 i = 0; i < 3; ++i) {
      const ModelNode& expected = model->getNodes()[i];
      const ModelNode& actual = read->getNodes()[i];
      REQUIRE(actual.name == expected.name);
      REQUIRE(actual.parent == expected.parent);
      REQUIRE(actual.position.nearEqual(expected.position));
      REQUIRE(actual.rotation.isRotationEqual(expected.rotation));
      REQUIRE(actual.scale.nearEqual(expected.scale));
      REQUIRE(read->getNodeRestMatrix(i).nearEqual(model->getNodeRestMatrix(i)));
      const Span<const uint32> expectedMeshes = model->getNodeMeshes(i);
      const Span<const uint32> actualMeshes = read->getNodeMeshes(i);
      REQUIRE(actualMeshes.size() == expectedMeshes.size());
      for (SIZE_T j = 0; j < actualMeshes.size(); ++j) {
        REQUIRE(actualMeshes[j] == expectedMeshes[j]);
      }
    }
    for (uint32 i = 0; i < 2; ++i) {
      const Mesh& expected = *model->getMeshes()[i];
      const Mesh& actual = *read->getMeshes()[i];
      REQUIRE(actual.getVertexData() == expected.getVertexData());
      REQUIRE(actual.getIndexData() == expected.getIndexData());
      REQUIRE(actual.getIndexType() == expected.getIndexType());
      REQUIRE(actual.getIndexCount() == expected.getIndexCount());
      REQUIRE(actual.getMaterialSlot() == expected.getMaterialSlot());
      REQUIRE(actual.getVertexLayout().getVertexSize() == sizeof(VertexStatic));
    }
    REQUIRE(read->getMaterialSlots().size() == 2);
    REQUIRE(read->getMaterialSlots()[0].name == "Body");
    REQUIRE(read->getMaterialSlots()[0].materialId == bodyId);
    REQUIRE(read->getMaterialSlots()[1].materialId.isNull());
    REQUIRE(read->getSkeletonId() == skeletonId);
    REQUIRE(read->getNodes()[0].bone == 0);
    REQUIRE(read->getNodes()[1].bone == 1);
    REQUIRE(read->getNodes()[2].bone == ModelNode::NO_BONE);
    REQUIRE_FALSE(read->getMeshes()[0]->hasSkin());
    const MeshSkin& readSkin = read->getMeshes()[1]->getSkin();
    REQUIRE(readSkin.boneNodes == skin.boneNodes);
    REQUIRE(readSkin.inverseBindMatrices[1].nearEqual(skin.inverseBindMatrices[1]));
    REQUIRE(readSkin.vertices.size() == 3);
    REQUIRE(readSkin.vertices[1].bones[1] == skin.vertices[1].bones[1]);
    REQUIRE(readSkin.vertices[1].weights[1] == skin.vertices[1].weights[1]);

    // Saving what was read gives the same bytes.
    MemoryDataStream again;
    ModelSerializer::write(again, *read);
    REQUIRE(readAllBytes(again) == bytes);
  }

  SECTION("A cut file is rejected")
  {
    for (const SIZE_T size : {SIZE_T(0), SIZE_T(10), bytes.size() / 2, bytes.size() - 1}) {
      MemoryDataStream cut;
      cut.write(bytes.data(), size);
      cut.seek(0);
      REQUIRE_FALSE(ModelSerializer::read(cut));
    }
  }

  SECTION("A skin bone past the nodes is rejected")
  {
    MeshSkin broken = skin;
    broken.boneNodes[1] = 7;
    second->setSkin(broken);
    MemoryDataStream other;
    ModelSerializer::write(other, *model);
    other.seek(0);
    REQUIRE_FALSE(ModelSerializer::read(other));
  }

  SECTION("A newer version is rejected")
  {
    Vector<uint8> newer = bytes;
    const uint32 version = ModelHeader::VERSION + 1;
    std::memcpy(newer.data(), &version, sizeof(version));
    MemoryDataStream other;
    other.write(newer.data(), newer.size());
    other.seek(0);
    REQUIRE_FALSE(ModelSerializer::read(other));
  }
}

TEST_CASE("chCore - Model skeleton from joints", "[Resources]")
{
  // Scene > Armature > Hips > Spine > Head, Hips > Leg, Scene > Prop, and a second root.
  Model model;
  const auto add = [&model](StringView name, uint32 parent) {
    return model.addNode(name, parent, Vector3::ZERO, Quaternion::IDENTITY, Vector3::UNIT,
                         Span<const uint32>());
  };
  const uint32 scene = add("Scene", ModelNode::NO_PARENT);
  const uint32 armature = add("Armature", scene);
  const uint32 hips = add("Hips", armature);
  const uint32 spine = add("Spine", hips);
  const uint32 head = add("Head", spine);
  const uint32 leg = add("Leg", hips);
  const uint32 prop = add("Prop", scene);
  const uint32 other = add("Other", ModelNode::NO_PARENT);
  const Vector<ModelNode>& nodes = model.getNodes();

  SECTION("Nodes above the lowest common one stay plain nodes")
  {
    const uint32 joints[] = {spine, leg};
    const SPtr<Skeleton> skeleton = model.buildSkeleton(joints);
    REQUIRE(skeleton);
    REQUIRE(skeleton->getBoneCount() == 3);
    REQUIRE(nodes[scene].bone == ModelNode::NO_BONE);
    REQUIRE(nodes[armature].bone == ModelNode::NO_BONE);
    REQUIRE(nodes[head].bone == ModelNode::NO_BONE);
    REQUIRE(skeleton->getBones()[nodes[hips].bone].name == "Hips");
    REQUIRE(skeleton->getBones()[nodes[hips].bone].parent == SkeletonBone::NO_PARENT);
    REQUIRE(skeleton->getBones()[nodes[spine].bone].parent == nodes[hips].bone);
    REQUIRE(skeleton->getBones()[nodes[leg].bone].parent == nodes[hips].bone);

    // Built again, the nodes that are no longer bones lose their bone.
    const uint32 headOnly[] = {head};
    const SPtr<Skeleton> single = model.buildSkeleton(headOnly);
    REQUIRE(single->getBoneCount() == 1);
    REQUIRE(nodes[head].bone == 0);
    REQUIRE(nodes[hips].bone == ModelNode::NO_BONE);
  }

  SECTION("Joints in other branches reach their common node")
  {
    const uint32 joints[] = {head, prop};
    const SPtr<Skeleton> skeleton = model.buildSkeleton(joints);
    REQUIRE(skeleton->getBoneCount() == 6);
    REQUIRE(nodes[scene].bone == 0);
    REQUIRE(nodes[leg].bone == ModelNode::NO_BONE);
    REQUIRE(skeleton->getBones()[nodes[prop].bone].parent == nodes[scene].bone);
  }

  SECTION("Joints under different roots make every node above them a bone")
  {
    const uint32 joints[] = {head, other};
    const SPtr<Skeleton> skeleton = model.buildSkeleton(joints);
    REQUIRE(skeleton->getBoneCount() == 6);
    REQUIRE(skeleton->getBones()[nodes[scene].bone].parent == SkeletonBone::NO_PARENT);
    REQUIRE(skeleton->getBones()[nodes[other].bone].parent == SkeletonBone::NO_PARENT);
  }

  SECTION("No joints, no skeleton")
  {
    REQUIRE_FALSE(model.buildSkeleton(Span<const uint32>()));
    const uint32 outside[] = {99};
    REQUIRE_FALSE(model.buildSkeleton(outside));
  }
}

TEST_CASE("chCore - Skeleton", "[Resources]")
{
  // Hips > Spine > Head, and Hips > Leg.
  Skeleton skeleton;
  const uint32 hips = skeleton.addBone({.name = "Hips",
                                        .parent = SkeletonBone::NO_PARENT,
                                        .position = Vector3(0.0f, 0.0f, 1.0f),
                                        .rotation = Quaternion::IDENTITY,
                                        .scale = Vector3::UNIT});
  const uint32 spine = skeleton.addBone({.name = "Spine", .parent = hips});
  skeleton.addBone({.name = "Head", .parent = spine});
  skeleton.addBone({.name = "Leg",
                    .parent = hips,
                    .position = Vector3::ZERO,
                    .rotation = Quaternion(Vector3::RIGHT, Degree(30.0f)),
                    .scale = Vector3::UNIT});
  REQUIRE(skeleton.getBoneCount() == 4);
  REQUIRE(skeleton.findBone("Head") == 2);
  REQUIRE(skeleton.findBone("Tail") == Skeleton::NO_BONE);

  Vector<uint32> boneMap;
  SECTION("A skeleton fits itself")
  {
    REQUIRE(skeleton.mapBones(skeleton, boneMap));
    REQUIRE(boneMap == Vector<uint32>{0, 1, 2, 3});
  }

  SECTION("Fewer bones in another order fit")
  {
    Skeleton part;
    const uint32 partHips = part.addBone({.name = "Hips"});
    part.addBone({.name = "Leg", .parent = partHips});
    part.addBone({.name = "Spine", .parent = partHips});
    REQUIRE(skeleton.mapBones(part, boneMap));
    REQUIRE(boneMap == Vector<uint32>{0, 3, 1});
  }

  SECTION("A root may hang from a bone the other skeleton lacks")
  {
    Skeleton head;
    head.addBone({.name = "Head"});
    REQUIRE(skeleton.mapBones(head, boneMap));
    REQUIRE(boneMap == Vector<uint32>{2});
  }

  SECTION("A missing bone or another parent does not fit")
  {
    Skeleton other;
    other.addBone({.name = "Hips"});
    other.addBone({.name = "Tail", .parent = 0});
    REQUIRE_FALSE(skeleton.mapBones(other, boneMap));

    Skeleton moved;
    moved.addBone({.name = "Spine"});
    moved.addBone({.name = "Leg", .parent = 0});
    REQUIRE_FALSE(skeleton.mapBones(moved, boneMap));
  }

  SECTION("Save and read")
  {
    MemoryDataStream stream;
    skeleton.serialize(stream);
    const Vector<uint8> bytes = readAllBytes(stream);
    const SPtr<Skeleton> read = Skeleton::deserialize(stream);
    REQUIRE(read);
    REQUIRE(read->getBoneCount() == 4);
    for (uint32 i = 0; i < 4; ++i) {
      const SkeletonBone& expected = skeleton.getBones()[i];
      const SkeletonBone& actual = read->getBones()[i];
      REQUIRE(actual.name == expected.name);
      REQUIRE(actual.parent == expected.parent);
      REQUIRE(actual.position.nearEqual(expected.position));
      REQUIRE(actual.rotation.isRotationEqual(expected.rotation));
      REQUIRE(actual.scale.nearEqual(expected.scale));
    }

    for (const SIZE_T size : {SIZE_T(0), SIZE_T(6), bytes.size() - 1}) {
      MemoryDataStream cut;
      cut.write(bytes.data(), size);
      cut.seek(0);
      REQUIRE_FALSE(Skeleton::deserialize(cut));
    }
  }
}
