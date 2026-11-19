/************************************************************************/
/**
 * @file chPostProcessRenderer.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Adds the post-process passes of a render frame to the render graph.
 */
/************************************************************************/
#include "chPostProcessRenderer.h"

#include <chrono>

#include "chIBuffer.h"
#include "chICommandList.h"
#include "chIGraphicsAPI.h"
#include "chIPipeline.h"
#include "chISampler.h"
#include "chIShader.h"
#include "chITextureView.h"
#include "chMath.h"
#include "chRenderFrame.h"
#include "chShaderAsset.h"

namespace chEngineSDK {

namespace {
// Must match PostProcessPushConstants in chPostProcessCommon.hlsli.
struct PostProcessPushConstants
{
  uint32 sourceIndex;
  uint32 parametersIndex;
  uint32 parametersOffset;
  uint32 linearSamplerIndex;
  uint32 pointSamplerIndex;
  uint32 frameIndex;
  float time;
  float deltaTime;
  float resolution[2];
  float inverseResolution[2];
};
static_assert(sizeof(PostProcessPushConstants) == 48, "Post-process push constants changed");

// Must match PushConstants in postprocessblend.hlsl.
struct BlendPushConstants
{
  uint32 sourceIndex;
  uint32 effectIndex;
  float weight;
};

// A few passes of the biggest parameter blocks, so most projects never make it again.
constexpr uint32 kInitialParameterCapacity = 4 * ShaderParameters::MAX_BUFFER_SIZE;

// A frame that took longer than this was a pause (a breakpoint, a window drag); effects that
// move with deltaTime should not jump.
constexpr float kMaxDeltaTime = 0.25f;

NODISCARD SPtr<ISampler>
createSampler(SamplerFilter filter, SamplerMipmapMode mipmapMode)
{
  return IGraphicsAPI::instance().createSampler(
      {.magFilter = filter,
       .minFilter = filter,
       .mipmapMode = mipmapMode,
       .addressModeU = SamplerAddressMode::ClampToEdge,
       .addressModeV = SamplerAddressMode::ClampToEdge,
       .addressModeW = SamplerAddressMode::ClampToEdge});
}

NODISCARD int64
getTicks() noexcept
{
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

NODISCARD GraphicsPipelineDesc
makePipelineDesc(const SPtr<IShader>& vertexShader,
                 const SPtr<IShader>& fragmentShader,
                 Format format)
{
  GraphicsPipelineDesc desc{.vertexShader = vertexShader,
                            .fragmentShader = fragmentShader,
                            .raster = {.cullMode = CullMode::None},
                            .depth = {.testEnable = false, .writeEnable = false},
                            .colorAttachmentCount = 1};
  desc.colorFormats[0] = format;
  return desc;
}
} // namespace

/*
 */
PostProcessRenderer::PostProcessRenderer()
{
  IGraphicsAPI& graphicsAPI = IGraphicsAPI::instance();
  m_vertexShader = graphicsAPI.loadShader(ShaderStage::Vertex, "postprocess");
  m_blendVertexShader = graphicsAPI.loadShader(ShaderStage::Vertex, "postprocessblend");
  m_blendFragmentShader = graphicsAPI.loadShader(ShaderStage::Fragment, "postprocessblend");
  m_linearSampler = createSampler(SamplerFilter::Linear, SamplerMipmapMode::Linear);
  m_pointSampler = createSampler(SamplerFilter::Nearest, SamplerMipmapMode::Nearest);
  m_parameterBuffers.resize(GraphicsLimits::MAX_FRAMES_IN_FLIGHT);
  m_parameterCapacities.resize(GraphicsLimits::MAX_FRAMES_IN_FLIGHT, 0);
  m_lastTicks = getTicks();
}

/*
 */
PostProcessRenderer::~PostProcessRenderer() = default;

/*
 */
void
PostProcessRenderer::beginFrame(const RenderFrame& frame)
{
  const int64 ticks = getTicks();
  m_deltaTime =
      Math::min(static_cast<float>(ticks - m_lastTicks) * 0.000001f, kMaxDeltaTime);
  m_lastTicks = ticks;
  m_time += m_deltaTime;
  ++m_frameCount;

  m_passes = frame.getPostProcessPasses();
  m_parameterBufferIndex = 0;
  for (Vector<PreparedPass>& prepared : m_prepared) {
    prepared.clear();
  }
  const Span<const uint8> parameters = frame.getPostProcessParameters();
  if (m_passes.empty() || parameters.empty()) {
    return;
  }

  const uint32 slot = IGraphicsAPI::instance().getFrameIndex();
  SPtr<IBuffer>& buffer = m_parameterBuffers[slot];
  uint32& capacity = m_parameterCapacities[slot];
  if (!buffer || capacity < parameters.size()) {
    capacity = Math::max(kInitialParameterCapacity, static_cast<uint32>(parameters.size()) * 2);
    buffer = IGraphicsAPI::instance().createBuffer({.size = capacity,
                                                    .usage = BufferUsage::StorageBuffer,
                                                    .memoryUsage = MemoryUsage::CpuToGpu});
  }
  buffer->update(parameters.data(), parameters.size());
  m_parameterBufferIndex = buffer->getBindlessIndex();
}

/*
 */
uint32
PostProcessRenderer::prepare(const PostProcessStage stage, const Format format)
{
  Vector<PreparedPass>& prepared = m_prepared[static_cast<SIZE_T>(stage)];
  prepared.clear();
  for (const PostProcessPassItem& pass : m_passes) {
    if (pass.stage != stage) {
      continue;
    }
    if (const IPipeline* pipeline = findPipeline(*pass.shader, format)) {
      prepared.push_back({.pass = &pass, .pipeline = pipeline});
    }
  }
  return static_cast<uint32>(prepared.size());
}

/*
 */
RGTextureHandle
PostProcessRenderer::addPasses(RenderGraph& graph,
                               const PostProcessStage stage,
                               RGTextureHandle source,
                               const RGTextureHandle finalOutput)
{
  const Vector<PreparedPass>& prepared = m_prepared[static_cast<SIZE_T>(stage)];
  if (prepared.empty()) {
    return source;
  }
  const RGTextureDesc sourceDesc = graph.getTextureDesc(source);
  const Format format = finalOutput.isValid() ? graph.getTextureDesc(finalOutput).format
                                              : sourceDesc.format;
  const RGTextureDesc chainDesc{.format = format,
                                .width = sourceDesc.width,
                                .height = sourceDesc.height};
  for (SIZE_T i = 0; i < prepared.size(); ++i) {
    const PostProcessPassItem& pass = *prepared[i].pass;
    const IPipeline* pipeline = prepared[i].pipeline;
    const bool bLast = i + 1 == prepared.size();
    const bool bBlend = pass.weight < 1.0f;
    const RGTextureHandle target = bLast && finalOutput.isValid()
                                       ? finalOutput
                                       : graph.createTexture("PostProcess", chainDesc);
    const RGTextureHandle effect =
        bBlend ? graph.createTexture("PostProcessEffect", chainDesc) : target;

    const PostProcessPushConstants pushConstants{
        .sourceIndex = 0,
        .parametersIndex = m_parameterBufferIndex,
        .parametersOffset = pass.parameterOffset,
        .linearSamplerIndex = m_linearSampler->getBindlessIndex(),
        .pointSamplerIndex = m_pointSampler->getBindlessIndex(),
        .frameIndex = m_frameCount,
        .time = m_time,
        .deltaTime = m_deltaTime,
        .resolution = {static_cast<float>(sourceDesc.width),
                       static_cast<float>(sourceDesc.height)},
        .inverseResolution = {1.0f / static_cast<float>(Math::max(sourceDesc.width, 1u)),
                              1.0f / static_cast<float>(Math::max(sourceDesc.height, 1u))}};
    graph.addPass(pass.shader->getName())
        .read(source)
        .writeColor(effect, LoadOp::DontCare)
        .setExecute([pipeline, pushConstants, source](RenderPassContext& context) {
          ICommandList& commandList = context.getCommandList();
          PostProcessPushConstants constants = pushConstants;
          constants.sourceIndex = context.getView(source).getBindlessIndex();
          commandList.bindPipeline(*pipeline);
          commandList.pushConstants(&constants, sizeof(constants));
          commandList.draw(3);
        });

    if (bBlend) {
      addBlendPass(graph, source, effect, target, pass.weight);
    }
    source = target;
  }
  return source;
}

/*
 */
void
PostProcessRenderer::addBlendPass(RenderGraph& graph,
                                  const RGTextureHandle source,
                                  const RGTextureHandle effect,
                                  const RGTextureHandle target,
                                  const float weight)
{
  const IPipeline* pipeline = findBlendPipeline(graph.getTextureDesc(target).format);
  if (!pipeline) {
    return;
  }
  graph.addPass("PostProcessBlend")
      .read(source)
      .read(effect)
      .writeColor(target, LoadOp::DontCare)
      .setExecute([pipeline, source, effect, weight](RenderPassContext& context) {
        ICommandList& commandList = context.getCommandList();
        const BlendPushConstants constants{
            .sourceIndex = context.getView(source).getBindlessIndex(),
            .effectIndex = context.getView(effect).getBindlessIndex(),
            .weight = weight};
        commandList.bindPipeline(*pipeline);
        commandList.pushConstants(&constants, sizeof(constants));
        commandList.draw(3);
      });
}

/*
 */
const IPipeline*
PostProcessRenderer::findPipeline(ShaderAsset& shader, const Format format)
{
  const uint32 revision = shader.getRevision();
  PipelineEntry* reusable = nullptr;
  for (PipelineEntry& entry : m_pipelines) {
    if (entry.shader == &shader && entry.format == format) {
      if (entry.revision == revision) {
        return entry.pipeline;
      }
      reusable = &entry;
    }
  }

  const SPtr<IShader>& vertexShader =
      shader.getVertexShader() ? shader.getVertexShader() : m_vertexShader;
  const SPtr<IShader>& fragmentShader = shader.getPixelShader();
  const IPipeline* pipeline =
      vertexShader && fragmentShader
          ? m_pipelineCache.getOrCreate(makePipelineDesc(vertexShader, fragmentShader, format))
                .get()
          : nullptr;
  if (!reusable) {
    reusable = &m_pipelines.emplace_back();
  }
  *reusable = {.shader = &shader, .revision = revision, .format = format, .pipeline = pipeline};
  return pipeline;
}

/*
 */
const IPipeline*
PostProcessRenderer::findBlendPipeline(const Format format)
{
  for (const PipelineEntry& entry : m_blendPipelines) {
    if (entry.format == format) {
      return entry.pipeline;
    }
  }
  const IPipeline* pipeline =
      m_blendVertexShader && m_blendFragmentShader
          ? m_pipelineCache
                .getOrCreate(makePipelineDesc(m_blendVertexShader, m_blendFragmentShader, format))
                .get()
          : nullptr;
  m_blendPipelines.push_back({.shader = nullptr, .revision = 0, .format = format,
                              .pipeline = pipeline});
  return pipeline;
}

} // namespace chEngineSDK
