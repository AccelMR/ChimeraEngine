/************************************************************************/
/**
 * @file chSpritePass.cpp
 * @author AccelMR
 * @date 2026/11/12
 * @brief Draws the sprites of a frame as instanced quads.
 */
/************************************************************************/

#include "chSpritePass.h"

#include "chCamera.h"
#include "chIBuffer.h"
#include "chICommandList.h"
#include "chIGraphicsAPI.h"
#include "chIPipeline.h"
#include "chISampler.h"
#include "chIShader.h"
#include "chMath.h"
#include "chMatrix4.h"
#include "chRenderSettings.h"

namespace chEngineSDK {

namespace {
// Must match PushConstants in sprite.hlsl.
struct SpritePushConstants
{
  Matrix4 viewProjection;
  uint32 instanceBufferIndex;
  // Every draw starts at instance 0, so where its run starts in the buffer comes here.
  uint32 firstInstance;
  uint32 pointSamplerIndex;
  uint32 linearSamplerIndex;
};

// A few thousand sprites, so small scenes never make the buffer again.
constexpr uint32 kInitialInstanceCapacity = 4096;

// Two triangles of a quad, made in the vertex shader from the vertex index.
constexpr uint32 kVerticesPerSprite = 6;

NODISCARD SPtr<IBuffer>
createInstanceBuffer(uint32 capacity)
{
  return IGraphicsAPI::instance().createBuffer(
      {.size = static_cast<uint32>(capacity * sizeof(GpuSpriteInstance)),
       .usage = BufferUsage::StorageBuffer,
       .memoryUsage = MemoryUsage::CpuToGpu});
}

NODISCARD SPtr<ISampler>
createSampler(SamplerFilter filter, SamplerMipmapMode mipmapMode)
{
  // Clamped, so a frame of an atlas never picks up the texels of its neighbor at its edge.
  return IGraphicsAPI::instance().createSampler(
      {.magFilter = filter,
       .minFilter = filter,
       .mipmapMode = mipmapMode,
       .addressModeU = SamplerAddressMode::ClampToEdge,
       .addressModeV = SamplerAddressMode::ClampToEdge,
       .addressModeW = SamplerAddressMode::ClampToEdge});
}
} // namespace

/*
 */
SpritePass::SpritePass()
{
  IGraphicsAPI& graphicsAPI = IGraphicsAPI::instance();
  m_vertexShader = graphicsAPI.loadShader(ShaderStage::Vertex, "sprite");
  m_fragmentShader = graphicsAPI.loadShader(ShaderStage::Fragment, "sprite");
  m_pointSampler = createSampler(SamplerFilter::Nearest, SamplerMipmapMode::Nearest);
  m_linearSampler = createSampler(SamplerFilter::Linear, SamplerMipmapMode::Linear);
}

/*
 */
SpritePass::~SpritePass() = default;

/*
 */
void
SpritePass::addPass(RenderGraph& graph,
                    const Camera& camera,
                    Span<const SpriteItem> sprites,
                    const LinearColor& clearColor,
                    RGTextureHandle output)
{
  m_batcher.build(sprites,
                  camera.getFrustum(),
                  g_cvarSpriteYSort.get() ? camera.getUpVector() : camera.getForwardVector());
  const Span<const GpuSpriteInstance> instances = m_batcher.getInstances();
  const uint32 count = static_cast<uint32>(instances.size());

  const IBuffer* buffer = nullptr;
  if (count > 0) {
    const uint32 frameIndex = IGraphicsAPI::instance().getFrameIndex();
    uint32& capacity = m_capacities[frameIndex];
    if (count > capacity) {
      capacity = Math::max(count, Math::max(capacity * 2, kInitialInstanceCapacity));
      m_instanceBuffers[frameIndex] = createInstanceBuffer(capacity);
    }
    m_instanceBuffers[frameIndex]->update(instances.data(), instances.size_bytes());
    buffer = m_instanceBuffers[frameIndex].get();
  }

  const Format colorFormat = graph.getTextureDesc(output).format;
  Array<const IPipeline*, static_cast<SIZE_T>(SpriteBlendMode::COUNT)> pipelines{};
  for (uint32 mode = 0; mode < pipelines.size(); ++mode) {
    pipelines[mode] = &getPipeline(static_cast<SpriteBlendMode>(mode), colorFormat);
  }
  const SpritePushConstants pushConstants{
      .viewProjection = camera.getViewProjectionMatrix(),
      .instanceBufferIndex = buffer ? buffer->getBindlessIndex() : 0,
      .firstInstance = 0,
      .pointSamplerIndex = m_pointSampler->getBindlessIndex(),
      .linearSamplerIndex = m_linearSampler->getBindlessIndex()};

  graph.addPass("Sprites")
      .setOptional()
      .writeColor(output, LoadOp::Clear, clearColor)
      .setExecute([this, pipelines, pushConstants](RenderPassContext& context) {
        ICommandList& commandList = context.getCommandList();
        SpritePushConstants constants = pushConstants;
        for (const SpriteRun& run : m_batcher.getRuns()) {
          constants.firstInstance = run.firstInstance;
          commandList.bindPipeline(*pipelines[static_cast<uint32>(run.blendMode)]);
          commandList.pushConstants(&constants, sizeof(constants));
          commandList.draw(kVerticesPerSprite, run.instanceCount);
        }
      });
}

/*
 */
const IPipeline&
SpritePass::getPipeline(SpriteBlendMode blendMode, Format colorFormat)
{
  if (colorFormat != m_pipelineFormat) {
    m_pipelines.fill(nullptr);
    m_pipelineFormat = colorFormat;
  }
  const IPipeline*& pipeline = m_pipelines[static_cast<uint32>(blendMode)];
  if (pipeline != nullptr) {
    return *pipeline;
  }

  GraphicsPipelineDesc pipelineDesc{.vertexShader = m_vertexShader,
                                    .fragmentShader = m_fragmentShader,
                                    .raster = {.cullMode = CullMode::None},
                                    .depth = {.testEnable = false, .writeEnable = false},
                                    .colorAttachmentCount = 1};
  pipelineDesc.colorFormats[0] = colorFormat;
  BlendAttachmentState& blend = pipelineDesc.blendStates[0];
  blend.enable = true;
  blend.srcColorFactor = BlendFactor::SrcAlpha;
  blend.dstColorFactor = blendMode == SpriteBlendMode::Additive
                             ? BlendFactor::One
                             : BlendFactor::OneMinusSrcAlpha;
  blend.srcAlphaFactor = BlendFactor::One;
  blend.dstAlphaFactor = BlendFactor::OneMinusSrcAlpha;
  pipeline = m_pipelineCache.getOrCreate(pipelineDesc).get();
  return *pipeline;
}

} // namespace chEngineSDK
