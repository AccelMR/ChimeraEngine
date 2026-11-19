/************************************************************************/
/**
 * @file chAnimationTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of animation clips, the animator playing them and skinning.
 */
/************************************************************************/
#include "chAlgorithm.h"
#include "chAngle.h"
#include "chAnimationClip.h"
#include "chAnimationClipAsset.h"
#include "chAnimationPlayer.h"
#include "chAnimatorComponent.h"
#include "chAssetMeta.h"
#include "chBox.h"
#include "chCamera.h"
#include "chFileStream.h"
#include "chGraphicsTypes.h"
#include "chLinearColor.h"
#include "chMath.h"
#include "chMatrixHelpers.h"
#include "chMesh.h"
#include "chModel.h"
#include "chModelAsset.h"
#include "chModelComponent.h"
#include "chQuaternion.h"
#include "chRay.h"
#include "chRenderFrame.h"
#include "chScene.h"
#include "chSkeleton.h"
#include "chStringUtils.h"
#include "chUUID.h"
#include "chVector3.h"

#include "catch.hpp"
#include "chCoreTestHelpers.h"
#include "chModelTestHelpers.h"

using namespace chEngineSDK;

TEST_CASE("chCore - Animation clip", "[Animation]")
{
  const SPtr<AnimationClip> clip = createTestClip();
  REQUIRE(clip->getDuration() == 2.0f);
  REQUIRE(clip->getTracks().size() == 2);
  // Channels that never change keep one key.
  const AnimationTrack& arm = clip->getTracks()[0];
  const AnimationTrack& base = clip->getTracks()[1];
  REQUIRE(arm.positionCount == 3);
  REQUIRE(arm.rotationCount == 3);
  REQUIRE(arm.scaleCount == 1);
  REQUIRE(base.positionCount == 1);
  REQUIRE(base.rotationCount == 1);

  Vector<NodeTransform> pose(3);
  const Vector<uint32> targets{2, 0};

  SECTION("Keys are blended at times between them")
  {
    clip->sample(0.5f, targets, pose);
    REQUIRE(pose[2].position.nearEqual(Vector3(1.0f, 0.0f, 0.0f)));
    REQUIRE(pose[2].rotation.isRotationEqual(Quaternion(Vector3::UP, Degree(45.0f)), 1e-6f));
    REQUIRE(pose[0].position.nearEqual(Vector3(0.0f, 0.0f, 1.0f)));
    // A node no track moves is left alone.
    REQUIRE(pose[1].position == Vector3::ZERO);
  }

  SECTION("Times outside the clip hold its ends")
  {
    clip->sample(-1.0f, targets, pose);
    REQUIRE(pose[2].position.nearEqual(Vector3::ZERO));
    clip->sample(10.0f, targets, pose);
    REQUIRE(pose[2].position.nearEqual(Vector3(4.0f, 0.0f, 0.0f)));
  }

  SECTION("Tracks without a target are skipped")
  {
    const Vector<uint32> onlyBase{AnimationClip::NO_TARGET, 1};
    clip->sample(1.0f, onlyBase, pose);
    REQUIRE(pose[2].position == Vector3::ZERO);
    REQUIRE(pose[1].position.nearEqual(Vector3(0.0f, 0.0f, 1.0f)));
  }

  SECTION("Rotation keys stay on one side, so blends take the short way")
  {
    AnimationClip flipped(1.0f, 2);
    const Quaternion turned(Vector3::UP, Degree(20.0f));
    const Vector<Vector3> positions(2, Vector3::ZERO);
    const Vector<Vector3> scales(2, Vector3::UNIT);
    const Vector<Quaternion> rotations{Quaternion::IDENTITY, turned * -1.0f};
    static_cast<void>(flipped.addTrack("Node", positions, rotations, scales));
    const Vector<uint32> target{0};
    flipped.sample(0.5f, target, pose);
    REQUIRE(pose[0].rotation.isRotationEqual(Quaternion(Vector3::UP, Degree(10.0f)), 1e-6f));
  }

  SECTION("Save and read")
  {
    clip->setSkeletonId(chEngineSDK::UUID::createRandom());
    MemoryDataStream stream;
    clip->serialize(stream);
    const Vector<uint8> bytes = readAllBytes(stream);
    const SPtr<AnimationClip> read = AnimationClip::deserialize(stream);
    REQUIRE(read);
    REQUIRE(read->getSkeletonId() == clip->getSkeletonId());
    REQUIRE(read->getFrameCount() == 3);
    REQUIRE(read->getSampleRate() == 1.0f);
    REQUIRE(read->getTracks()[1].nodeName == "Base");
    read->sample(0.5f, targets, pose);
    REQUIRE(pose[2].position.nearEqual(Vector3(1.0f, 0.0f, 0.0f)));

    for (const SIZE_T size : {SIZE_T(0), SIZE_T(10), bytes.size() - 1}) {
      MemoryDataStream cut;
      cut.write(bytes.data(), size);
      cut.seek(0);
      REQUIRE_FALSE(AnimationClip::deserialize(cut));
    }
  }
}

TEST_CASE("chCore - Model animation", "[Animation]")
{
  // Base (one mesh) > Arm (one mesh, at x + 1); the clip moves Arm and holds Base at z + 1.
  const SPtr<Model> model = chMakeShared<Model>();
  const uint32 baseNode = addTestNode(*model, "Base", ModelNode::NO_PARENT, Matrix4::IDENTITY,
                                      {model->addMesh(createTriangleModel()->getMeshes()[0])});
  const uint32 armNode =
      addTestNode(*model, "Arm", baseNode, TranslationMatrix(Vector3(1.0f, 0.0f, 0.0f)),
                  {model->addMesh(createTriangleModel()->getMeshes()[0])});

  Scene scene("Test", chEngineSDK::UUID::createRandom());
  const SPtr<GameObject> gameObject = scene.createGameObject("Animated");
  ModelComponent& component =
      gameObject->addComponent<ModelComponent>(createTestModelAsset(model));
  scene.updateTransforms();
  REQUIRE(component.getPose().empty());
  REQUIRE(component.getNodeMatrix(armNode) == model->getNodeRestMatrix(armNode));

  const auto armItemCenter = [&scene]() {
    // The arm's item is the second one the component added.
    return scene.getRenderItemSlots()[1].worldBounds.center;
  };
  REQUIRE(armItemCenter().nearEqual(Vector3(1.0f, 1.0f, 0.0f)));

  const SPtr<AnimationClipAsset> clipAsset = createTestClipAsset(createTestClip());
  AnimatorComponent& animator = gameObject->addComponent<AnimatorComponent>(clipAsset);
  REQUIRE(animator.hasBoundTracks());
  REQUIRE(animator.isPlaying());
  REQUIRE(component.getPose().size() == model->getNodeCount());
  // At time 0 the arm stands at the clip's first key, under the base lifted by the clip.
  REQUIRE(armItemCenter().nearEqual(Vector3(0.0f, 1.0f, 1.0f)));

  SECTION("Update moves the nodes and their render items")
  {
    scene.update(1.0f);
    REQUIRE(animator.getTime() == 1.0f);
    // Arm at x + 2, turned 90 degrees: the triangle's (0, 1, 0) center goes to (-1, 0, 0).
    REQUIRE(armItemCenter().nearEqual(Vector3(1.0f, 0.0f, 1.0f), 1e-5f));
    gameObject->getTransform().setLocalPosition(Vector3(0.0f, 10.0f, 0.0f));
    scene.updateTransforms();
    REQUIRE(armItemCenter().nearEqual(Vector3(1.0f, 10.0f, 1.0f), 1e-5f));
  }

  SECTION("Looping wraps the time; without it the clip stops at its end")
  {
    scene.update(2.5f);
    REQUIRE(Math::abs(animator.getTime() - 0.5f) < 1e-5f);
    animator.setLooping(false);
    scene.update(5.0f);
    REQUIRE(animator.getTime() == 2.0f);
    REQUIRE_FALSE(animator.isPlaying());
    animator.setPlaying(true);
    REQUIRE(animator.getTime() == 0.0f);
  }

  SECTION("Paused clips only follow the time they are given")
  {
    animator.setPlaying(false);
    scene.update(1.0f);
    REQUIRE(animator.getTime() == 0.0f);
    animator.setTime(1.0f);
    scene.update(0.0f);
    REQUIRE(armItemCenter().nearEqual(Vector3(1.0f, 0.0f, 1.0f), 1e-5f));
  }

  SECTION("Playing the clip that plays does not start it again")
  {
    scene.update(1.0f);
    animator.play(clipAsset, 0.5f);
    REQUIRE(animator.getTime() == 1.0f);
  }

  SECTION("Clearing the clip goes back to the rest pose")
  {
    scene.update(1.0f);
    animator.play(nullptr);
    REQUIRE(component.getPose().empty());
    REQUIRE(armItemCenter().nearEqual(Vector3(1.0f, 1.0f, 0.0f)));
  }

  SECTION("A clip whose tracks name no node moves nothing")
  {
    SPtr<AnimationClip> other = chMakeShared<AnimationClip>(1.0f, 1);
    static_cast<void>(other->addTrack("Missing", Vector<Vector3>{Vector3::UNIT},
                                      Vector<Quaternion>{Quaternion::IDENTITY},
                                      Vector<Vector3>{Vector3::UNIT}));
    animator.play(createTestClipAsset(other));
    REQUIRE_FALSE(animator.hasBoundTracks());
    REQUIRE(armItemCenter().nearEqual(Vector3(1.0f, 1.0f, 0.0f)));
  }

  SECTION("A crossfade blends from the pose shown to the new clip")
  {
    // Holds Arm at x + 5; Base, which it does not name, goes back to rest.
    SPtr<AnimationClip> reach = chMakeShared<AnimationClip>(1.0f, 1);
    static_cast<void>(reach->addTrack("Arm", Vector<Vector3>{Vector3(5.0f, 0.0f, 0.0f)},
                                      Vector<Quaternion>{Quaternion::IDENTITY},
                                      Vector<Vector3>{Vector3::UNIT}));
    animator.play(createTestClipAsset(reach), 1.0f);
    // The fade has not moved yet.
    REQUIRE(armItemCenter().nearEqual(Vector3(0.0f, 1.0f, 1.0f), 1e-5f));
    scene.update(0.5f);
    // Half way, which the eased weight keeps at half.
    REQUIRE(armItemCenter().nearEqual(Vector3(2.5f, 1.0f, 0.5f), 1e-5f));

    SECTION("It ends on the new clip")
    {
      scene.update(0.6f);
      REQUIRE(armItemCenter().nearEqual(Vector3(5.0f, 1.0f, 0.0f), 1e-5f));
    }

    SECTION("Another clip during the fade starts from the blended pose")
    {
      animator.play(clipAsset, 1.0f);
      REQUIRE(armItemCenter().nearEqual(Vector3(2.5f, 1.0f, 0.5f), 1e-5f));
      scene.update(1.0f);
      // Fully on the first clip, which started again: time 1.
      REQUIRE(armItemCenter().nearEqual(Vector3(1.0f, 0.0f, 1.0f), 1e-5f));
    }

    SECTION("Fading to no clip ends at rest")
    {
      animator.play(nullptr, 0.5f);
      scene.update(0.6f);
      REQUIRE(component.getPose().empty());
      REQUIRE(armItemCenter().nearEqual(Vector3(1.0f, 1.0f, 0.0f), 1e-5f));
    }
  }

  SECTION("Removing the animator leaves the model at rest")
  {
    scene.update(1.0f);
    gameObject->removeComponent(animator);
    REQUIRE(component.getPose().empty());
    REQUIRE(armItemCenter().nearEqual(Vector3(1.0f, 1.0f, 0.0f)));
  }

  SECTION("Setting the model again keeps the clip and its time")
  {
    scene.update(1.0f);
    component.setModel(createTestModelAsset(model));
    scene.update(0.0f);
    REQUIRE(animator.getTime() == 1.0f);
    // The render items were made again, so their slots may differ: read the node instead.
    const float* armOrigin = component.getNodeMatrix(armNode).getRow(3);
    REQUIRE(Vector3(armOrigin[0], armOrigin[1], armOrigin[2])
                .nearEqual(Vector3(2.0f, 0.0f, 1.0f), 1e-5f));
  }

  SECTION("A part is not animated")
  {
    component.setModelPart(createTestModelAsset(model), armNode, ModelComponent::ALL,
                           Vector3::ZERO);
    scene.update(1.0f);
    REQUIRE(component.getPose().empty());
    REQUIRE_FALSE(animator.hasBoundTracks());
  }

  SECTION("The clip and how it plays are saved")
  {
    animator.setLooping(false);
    animator.setSpeed(-0.5f);
    MemoryDataStream stream;
    animator.serialize(stream);
    stream.seek(0);
    // Without an AssetManager the clip is not found, but its id is kept.
    AnimatorComponent read;
    REQUIRE(read.deserialize(stream));
    REQUIRE_FALSE(read.isLooping());
    REQUIRE(read.isPlaying());
    REQUIRE(read.getSpeed() == -0.5f);
    Vector<chEngineSDK::UUID> references;
    read.collectReferences(references);
    REQUIRE(Algorithm::contains(references, clipAsset->getUUID()));
  }

  SECTION("Clips are not found without assets")
  {
    String error;
    REQUIRE_FALSE(animator.findClip("Run", error));
    REQUIRE_FALSE(error.empty());
  }
}

TEST_CASE("chCore - Animation pose blend", "[Animation]")
{
  const NodeTransform rest{
      .position = Vector3::ZERO, .rotation = Quaternion::IDENTITY, .scale = Vector3::UNIT};
  const Vector<NodeTransform> from{rest, rest};
  const Quaternion turned(Vector3::UP, Degree(90.0f));
  const Vector<NodeTransform> to{
      {.position = Vector3(2.0f, 0.0f, 0.0f),
       .rotation = turned,
       .scale = Vector3(3.0f, 3.0f, 3.0f)},
      {.position = Vector3(2.0f, 0.0f, 0.0f),
       .rotation = turned * -1.0f,
       .scale = Vector3::UNIT}};
  Vector<NodeTransform> out(2);

  SECTION("Every node blends by the weight, rotations the short way")
  {
    AnimationPlayer::blendPoses(from, to, 0.5f, {}, out);
    REQUIRE(out[0].position.nearEqual(Vector3(1.0f, 0.0f, 0.0f)));
    REQUIRE(out[0].scale.nearEqual(Vector3(2.0f, 2.0f, 2.0f)));
    REQUIRE(out[0].rotation.isRotationEqual(Quaternion(Vector3::UP, Degree(45.0f)), 1e-5f));
    REQUIRE(out[1].rotation.isRotationEqual(Quaternion(Vector3::UP, Degree(45.0f)), 1e-5f));
  }

  SECTION("Node weights keep some nodes on the first pose")
  {
    const Vector<float> nodeWeights{1.0f, 0.0f};
    AnimationPlayer::blendPoses(from, to, 1.0f, nodeWeights, out);
    REQUIRE(out[0].position.nearEqual(Vector3(2.0f, 0.0f, 0.0f)));
    REQUIRE(out[1].position.nearEqual(Vector3::ZERO));
  }
}

TEST_CASE("chCore - Skinned model", "[Animation]")
{
  // Base > Arm (at x + 1), named like the test clip's tracks. One triangle on Base, skinned:
  // its first two vertices follow Base, the third follows Arm.
  const SPtr<Model> model = chMakeShared<Model>();
  const SPtr<Mesh> mesh = createTriangleModel()->getMeshes()[0];
  const uint32 baseNode = addTestNode(*model, "Base", ModelNode::NO_PARENT, Matrix4::IDENTITY,
                                      {model->addMesh(mesh)});
  const uint32 armNode =
      addTestNode(*model, "Arm", baseNode, TranslationMatrix(Vector3(1.0f, 0.0f, 0.0f)), {});
  MeshSkin skin;
  skin.boneNodes = {baseNode, armNode};
  skin.inverseBindMatrices = {Matrix4::IDENTITY,
                              model->getNodeRestMatrix(armNode).getInverseAffine()};
  const VertexSkin onBase{.bones = {0, 0, 0, 0}, .weights = {255, 0, 0, 0}};
  const VertexSkin onArm{.bones = {1, 0, 0, 0}, .weights = {255, 0, 0, 0}};
  skin.vertices = {onBase, onBase, onArm};
  mesh->setSkin(std::move(skin));

  SECTION("Each bone gets the box of its vertices in its own space")
  {
    const Vector<SkinBoneBounds>& boneBounds = mesh->getSkinBoneBounds();
    REQUIRE(boneBounds.size() == 2);
    REQUIRE(boneBounds[0].bone == 0);
    REQUIRE(boneBounds[0].box.minPoint.nearEqual(Vector3(-1.0f, 0.0f, 0.0f)));
    REQUIRE(boneBounds[0].box.maxPoint.nearEqual(Vector3(1.0f, 0.0f, 0.0f)));
    REQUIRE(boneBounds[1].box.minPoint.nearEqual(Vector3(-1.0f, 2.0f, 0.0f)));
  }

  Scene scene("Test", chEngineSDK::UUID::createRandom());
  const SPtr<GameObject> gameObject = scene.createGameObject("Skinned");
  ModelComponent& component =
      gameObject->addComponent<ModelComponent>(createTestModelAsset(model));
  scene.updateTransforms();
  const RenderItem& item = scene.getRenderItemSlots()[0];
  REQUIRE(item.skinMatrices);

  SECTION("At rest the skin leaves the vertices where they are")
  {
    REQUIRE(item.skinMatrices[0] == Matrix4::IDENTITY);
    REQUIRE(item.skinMatrices[1].nearEqual(Matrix4::IDENTITY));
    REQUIRE(item.worldMatrix == Matrix4::IDENTITY);
  }

  // At time 0 the clip lifts Base by z + 1 and puts Arm at Base's origin, so the third
  // vertex moves from (0, 2, 0) to (-1, 2, 1).
  AnimatorComponent& animator =
      gameObject->addComponent<AnimatorComponent>(createTestClipAsset(createTestClip()));
  const uint64 posedRevision = item.skinRevision;
  REQUIRE(item.skinMatrices[0].nearEqual(TranslationMatrix(Vector3(0.0f, 0.0f, 1.0f))));
  REQUIRE(item.skinMatrices[1].nearEqual(TranslationMatrix(Vector3(-1.0f, 0.0f, 1.0f))));

  SECTION("Bounds are the posed bone boxes")
  {
    const AABox box = item.worldBounds.getBox();
    REQUIRE(box.minPoint.nearEqual(Vector3(-1.0f, 0.0f, 1.0f), 1e-5f));
    REQUIRE(box.maxPoint.nearEqual(Vector3(1.0f, 2.0f, 1.0f), 1e-5f));
  }

  SECTION("Moving the object moves the item without skinning again")
  {
    gameObject->getTransform().setLocalPosition(Vector3(0.0f, 10.0f, 0.0f));
    scene.updateTransforms();
    REQUIRE(item.skinRevision == posedRevision);
    REQUIRE(item.worldMatrix.nearEqual(TranslationMatrix(Vector3(0.0f, 10.0f, 0.0f))));
    REQUIRE(item.worldBounds.getBox().minPoint.nearEqual(Vector3(-1.0f, 10.0f, 1.0f), 1e-5f));
  }

  SECTION("A new pose gives a new revision")
  {
    scene.update(1.0f);
    REQUIRE(item.skinRevision != posedRevision);
    animator.setPlaying(false);
    const uint64 pausedRevision = item.skinRevision;
    scene.update(1.0f);
    REQUIRE(item.skinRevision == pausedRevision);
  }

  SECTION("Picking hits the posed triangle")
  {
    // Inside the posed triangle, outside the one at rest.
    const Ray ray(Vector3(-0.8f, 1.5f, 5.0f), Vector3(0.0f, 0.0f, -1.0f));
    SceneRayHit hit;
    REQUIRE(scene.raycast(ray, hit));
    REQUIRE(Math::abs(hit.distance - 4.0f) < 1e-4f);
    float distance = 0.0f;
    REQUIRE_FALSE(mesh->raycast(ray, distance));
  }

  SECTION("The frame copies the skin matrices")
  {
    RenderFrame frame;
    frame.extract(scene, createTestCamera(), LinearColor::BLACK);
    const RenderItem& copy = frame.getItems()[0];
    REQUIRE(copy.skinMatrices != item.skinMatrices);
    REQUIRE(copy.skinMatrices[1] == item.skinMatrices[1]);
    REQUIRE(copy.sceneSlot == 0);
    REQUIRE(copy.skinnedVertices == nullptr);
  }

  SECTION("A part of a model is never skinned")
  {
    component.setModelPart(createTestModelAsset(model), baseNode, ModelComponent::ALL,
                           Vector3::ZERO);
    for (const RenderItem& slot : scene.getRenderItemSlots()) {
      REQUIRE((!slot.mesh || !slot.skinMatrices));
    }
  }
}
