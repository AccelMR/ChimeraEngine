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
#include "chSpriteShader.h"

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

// Must match PushConstants in glyphrun.hlsl.
struct GlyphRunPushConstants
{
  Matrix4 viewProjection;
  uint32 runBufferIndex;
  uint32 firstRun;
  uint32 pointSamplerIndex;
  uint32 linearSamplerIndex;
  // A GpuOnly buffer is bound as writable storage, the frame's own buffer as read only.
  uint32 bWritableBuffer;
};

// Must match CH_SPRITE_BATCH_HEADER in chSpriteBatch.hlsli.
struct CustomPushConstants
{
  Matrix4 viewProjection;
  uint32 pointSamplerIndex;
  uint32 linearSamplerIndex;
  Array<uint8, CustomSpriteBatch::MAX_PARAMETER_BYTES> parameters;
};
static_assert(sizeof(CustomPushConstants) == 128, "Custom push constants must fill 128 bytes");

// A few thousand sprites, so small scenes never make the buffer again.
constexpr uint32 kInitialInstanceCapacity = 4096;

constexpr uint32 kInitialGlyphRunCapacity = 1024;

// Two triangles of a quad, made in the vertex shader from the vertex index.
constexpr uint32 kVerticesPerSprite = 6;

NODISCARD SPtr<IBuffer>
createInstanceBuffer(SIZE_T bytes)
{
  return IGraphicsAPI::instance().createBuffer({.size = static_cast<uint32>(bytes),
                                                .usage = BufferUsage::StorageBuffer,
                                                .memoryUsage = MemoryUsage::CpuToGpu});
}

/**
 * Copies data into the frame slot's buffer, made bigger when it does not fit, and gives
 * the buffer (null for no data).
 */
template<typename T>
NODISCARD const IBuffer*
uploadToFrameBuffer(Span<const T> data,
                    uint32 initialCapacity,
                    SPtr<IBuffer>& buffer,
                    uint32& capacity)
{
  const uint32 count = static_cast<uint32>(data.size());
  if (count == 0) {
    return nullptr;
  }
  if (count > capacity) {
    capacity = Math::max(count, Math::max(capacity * 2, initialCapacity));
    buffer = createInstanceBuffer(static_cast<SIZE_T>(capacity) * sizeof(T));
  }
  buffer->update(data.data(), data.size_bytes());
  return buffer.get();
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
  m_encodedVertexShader = graphicsAPI.loadShader(ShaderStage::Vertex, "sprite_encode");
  m_encodedFragmentShader = graphicsAPI.loadShader(ShaderStage::Fragment, "sprite_encode");
  m_glyphVertexShader = graphicsAPI.loadShader(ShaderStage::Vertex, "glyphrun");
  m_glyphFragmentShader = graphicsAPI.loadShader(ShaderStage::Fragment, "glyphrun");
  m_encodedGlyphVertexShader = graphicsAPI.loadShader(ShaderStage::Vertex, "glyphrun_encode");
  m_encodedGlyphFragmentShader = graphicsAPI.loadShader(ShaderStage::Fragment, "glyphrun_encode");
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
                    Span<const GlyphRunItem> glyphRuns,
                    Span<const GlyphRunBatch> glyphRunBatches,
                    Span<const CustomSpriteBatch> customBatches,
                    const LinearColor& clearColor,
                    RGTextureHandle output,
                    LoadOp loadOp,
                    bool bEncodeOutput,
                    StringView passName)
{
  m_batcher.build(sprites,
                  camera.getFrustum(),
                  g_cvarSpriteYSort.get() ? camera.getUpVector() : camera.getForwardVector(),
                  glyphRuns,
                  glyphRunBatches,
                  customBatches);

  const uint32 frameIndex = IGraphicsAPI::instance().getFrameIndex();
  const IBuffer* instanceBuffer = uploadToFrameBuffer(m_batcher.getInstances(),
                                                      kInitialInstanceCapacity,
                                                      m_instanceBuffers[frameIndex],
                                                      m_capacities[frameIndex]);
  const IBuffer* glyphRunBuffer = uploadToFrameBuffer(m_batcher.getGlyphRuns(),
                                                      kInitialGlyphRunCapacity,
                                                      m_glyphRunBuffers[frameIndex],
                                                      m_glyphRunCapacities[frameIndex]);

  const Format colorFormat = graph.getTextureDesc(output).format;
  constexpr uint32 kBlendModes = static_cast<uint32>(SpriteBlendMode::COUNT);
  Array<const IPipeline*, kBlendModes * 4> pipelines{};
  for (uint32 mode = 0; mode < kBlendModes; ++mode) {
    pipelines[mode] =
        &getPipeline(static_cast<SpriteBlendMode>(mode), false, colorFormat, false);
    pipelines[mode + kBlendModes] =
        &getPipeline(static_cast<SpriteBlendMode>(mode), true, colorFormat, false);
    pipelines[mode + kBlendModes * 2] =
        &getPipeline(static_cast<SpriteBlendMode>(mode), false, colorFormat, true);
    pipelines[mode + kBlendModes * 3] =
        &getPipeline(static_cast<SpriteBlendMode>(mode), true, colorFormat, true);
  }
  const SpritePushConstants spriteConstants{
      .viewProjection = camera.getViewProjectionMatrix(),
      .instanceBufferIndex = instanceBuffer ? instanceBuffer->getBindlessIndex() : 0,
      .firstInstance = 0,
      .pointSamplerIndex = m_pointSampler->getBindlessIndex(),
      .linearSamplerIndex = m_linearSampler->getBindlessIndex()};
  const uint32 glyphRunBufferIndex = glyphRunBuffer ? glyphRunBuffer->getBindlessIndex() : 0;
  const uint32 encodedOffset = bEncodeOutput ? kBlendModes * 2u : 0u;

  m_customDraws.clear();
  for (const SpriteRun& run : m_batcher.getRuns()) {
    if (run.kind != SpriteRunKind::Custom) {
      continue;
    }
    const CustomSpriteBatch& batch = customBatches[run.bufferIndex];
    const IPipeline* pipeline = batch.shader->getPipeline(batch.blendMode, colorFormat);
    m_customDraws.push_back({.pipeline = pipeline,
                             .vertexCount = batch.vertexCount,
                             .instanceCount = batch.instanceCount,
                             .parameters = batch.parameters});
  }

  graph.addPass(passName)
      .setOptional()
      .writeColor(output, loadOp, clearColor)
      .setExecute([this, pipelines, spriteConstants, glyphRunBufferIndex, encodedOffset](
                      RenderPassContext& context) {
        ICommandList& commandList = context.getCommandList();
        SpritePushConstants sprite = spriteConstants;
        GlyphRunPushConstants glyph{.viewProjection = spriteConstants.viewProjection,
                                    .runBufferIndex = glyphRunBufferIndex,
                                    .firstRun = 0,
                                    .pointSamplerIndex = spriteConstants.pointSamplerIndex,
                                    .linearSamplerIndex = spriteConstants.linearSamplerIndex,
                                    .bWritableBuffer = 0};
        CustomPushConstants custom{.viewProjection = spriteConstants.viewProjection,
                                   .pointSamplerIndex = spriteConstants.pointSamplerIndex,
                                   .linearSamplerIndex = spriteConstants.linearSamplerIndex,
                                   .parameters = {}};
        SIZE_T customIndex = 0;
        for (const SpriteRun& run : m_batcher.getRuns()) {
          const uint32 blend = static_cast<uint32>(run.blendMode);
          if (run.kind == SpriteRunKind::Custom) {
            const CustomDraw& draw = m_customDraws[customIndex++];
            if (draw.pipeline == nullptr) {
              continue;
            }
            custom.parameters = draw.parameters;
            commandList.bindPipeline(*draw.pipeline);
            commandList.pushConstants(&custom, sizeof(custom));
            commandList.draw(draw.vertexCount, draw.instanceCount);
            continue;
          }
          if (run.kind == SpriteRunKind::Sprites) {
            sprite.firstInstance = run.firstInstance;
            commandList.bindPipeline(*pipelines[blend + encodedOffset]);
            commandList.pushConstants(&sprite, sizeof(sprite));
            commandList.draw(kVerticesPerSprite, run.instanceCount);
            continue;
          }
          const bool bBatch = run.kind == SpriteRunKind::GlyphRunBatch;
          glyph.runBufferIndex = bBatch ? run.bufferIndex : glyphRunBufferIndex;
          glyph.bWritableBuffer = bBatch ? 1u : 0u;
          glyph.firstRun = run.firstInstance;
          commandList.bindPipeline(*pipelines[blend + kBlendModes + encodedOffset]);
          commandList.pushConstants(&glyph, sizeof(glyph));
          // Runs of the frame's own list may hold any count; batches say their most.
          const uint32 glyphs = bBatch ? run.maxGlyphs : GpuGlyphRun::MAX_GLYPHS;
          commandList.draw(kVerticesPerSprite * glyphs, run.instanceCount);
        }
      });
}

/*
 */
const IPipeline&
SpritePass::getPipeline(SpriteBlendMode blendMode, bool bGlyphRuns, Format colorFormat,
                        bool bEncodeOutput)
{
  if (colorFormat != m_pipelineFormat) {
    m_pipelines.fill(nullptr);
    m_pipelineFormat = colorFormat;
  }
  const uint32 blendCount = static_cast<uint32>(SpriteBlendMode::COUNT);
  const uint32 slot = static_cast<uint32>(blendMode) +
                      (bGlyphRuns ? blendCount : 0u) +
                      (bEncodeOutput ? blendCount * 2u : 0u);
  const IPipeline*& pipeline = m_pipelines[slot];
  if (pipeline != nullptr) {
    return *pipeline;
  }

  const SPtr<IShader>& vertexShader =
      bEncodeOutput
          ? (bGlyphRuns ? m_encodedGlyphVertexShader : m_encodedVertexShader)
          : (bGlyphRuns ? m_glyphVertexShader : m_vertexShader);
  const SPtr<IShader>& fragmentShader =
      bEncodeOutput
          ? (bGlyphRuns ? m_encodedGlyphFragmentShader : m_encodedFragmentShader)
          : (bGlyphRuns ? m_glyphFragmentShader : m_fragmentShader);
  const GraphicsPipelineDesc pipelineDesc = SpriteShader::makePipelineDesc(
      vertexShader, fragmentShader, blendMode, colorFormat);
  pipeline = m_pipelineCache.getOrCreate(pipelineDesc).get();
  return *pipeline;
}

} // namespace chEngineSDK
