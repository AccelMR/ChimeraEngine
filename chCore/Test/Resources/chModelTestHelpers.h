/************************************************************************/
/**
 * @file chModelTestHelpers.h
 * @author AccelMR
 * @date 2026/11/17
 * @brief Helpers that build models, materials and clips for tests of several topics.
 */
/************************************************************************/
#pragma once

#include "chAngle.h"
#include "chAnimationClip.h"
#include "chAnimationClipAsset.h"
#include "chAssetFile.h"
#include "chFileStream.h"
#include "chMaterialAsset.h"
#include "chMatrix4.h"
#include "chMesh.h"
#include "chMeshGenerator.h"
#include "chModel.h"
#include "chModelAsset.h"
#include "chQuaternion.h"
#include "chStringUtils.h"
#include "chUUID.h"

#include "catch.hpp"

namespace chEngineSDK {

inline SPtr<AnimationClipAsset>
createTestClipAsset(const SPtr<AnimationClip>& clip)
{
  AssetMetadata metadata;
  metadata.uuid = chEngineSDK::UUID::createRandom();
  metadata.assetType = AssetTypeTraits<AnimationClipAsset>::getTypeId();
  StringUtils::copyToBuffer(metadata.name, "A_Test");
  return chMakeShared<AnimationClipAsset>(metadata, clip);
}

inline Vector<uint8>
readAllBytes(MemoryDataStream& stream)
{
  Vector<uint8> bytes(stream.size());
  stream.seek(0);
  REQUIRE(stream.read(bytes.data(), bytes.size()) == bytes.size());
  stream.seek(0);
  return bytes;
}

inline uint32
addTestNode(Model& model,
            StringView name,
            uint32 parent,
            const Matrix4& localMatrix,
            const Vector<uint32>& meshIndices)
{
  return model.addNode(name, parent, localMatrix, meshIndices);
}

// One triangle in front of the test camera when the object is at the origin.
inline SPtr<Model>
createTriangleModel()
{
  SPtr<Mesh> mesh = chMakeShared<Mesh>();
  mesh->setVertexData(Vector<VertexStatic>{
      {Vector3(1.0f, 0.0f, 0.0f), Vector3::UP, Vector2(0.0f, 0.0f),
       Vector4(1.0f, 0.0f, 0.0f, 1.0f)},
      {Vector3(-1.0f, 0.0f, 0.0f), Vector3::UP, Vector2(1.0f, 0.0f),
       Vector4(1.0f, 0.0f, 0.0f, 1.0f)},
      {Vector3(0.0f, 2.0f, 0.0f), Vector3::UP, Vector2(0.0f, 1.0f),
       Vector4(1.0f, 0.0f, 0.0f, 1.0f)}});
  mesh->setIndexData(Vector<uint16>{0, 1, 2});

  SPtr<Model> model = chMakeShared<Model>();
  addTestNode(*model, "Root", ModelNode::NO_PARENT, Matrix4::IDENTITY, {model->addMesh(mesh)});
  return model;
}

inline SPtr<ModelAsset>
createTestModelAsset(const SPtr<Model>& model, StringView name = "SM_Test")
{
  AssetMetadata metadata;
  metadata.uuid = chEngineSDK::UUID::createRandom();
  metadata.assetType = AssetTypeTraits<ModelAsset>::getTypeId();
  StringUtils::copyToBuffer(metadata.name, name);
  return chMakeShared<ModelAsset>(metadata, model);
}

// Material assets only need valid metadata until they are saved or loaded.
inline SPtr<MaterialAsset>
createTestMaterialAsset(StringView name)
{
  AssetMetadata metadata;
  metadata.uuid = chEngineSDK::UUID::createRandom();
  metadata.assetType = AssetTypeTraits<MaterialAsset>::getTypeId();
  StringUtils::copyToBuffer(metadata.name, name);
  return chMakeShared<MaterialAsset>(metadata);
}

// Three keys, one per second: "Arm" moves along x and turns 90 degrees around Z between the
// first two keys; "Base" never changes.
inline SPtr<AnimationClip>
createTestClip()
{
  SPtr<AnimationClip> clip = chMakeShared<AnimationClip>(1.0f, 3);
  const Vector<Vector3> armPositions{Vector3(0.0f, 0.0f, 0.0f), Vector3(2.0f, 0.0f, 0.0f),
                                     Vector3(4.0f, 0.0f, 0.0f)};
  const Quaternion turned(Vector3::UP, Degree(90.0f));
  const Vector<Quaternion> armRotations{Quaternion::IDENTITY, turned, turned};
  const Vector<Vector3> units(3, Vector3::UNIT);
  static_cast<void>(clip->addTrack("Arm", armPositions, armRotations, units));
  const Vector<Vector3> basePositions(3, Vector3(0.0f, 0.0f, 1.0f));
  const Vector<Quaternion> baseRotations(3, Quaternion::IDENTITY);
  static_cast<void>(clip->addTrack("Base", basePositions, baseRotations, units));
  return clip;
}

} // namespace chEngineSDK
