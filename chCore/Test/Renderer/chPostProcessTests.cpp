/************************************************************************/
/**
 * @file chPostProcessTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of post-process components and what the render frame copies of them.
 */
/************************************************************************/
#include <cstring>

#include "chCamera.h"
#include "chFileStream.h"
#include "chGameObject.h"
#include "chGameObjectSerializer.h"
#include "chPostProcessComponent.h"
#include "chRenderFrame.h"
#include "chScene.h"

#include "catch.hpp"
#include "chCoreTestHelpers.h"
#include "chPostProcessTestHelpers.h"

using namespace chEngineSDK;

namespace {

NODISCARD float
readFloat(Span<const uint8> bytes, uint32 offset)
{
  float value = 0.0f;
  std::memcpy(&value, bytes.data() + offset, sizeof(value));
  return value;
}

NODISCARD int32
readInt(Span<const uint8> bytes, uint32 offset)
{
  int32 value = 0;
  std::memcpy(&value, bytes.data() + offset, sizeof(value));
  return value;
}

/**
 * The bytes of stream up to cut, as a new stream.
 */
NODISCARD MemoryDataStream
copyStart(MemoryDataStream& stream, SIZE_T cut)
{
  stream.seek(0);
  Vector<uint8> bytes(cut);
  REQUIRE(stream.read(bytes.data(), bytes.size()) == bytes.size());
  MemoryDataStream part;
  REQUIRE(part.write(bytes.data(), bytes.size()) == bytes.size());
  part.seek(0);
  return part;
}

} // namespace

TEST_CASE("chCore - Post-process component", "[Renderer]")
{
  Scene scene("Looks", chEngineSDK::UUID::createRandom());
  const SPtr<ShaderAsset> vignette = makeTestPostProcessShader("PP_Vignette");
  const SPtr<ShaderAsset> grade =
      makeTestPostProcessShader("PP_Grade", PostProcessStage::BeforeTonemap);
  const SPtr<GameObject> owner = scene.createGameObject("Look");
  PostProcessComponent& postProcess = owner->addComponent<PostProcessComponent>();
  REQUIRE(postProcess.addPass(vignette) == 0);
  REQUIRE(postProcess.addPass(grade) == 1);
  REQUIRE(postProcess.getPassCount() == 2);
  REQUIRE(postProcess.findPass("PP_Grade") == 1);
  REQUIRE(postProcess.findPass("PP_Missing") == PostProcessComponent::NOT_FOUND);
  REQUIRE(scene.getPostProcesses().size() == 1);

  const Camera camera = createTestCamera();
  RenderFrame frame;

  SECTION("The frame copies the passes with their parameter bytes")
  {
    REQUIRE(postProcess.setValue(0, "intensity", Vector4(1.5f, 0.0f, 0.0f, 0.0f)));
    REQUIRE(postProcess.setValue(0, "steps", Vector4(7.0f, 0.0f, 0.0f, 0.0f)));
    REQUIRE_FALSE(postProcess.setValue(0, "missing", Vector4::ZERO));
    frame.extract(scene, camera, LinearColor::BLACK);

    const Span<const PostProcessPassItem> passes = frame.getPostProcessPasses();
    REQUIRE(passes.size() == 2);
    REQUIRE(passes[0].shader == vignette.get());
    REQUIRE(passes[0].stage == PostProcessStage::AfterTonemap);
    REQUIRE(passes[1].stage == PostProcessStage::BeforeTonemap);
    REQUIRE(passes[0].parameterOffset == 0);
    REQUIRE(passes[0].parameterSize == vignette->getParameterBufferSize());
    REQUIRE(passes[1].parameterOffset == passes[0].parameterSize);

    const Span<const uint8> bytes = frame.getPostProcessParameters();
    REQUIRE(bytes.size() == passes[0].parameterSize + passes[1].parameterSize);
    REQUIRE(readFloat(bytes, 0) == 1.5f);
    REQUIRE(readFloat(bytes, 16) == 1.0f);
    REQUIRE(readInt(bytes, 32) == 7);
    // The second pass keeps every default.
    REQUIRE(readFloat(bytes, passes[1].parameterOffset) == 1.0f);
    REQUIRE(readInt(bytes, passes[1].parameterOffset + 32) == 3);

    Vector4 value;
    REQUIRE(postProcess.getValue(0, "intensity", value));
    REQUIRE(value.x == 1.5f);
    REQUIRE(postProcess.getValue(1, "intensity", value));
    REQUIRE(value.x == 1.0f);

    postProcess.resetValue(0, "intensity");
    REQUIRE_FALSE(postProcess.findOverride(0, "intensity"));
    frame.extract(scene, camera, LinearColor::BLACK);
    REQUIRE(readFloat(frame.getPostProcessParameters(), 0) == 1.0f);
  }

  SECTION("Disabled passes, zero weights and missing shaders are left out")
  {
    postProcess.setPassEnabled(0, false);
    frame.extract(scene, camera, LinearColor::BLACK);
    REQUIRE(frame.getPostProcessPasses().size() == 1);
    REQUIRE(frame.getPostProcessPasses()[0].shader == grade.get());

    postProcess.setPassEnabled(0, true);
    postProcess.setPassWeight(1, 0.0f);
    postProcess.setPassWeight(0, 3.0f);
    REQUIRE(postProcess.getPass(0).weight == 1.0f);
    frame.extract(scene, camera, LinearColor::BLACK);
    REQUIRE(frame.getPostProcessPasses().size() == 1);
    REQUIRE(frame.getPostProcessPasses()[0].shader == vignette.get());

    postProcess.setPassShader(0, nullptr);
    frame.extract(scene, camera, LinearColor::BLACK);
    REQUIRE(frame.getPostProcessPasses().empty());

    postProcess.setEnabled(false);
    REQUIRE(scene.getPostProcesses().empty());
  }

  SECTION("Passes move, and a new compile rebuilds the bytes")
  {
    postProcess.movePass(1, 0);
    REQUIRE(postProcess.getPass(0).shader == grade);
    REQUIRE(postProcess.setValue(1, "intensity", Vector4(0.5f, 0.0f, 0.0f, 0.0f)));
    frame.extract(scene, camera, LinearColor::BLACK);
    const uint32 offset = frame.getPostProcessPasses()[1].parameterOffset;
    REQUIRE(readFloat(frame.getPostProcessParameters(), offset) == 0.5f);

#if USING(CH_EDITOR)
    // The parameter moves to the second slot in the new code; the override follows it.
    ShaderAssetData data = vignette->getData();
    std::swap(data.parameters[0], data.parameters[1]);
    ShaderParameters::computeLayout(data.parameters);
    vignette->setData(std::move(data));
    frame.extract(scene, camera, LinearColor::BLACK);
    REQUIRE(readFloat(frame.getPostProcessParameters(), offset + 16) == 0.5f);
#endif // USING(CH_EDITOR)

    postProcess.removePass(0);
    REQUIRE(postProcess.getPassCount() == 1);
    REQUIRE(postProcess.getPass(0).shader == vignette);
  }

  SECTION("Components run lower priority first")
  {
    const SPtr<GameObject> second = scene.createGameObject("Second");
    PostProcessComponent& early = second->addComponent<PostProcessComponent>();
    early.addPass(grade);
    early.setPriority(-1);
    const SPtr<GameObject> third = scene.createGameObject("Third");
    PostProcessComponent& tie = third->addComponent<PostProcessComponent>();
    tie.addPass(vignette);

    const Vector<PostProcessComponent*>& order = scene.getPostProcesses();
    REQUIRE(order.size() == 3);
    REQUIRE(order[0] == &early);
    REQUIRE(order[1] == &postProcess);
    REQUIRE(order[2] == &tie);

    early.setPriority(5);
    REQUIRE(order[2] == &early);
    frame.extract(scene, camera, LinearColor::BLACK);
    REQUIRE(frame.getPostProcessPasses().size() == 4);
    REQUIRE(frame.getPostProcessPasses()[3].shader == grade.get());
  }

  SECTION("Saved and read back, with a missing shader kept")
  {
    postProcess.setPriority(4);
    postProcess.setPassWeight(1, 0.25f);
    postProcess.setPassEnabled(0, false);
    REQUIRE(postProcess.setValue(1, "tint", Vector4(1.0f, 0.5f, 0.0f, 1.0f)));

    MemoryDataStream stream;
    postProcess.serialize(stream);
    const SIZE_T size = stream.tell();
    stream.seek(0);
    PostProcessComponent read;
    REQUIRE(read.deserialize(stream));
    REQUIRE(stream.tell() == size);
    REQUIRE(read.getPriority() == 4);
    REQUIRE(read.getPassCount() == 2);
    // Without an asset manager the shaders are not found, but their ids stay.
    REQUIRE_FALSE(read.getPass(0).shader);
    REQUIRE(read.getPass(0).shaderId == vignette->getUUID());
    REQUIRE_FALSE(read.getPass(0).bEnabled);
    REQUIRE(read.getPass(1).weight == 0.25f);
    REQUIRE(read.getPass(1).overrides.size() == 1);
    REQUIRE(read.getPass(1).overrides[0].name == "tint");
    REQUIRE(read.getPass(1).overrides[0].value == Vector4(1.0f, 0.5f, 0.0f, 1.0f));

    Vector<chEngineSDK::UUID> references;
    read.collectReferences(references);
    REQUIRE(references.size() == 2);

    // Saving what was read gives the same bytes.
    MemoryDataStream again;
    read.serialize(again);
    REQUIRE(again.tell() == size);

    for (const SIZE_T cut : {SIZE_T(2), size / 2, size - 1}) {
      MemoryDataStream part = copyStart(stream, cut);
      PostProcessComponent broken;
      REQUIRE_FALSE(broken.deserialize(part));
    }
  }

  SECTION("Scenes save the component")
  {
    MemoryDataStream stream;
    GameObjectSerializer::write(stream, *owner);
    stream.seek(0);
    Scene other("Copy", chEngineSDK::UUID::createRandom());
    REQUIRE(GameObjectSerializer::read(stream, other, nullptr));
    REQUIRE(other.getRootGameObjects().size() == 1);
    const PostProcessComponent* readBack =
        other.getRootGameObjects()[0]->getComponent<PostProcessComponent>();
    REQUIRE(readBack);
    REQUIRE(readBack->getPassCount() == 2);
    REQUIRE(other.getPostProcesses().size() == 1);
  }
}
