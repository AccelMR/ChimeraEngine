/************************************************************************/
/**
 * @file chAnimationTestHelpers.h
 * @author AccelMR
 * @date 2026/11/17
 * @brief Helpers that build animation graphs for the graph and script tests.
 */
/************************************************************************/
#pragma once

#include "chAnimationClip.h"
#include "chAnimationClipAsset.h"
#include "chAnimationGraph.h"
#include "chAnimationGraphAsset.h"
#include "chModel.h"
#include "chModelAsset.h"
#include "chStringUtils.h"
#include "chUUID.h"
#include "chModelTestHelpers.h"

#include "catch.hpp"

namespace chEngineSDK {

// Hips > Spine > Arm, and Hips > Leg, all at rest at the origin.
inline constexpr uint32 kHips = 0;

inline constexpr uint32 kSpine = 1;

/*
 * Moves every node of the test model along x from startX to endX over duration seconds.
 */
inline SPtr<AnimationClipAsset>
createGraphTestClip(float startX, float endX, float duration)
{
  constexpr uint32 kFrames = 5;
  SPtr<AnimationClip> clip =
      chMakeShared<AnimationClip>(static_cast<float>(kFrames - 1) / duration, kFrames);
  Vector<Vector3> positions;
  for (uint32 i = 0; i < kFrames; ++i) {
    const float alpha = static_cast<float>(i) / static_cast<float>(kFrames - 1);
    positions.push_back(Vector3(startX + (endX - startX) * alpha, 0.0f, 0.0f));
  }
  const Vector<Quaternion> rotations(kFrames, Quaternion::IDENTITY);
  const Vector<Vector3> scales(kFrames, Vector3::UNIT);
  for (const ANSICHAR* node : {"Hips", "Spine", "Arm", "Leg"}) {
    static_cast<void>(clip->addTrack(node, positions, rotations, scales));
  }
  return createTestClipAsset(clip);
}

inline uint32
addGraphTestClip(AnimationGraph& graph, float startX, float endX, float duration)
{
  const SPtr<AnimationClipAsset> clip = createGraphTestClip(startX, endX, duration);
  return graph.addClip(clip->getUUID(), clip);
}

inline SPtr<Model>
createGraphTestModel()
{
  SPtr<Model> model = chMakeShared<Model>();
  addTestNode(*model, "Hips", ModelNode::NO_PARENT, Matrix4::IDENTITY, {});
  addTestNode(*model, "Spine", kHips, Matrix4::IDENTITY, {});
  addTestNode(*model, "Arm", kSpine, Matrix4::IDENTITY, {});
  addTestNode(*model, "Leg", kHips, Matrix4::IDENTITY, {});
  return model;
}

inline SPtr<AnimationGraphAsset>
createTestGraphAsset(const SPtr<AnimationGraph>& graph)
{
  AssetMetadata metadata;
  metadata.uuid = chEngineSDK::UUID::createRandom();
  metadata.assetType = AssetTypeTraits<AnimationGraphAsset>::getTypeId();
  StringUtils::copyToBuffer(metadata.name, "AG_Test");
  return chMakeShared<AnimationGraphAsset>(metadata, graph);
}

} // namespace chEngineSDK
