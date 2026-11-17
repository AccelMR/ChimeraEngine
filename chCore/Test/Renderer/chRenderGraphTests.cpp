/************************************************************************/
/**
 * @file chRenderGraphTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of the render graph and the render setting names.
 */
/************************************************************************/
#include "chGraphicsTypes.h"
#include "chICommandList.h"
#include "chRenderGraph.h"
#include "chRenderSettings.h"
#include "chScene.h"
#include "chTransientTexturePool.h"

#include "catch.hpp"
#include "chRenderTestHelpers.h"

using namespace chEngineSDK;

/**
 * Records what the render graph asks for, so execute() can be checked without a GPU.
 */
class FakeCommandList : public ICommandList
{
 public:
  void
  beginRendering(const RenderingDesc& desc) override
  {
    m_stats.recordRendering();
    renderings.push_back(desc);
  }

  void
  endRendering() override
  {}

  void
  barrier(Span<const TextureBarrier> textureBarriers) override
  {
    m_stats.recordBarriers(textureBarriers.size());
  }

  void
  memoryBarrier(ResourceState before, ResourceState after) override
  {
    CH_PARAMETER_UNUSED(before);
    CH_PARAMETER_UNUSED(after);
    m_stats.recordBarriers(1);
  }

  void
  bindPipeline(const IPipeline& pipeline) override
  {
    CH_PARAMETER_UNUSED(pipeline);
  }

  void
  pushConstants(const void* data, uint32 size, uint32 offset) override
  {
    CH_PARAMETER_UNUSED(data);
    CH_PARAMETER_UNUSED(size);
    CH_PARAMETER_UNUSED(offset);
  }

  void
  bindVertexBuffer(const IBuffer& buffer, uint32 binding, uint64 offset) override
  {
    CH_PARAMETER_UNUSED(buffer);
    CH_PARAMETER_UNUSED(binding);
    CH_PARAMETER_UNUSED(offset);
  }

  void
  bindIndexBuffer(const IBuffer& buffer, IndexType indexType, uint64 offset) override
  {
    CH_PARAMETER_UNUSED(buffer);
    CH_PARAMETER_UNUSED(indexType);
    CH_PARAMETER_UNUSED(offset);
  }

  void
  draw(uint32 vertexCount, uint32 instanceCount, uint32 firstVertex, uint32 firstInstance)
      override
  {
    CH_PARAMETER_UNUSED(firstVertex);
    CH_PARAMETER_UNUSED(firstInstance);
    m_stats.recordDraw(vertexCount, instanceCount);
  }

  void
  drawIndexed(uint32 indexCount,
              uint32 instanceCount,
              uint32 firstIndex,
              int32 vertexOffset,
              uint32 firstInstance) override
  {
    CH_PARAMETER_UNUSED(firstIndex);
    CH_PARAMETER_UNUSED(vertexOffset);
    CH_PARAMETER_UNUSED(firstInstance);
    m_stats.recordDraw(indexCount, instanceCount);
  }

  void
  dispatch(uint32 groupCountX, uint32 groupCountY, uint32 groupCountZ) override
  {
    CH_PARAMETER_UNUSED(groupCountX);
    CH_PARAMETER_UNUSED(groupCountY);
    CH_PARAMETER_UNUSED(groupCountZ);
    m_stats.recordDispatch();
  }

  void
  setViewport(float x, float y, float width, float height, float minDepth, float maxDepth)
      override
  {
    CH_PARAMETER_UNUSED(x);
    CH_PARAMETER_UNUSED(y);
    CH_PARAMETER_UNUSED(width);
    CH_PARAMETER_UNUSED(height);
    CH_PARAMETER_UNUSED(minDepth);
    CH_PARAMETER_UNUSED(maxDepth);
  }

  void
  setScissor(uint32 x, uint32 y, uint32 width, uint32 height) override
  {
    CH_PARAMETER_UNUSED(x);
    CH_PARAMETER_UNUSED(y);
    CH_PARAMETER_UNUSED(width);
    CH_PARAMETER_UNUSED(height);
  }

  void
  writeTimestamp(uint32 index) override
  {
    CH_PARAMETER_UNUSED(index);
  }

  Vector<RenderingDesc> renderings;
};

TEST_CASE("chCore - RenderGraph", "[Renderer]")
{
  const FakeTexture backBuffer(Format::B8G8R8A8_UNORM, 64, 32);
  const FakeTextureView backBufferView;
  const RGTextureDesc colorDesc{
      .format = Format::R16G16B16A16_SFLOAT, .width = 64, .height = 32};
  const RGTextureDesc depthDesc{.format = Format::D32_SFLOAT, .width = 64, .height = 32};

  RenderGraph graph;
  const auto importBackBuffer = [&]() {
    return graph.importTexture("BackBuffer", backBuffer, backBufferView,
                               ResourceState::Undefined, ResourceState::Present);
  };

  SECTION("Forward pass and UI over it")
  {
    const RGTextureHandle output = importBackBuffer();
    REQUIRE(graph.getTextureDesc(output).width == 64);
    const RGTextureHandle depth = graph.createTexture("Depth", depthDesc);
    graph.addPass("Forward").writeColor(output).writeDepth(depth);
    graph.addPass("UI").writeColor(output, LoadOp::Load);
    graph.compile();

    REQUIRE(graph.getCompiledPassCount() == 2);
    REQUIRE(graph.getCompiledPassName(0) == "Forward");
    REQUIRE(graph.getCompiledPassName(1) == "UI");

    const Span<const RGBarrier> forward = graph.getCompiledPassBarriers(0);
    REQUIRE(forward.size() == 2);
    REQUIRE(
        hasBarrier(forward, output, ResourceState::Undefined, ResourceState::RenderTarget));
    REQUIRE(hasBarrier(forward, depth, ResourceState::Undefined, ResourceState::DepthWrite));

    // Writing again in the same state still waits for the first pass.
    const Span<const RGBarrier> ui = graph.getCompiledPassBarriers(1);
    REQUIRE(ui.size() == 1);
    REQUIRE(hasBarrier(ui, output, ResourceState::RenderTarget, ResourceState::RenderTarget));

    const Span<const RGBarrier> finalBarriers = graph.getFinalBarriers();
    REQUIRE(finalBarriers.size() == 1);
    REQUIRE(hasBarrier(finalBarriers, output, ResourceState::RenderTarget,
                       ResourceState::Present));

    REQUIRE(graph.getTextureSlot(output) == RenderGraph::INVALID_INDEX);
    REQUIRE(graph.getTextureSlot(depth) == 0);
    REQUIRE(graph.getTextureUsage(depth) == TextureUsage::DepthStencil);
    REQUIRE(graph.getTransientSlotCount() == 1);
  }

  SECTION("Attachments are stored only when needed later")
  {
    const RGTextureHandle output = importBackBuffer();
    const RGTextureHandle depth = graph.createTexture("Depth", depthDesc);
    graph.addPass("Scene").writeColor(output).writeDepth(depth);
    graph.compile();
    REQUIRE(graph.getAttachmentStoreOp(0, output) == StoreOp::Store);
    REQUIRE(graph.getAttachmentStoreOp(0, depth) == StoreOp::DontCare);

    // An overlay that loads the depth makes the scene pass keep it; the overlay itself
    // is the last user.
    graph.reset();
    const RGTextureHandle output2 = importBackBuffer();
    const RGTextureHandle depth2 = graph.createTexture("Depth", depthDesc);
    graph.addPass("Scene").writeColor(output2).writeDepth(depth2);
    graph.addPass("Overlay").writeColor(output2, LoadOp::Load).writeDepth(depth2, LoadOp::Load);
    graph.compile();
    REQUIRE(graph.getAttachmentStoreOp(0, depth2) == StoreOp::Store);
    REQUIRE(graph.getAttachmentStoreOp(1, depth2) == StoreOp::DontCare);
    REQUIRE(graph.getAttachmentStoreOp(1, output2) == StoreOp::Store);
  }

  SECTION("Passes nobody needs are dropped")
  {
    const RGTextureHandle output = importBackBuffer();
    const RGTextureHandle unused = graph.createTexture("Unused", colorDesc);
    graph.addPass("Unused").writeColor(unused);
    graph.addPass("Overwritten").writeColor(output);
    graph.addPass("Final").writeColor(output, LoadOp::Clear);
    graph.compile();

    REQUIRE(graph.getCompiledPassCount() == 1);
    REQUIRE(graph.getCompiledPassName(0) == "Final");
    REQUIRE(graph.getTextureSlot(unused) == RenderGraph::INVALID_INDEX);
    REQUIRE(graph.getTransientSlotCount() == 0);
  }

  SECTION("Side effects and loads keep passes")
  {
    const RGTextureHandle output = importBackBuffer();
    const RGTextureHandle capture = graph.createTexture("Capture", colorDesc);
    graph.addPass("Capture").writeColor(capture).setSideEffect();
    graph.addPass("Base").writeColor(output);
    graph.addPass("Overlay").writeColor(output, LoadOp::Load);
    graph.compile();

    REQUIRE(graph.getCompiledPassCount() == 3);
  }

  SECTION("A texture read by a later pass")
  {
    const RGTextureHandle output = importBackBuffer();
    const RGTextureHandle color = graph.createTexture("Color", colorDesc);
    const RGTextureHandle depth = graph.createTexture("Depth", depthDesc);
    graph.addPass("Scene").writeColor(color).writeDepth(depth);
    graph.addPass("Resolve").read(color).read(depth).writeColor(output);
    graph.compile();

    REQUIRE(graph.getCompiledPassCount() == 2);
    const Span<const RGBarrier> resolve = graph.getCompiledPassBarriers(1);
    REQUIRE(
        hasBarrier(resolve, color, ResourceState::RenderTarget, ResourceState::ShaderRead));
    REQUIRE(hasBarrier(resolve, depth, ResourceState::DepthWrite, ResourceState::DepthRead));
    REQUIRE(graph.getTextureUsage(color) ==
            (TextureUsageFlags(TextureUsage::ColorAttachment) | TextureUsage::Sampled));
  }

  SECTION("Depth layers written by the pass itself")
  {
    const RGTextureHandle output = importBackBuffer();
    const RGTextureHandle cubes = graph.createTexture("Cubes", depthDesc);
    graph.addPass("CubeFaces").writeDepthLayers(cubes);
    graph.addPass("Lighting").read(cubes).writeColor(output);
    graph.compile();

    REQUIRE(graph.getCompiledPassCount() == 2);
    REQUIRE(hasBarrier(graph.getCompiledPassBarriers(0), cubes, ResourceState::Undefined,
                       ResourceState::DepthWrite));
    REQUIRE(hasBarrier(graph.getCompiledPassBarriers(1), cubes, ResourceState::DepthWrite,
                       ResourceState::DepthRead));
    REQUIRE(graph.getTextureUsage(cubes) ==
            (TextureUsageFlags(TextureUsage::DepthStencil) | TextureUsage::Sampled));
  }

  SECTION("Storage written by a compute pass")
  {
    const RGTextureHandle output = importBackBuffer();
    const RGTextureHandle lookup = graph.createTexture("Lookup", colorDesc);
    graph.addPass("Generate").writeStorage(lookup);
    graph.addPass("Refine").writeStorage(lookup, LoadOp::Load);
    graph.addPass("Lighting").read(lookup).writeColor(output);
    graph.compile();

    REQUIRE(graph.getCompiledPassCount() == 3);
    REQUIRE(hasBarrier(graph.getCompiledPassBarriers(0), lookup, ResourceState::Undefined,
                       ResourceState::UnorderedAccess));
    // The second writer waits for the first.
    REQUIRE(hasBarrier(graph.getCompiledPassBarriers(1), lookup,
                       ResourceState::UnorderedAccess, ResourceState::UnorderedAccess));
    REQUIRE(hasBarrier(graph.getCompiledPassBarriers(2), lookup,
                       ResourceState::UnorderedAccess, ResourceState::ShaderRead));
    REQUIRE(graph.getTextureUsage(lookup) ==
            (TextureUsageFlags(TextureUsage::Storage) | TextureUsage::Sampled));
  }

  SECTION("A storage write that nobody reads is dropped")
  {
    const RGTextureHandle output = importBackBuffer();
    const RGTextureHandle unused = graph.createTexture("Unused", colorDesc);
    graph.addPass("Generate").writeStorage(unused);
    graph.addPass("Lighting").writeColor(output);
    graph.compile();

    REQUIRE(graph.getCompiledPassCount() == 1);
    REQUIRE(graph.getCompiledPassName(0) == "Lighting");
  }

  SECTION("Dead textures give their slot to new ones")
  {
    const RGTextureHandle output = importBackBuffer();
    const RGTextureHandle first = graph.createTexture("First", colorDesc);
    const RGTextureHandle second = graph.createTexture("Second", colorDesc);
    const RGTextureHandle third = graph.createTexture("Third", colorDesc);
    const RGTextureHandle half = graph.createTexture(
        "Half", {.format = colorDesc.format, .width = 32, .height = 16});
    graph.addPass("A").writeColor(first);
    graph.addPass("B").read(first).writeColor(second);
    graph.addPass("C").read(second).writeColor(third);
    graph.addPass("D").read(third).writeColor(half);
    graph.addPass("E").read(half).writeColor(output);
    graph.compile();

    REQUIRE(graph.getCompiledPassCount() == 5);
    REQUIRE(graph.getTextureSlot(first) == 0);
    REQUIRE(graph.getTextureSlot(second) == 1);
    // First died in B, so Third (same description and usage) takes its slot in C.
    REQUIRE(graph.getTextureSlot(third) == 0);
    // Another size never shares.
    REQUIRE(graph.getTextureSlot(half) == 2);
    REQUIRE(graph.getTransientSlotCount() == 3);

    // The slot keeps the state First left it in.
    REQUIRE(hasBarrier(graph.getCompiledPassBarriers(2), third, ResourceState::ShaderRead,
                       ResourceState::RenderTarget));
  }

  SECTION("Imported textures")
  {
    const FakeTexture history(Format::R16G16B16A16_SFLOAT, 64, 32);
    const FakeTextureView historyView;

    SECTION("Without a final state, passes that only write it are dropped")
    {
      const RGTextureHandle scratch =
          graph.importTexture("Scratch", history, historyView, ResourceState::ShaderRead,
                              ResourceState::Undefined);
      graph.addPass("Write").writeColor(scratch);
      graph.compile();
      REQUIRE(graph.getCompiledPassCount() == 0);
      REQUIRE(graph.getFinalBarriers().empty());
    }

    SECTION("Untouched, it still reaches its final state")
    {
      const RGTextureHandle output = importBackBuffer();
      graph.compile();
      REQUIRE(graph.getCompiledPassCount() == 0);
      REQUIRE(hasBarrier(graph.getFinalBarriers(), output, ResourceState::Undefined,
                         ResourceState::Present));
    }

    SECTION("Already in its final state, no barrier")
    {
      const RGTextureHandle kept =
          graph.importTexture("History", history, historyView, ResourceState::ShaderRead,
                              ResourceState::ShaderRead);
      const RGTextureHandle output = importBackBuffer();
      graph.addPass("Use").read(kept).writeColor(output);
      graph.compile();
      REQUIRE(graph.getCompiledPassBarriers(0).size() == 1);
      REQUIRE(graph.getFinalBarriers().size() == 1);
    }
  }

  SECTION("A turned off optional pass skips its code but still clears its outputs")
  {
    const FakeTexture depthTexture(Format::D32_SFLOAT, 64, 32);
    const FakeTextureView depthView;
    int32 runs = 0;
    const auto addPasses = [&]() {
      graph.reset();
      const RGTextureHandle output = importBackBuffer();
      const RGTextureHandle depth =
          graph.importTexture("Depth", depthTexture, depthView, ResourceState::Undefined,
                              ResourceState::Undefined);
      graph.addPass("GBuffer")
          .setOptional()
          .writeColor(output, LoadOp::DontCare)
          .writeDepth(depth)
          .setExecute([&runs](RenderPassContext& context) {
            CH_PARAMETER_UNUSED(context);
            ++runs;
          });
      graph.addPass("UI").writeColor(output, LoadOp::Load).setExecute(
          [&runs](RenderPassContext& context) {
            CH_PARAMETER_UNUSED(context);
            runs += 10;
          });
      graph.compile();
    };

    TransientTexturePool pool;
    FakeCommandList commandList;
    addPasses();
    graph.execute(commandList, pool);
    REQUIRE(runs == 11);
    REQUIRE(commandList.renderings[0].colorAttachments[0].loadOp == LoadOp::DontCare);

    // Not optional passes ignore the list.
    const Vector<String> disabled{"Other", "GBuffer", "UI"};
    graph.setDisabledPasses(disabled);
    commandList.renderings.clear();
    runs = 0;
    addPasses();
    graph.execute(commandList, pool);
    REQUIRE(runs == 10);
    REQUIRE(commandList.renderings.size() == 2);
    REQUIRE(commandList.renderings[0].colorAttachments[0].loadOp == LoadOp::Clear);
    REQUIRE(commandList.renderings[0].depthAttachment.loadOp == LoadOp::Clear);
    REQUIRE(commandList.renderings[1].colorAttachments[0].loadOp == LoadOp::Load);

#if USING(CH_PROFILER)
    const Span<const RenderPassStats> stats = graph.getExecutedPassStats();
    REQUIRE(stats.size() == 2);
    REQUIRE(stats[0].bOptional);
    REQUIRE_FALSE(stats[0].bRan);
    REQUIRE(stats[0].commands.renderings == 1);
    REQUIRE_FALSE(stats[1].bOptional);
    REQUIRE(stats[1].bRan);
#endif // USING(CH_PROFILER)
    graph.setDisabledPasses({});
  }

  SECTION("Reset frees the pass code and the graph builds again")
  {
    const SPtr<int32> counter = chMakeShared<int32>(0);
    for (int32 frame = 0; frame < 3; ++frame) {
      graph.reset();
      const RGTextureHandle output = importBackBuffer();
      graph.addPass("Forward").writeColor(output).setExecute(
          [counter](RenderPassContext& context) {
            CH_PARAMETER_UNUSED(context);
            ++*counter;
          });
      REQUIRE(counter.use_count() == 2);
      graph.compile();
      REQUIRE(graph.getCompiledPassCount() == 1);
    }
    graph.reset();
    REQUIRE(counter.use_count() == 1);
  }
}

TEST_CASE("chCore - Render settings names", "[Renderer]")
{
  for (uint32 i = 0; i < static_cast<uint32>(ViewMode::COUNT); ++i) {
    const ViewMode mode = static_cast<ViewMode>(i);
    REQUIRE(RenderSettingsUtils::viewModeFromName(RenderSettingsUtils::getName(mode)) == mode);
  }
  REQUIRE(RenderSettingsUtils::viewModeFromName("wireFRAME") == ViewMode::Wireframe);
  REQUIRE_FALSE(RenderSettingsUtils::viewModeFromName("Unknown").has_value());
  REQUIRE(RenderSettingsUtils::viewModeFromName(g_cvarViewMode.getDefault()) == ViewMode::Lit);

  for (uint32 i = 0; i < static_cast<uint32>(RenderPathType::COUNT); ++i) {
    const RenderPathType path = static_cast<RenderPathType>(i);
    REQUIRE(RenderSettingsUtils::renderPathFromName(RenderSettingsUtils::getName(path)) ==
            path);
  }
  REQUIRE(RenderSettingsUtils::renderPathFromName("forward") == RenderPathType::Forward);
  REQUIRE_FALSE(RenderSettingsUtils::renderPathFromName("Unknown").has_value());
  REQUIRE(RenderSettingsUtils::renderPathFromName(g_cvarRenderPath.getDefault()) ==
          RenderPathType::Deferred);
}
