/************************************************************************/
/**
 * @file chSpritePass.cpp
 * @author AccelMR
 * @date 2026/11/12
 * @brief Draws the sprites of a frame as instanced quads.
 */
/************************************************************************/

#include "chSpritePass.h"

#include <chrono>

#include "chCamera.h"
#include "chIBuffer.h"
#include "chICommandList.h"
#include "chIGraphicsAPI.h"
#include "chIPipeline.h"
#include "chISampler.h"
#include "chIShader.h"
#include "chMath.h"
#include "chMatrix4.h"
#include "chShaderAsset.h"
#include "chSpriteMaterialAsset.h"
#include "chRenderSettings.h"
#include "chSpriteShader.h"

namespace chEngineSDK {

namespace {
// Must match CH_SPRITE_BATCH_HEADER in chSpriteBatch.hlsli.
struct SpriteBatchHeader
{
  Matrix4 viewProjection;
  uint32 pointSamplerIndex;
  uint32 linearSamplerIndex;
  uint32 materialParametersIndex;
  uint32 materialParametersOffset;
  float spriteTime;
  uint32 padding[3];
};
static_assert(sizeof(SpriteBatchHeader) == 96, "The sprite batch header must be 96 bytes");

// Must match PushConstants in sprite.hlsl.
struct SpritePushConstants
{
  SpriteBatchHeader header;
  uint32 instanceBufferIndex;
  // Every draw starts at instance 0, so where its run starts in the buffer comes here.
  uint32 firstInstance;
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

// The header of chSpriteBatch.hlsli, then the batch's own bytes.
struct CustomPushConstants
{
  SpriteBatchHeader header;
  Array<uint8, CustomSpriteBatch::MAX_PARAMETER_BYTES> parameters;
};
static_assert(sizeof(CustomPushConstants) == 128, "Custom push constants must fill 128 bytes");

// A few materials of the biggest parameter blocks.
constexpr uint32 kInitialMaterialCapacity = 4 * ShaderParameters::MAX_BUFFER_SIZE;

NODISCARD int64
getTicks() noexcept
{
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

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
  m_startTicks = getTicks();
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
  prepareMaterials(customBatches, colorFormat);
  const SpriteBatchHeader header{
      .viewProjection = camera.getViewProjectionMatrix(),
      .pointSamplerIndex = m_pointSampler->getBindlessIndex(),
      .linearSamplerIndex = m_linearSampler->getBindlessIndex(),
      .materialParametersIndex = m_materialBufferIndex,
      .materialParametersOffset = 0,
      .spriteTime = static_cast<float>(getTicks() - m_startTicks) * 0.000001f,
      .padding = {0, 0, 0}};
  const SpritePushConstants spriteConstants{
      .header = header,
      .instanceBufferIndex = instanceBuffer ? instanceBuffer->getBindlessIndex() : 0,
      .firstInstance = 0};
  const uint32 glyphRunBufferIndex = glyphRunBuffer ? glyphRunBuffer->getBindlessIndex() : 0;
  const uint32 encodedOffset = bEncodeOutput ? kBlendModes * 2u : 0u;

  m_customDraws.clear();
  for (const SpriteRun& run : m_batcher.getRuns()) {
    if (run.kind != SpriteRunKind::Custom) {
      continue;
    }
    const CustomSpriteBatch& batch = customBatches[run.bufferIndex];
    const IPipeline* pipeline = batch.shader->getPipeline(batch.blendMode, colorFormat);
    uint32 materialOffset = 0;
    // A material that cannot draw leaves the batch to its own shaders.
    if (batch.material) {
      if (const IPipeline* materialPipeline = getMaterialPipeline(
              *batch.material, batch.shader->getVertexShader(), batch.blendMode, colorFormat)) {
        pipeline = materialPipeline;
        materialOffset = addFrameMaterial(*batch.material);
      }
    }
    m_customDraws.push_back({.pipeline = pipeline,
                             .vertexCount = batch.vertexCount,
                             .instanceCount = batch.instanceCount,
                             .parameters = batch.parameters,
                             .materialParametersOffset = materialOffset});
  }

  graph.addPass(passName)
      .setOptional()
      .writeColor(output, loadOp, clearColor)
      .setExecute([this, pipelines, spriteConstants, glyphRunBufferIndex, encodedOffset](
                      RenderPassContext& context) {
        ICommandList& commandList = context.getCommandList();
        SpritePushConstants sprite = spriteConstants;
        const SpriteBatchHeader& header = spriteConstants.header;
        GlyphRunPushConstants glyph{.viewProjection = header.viewProjection,
                                    .runBufferIndex = glyphRunBufferIndex,
                                    .firstRun = 0,
                                    .pointSamplerIndex = header.pointSamplerIndex,
                                    .linearSamplerIndex = header.linearSamplerIndex,
                                    .bWritableBuffer = 0};
        CustomPushConstants custom{.header = header, .parameters = {}};
        SIZE_T customIndex = 0;
        SIZE_T materialIndex = 0;
        for (const SpriteRun& run : m_batcher.getRuns()) {
          const uint32 blend = static_cast<uint32>(run.blendMode);
          if (run.kind == SpriteRunKind::Custom) {
            const CustomDraw& draw = m_customDraws[customIndex++];
            if (draw.pipeline == nullptr) {
              continue;
            }
            custom.parameters = draw.parameters;
            custom.header.materialParametersOffset = draw.materialParametersOffset;
            commandList.bindPipeline(*draw.pipeline);
            commandList.pushConstants(&custom, sizeof(custom));
            commandList.draw(draw.vertexCount, draw.instanceCount);
            continue;
          }
          if (run.kind == SpriteRunKind::Sprites) {
            sprite.firstInstance = run.firstInstance;
            const IPipeline* pipeline = pipelines[blend + encodedOffset];
            if (run.material) {
              const MaterialDraw& draw = m_materialDraws[materialIndex++];
              if (draw.pipeline) {
                pipeline = draw.pipeline;
                sprite.header.materialParametersOffset = draw.parametersOffset;
              }
            }
            commandList.bindPipeline(*pipeline);
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
void
SpritePass::prepareMaterials(Span<const CustomSpriteBatch> customBatches,
                             const Format colorFormat)
{
  m_materialDraws.clear();
  m_frameMaterials.clear();
  m_frameMaterialOffsets.clear();
  m_materialBytes.clear();
  m_materialBufferIndex = 0;
  for (const SpriteRun& run : m_batcher.getRuns()) {
    if (run.kind != SpriteRunKind::Sprites || !run.material) {
      continue;
    }
    MaterialDraw& draw = m_materialDraws.emplace_back();
    draw.pipeline = getMaterialPipeline(*run.material, m_vertexShader, run.blendMode, colorFormat);
    if (draw.pipeline) {
      draw.parametersOffset = addFrameMaterial(*run.material);
    }
  }
  // Added here, before the upload, so the custom draws made after find their bytes.
  for (const CustomSpriteBatch& batch : customBatches) {
    if (batch.material && batch.material->canDraw()) {
      static_cast<void>(addFrameMaterial(*batch.material));
    }
  }
  if (m_materialBytes.empty()) {
    return;
  }
  const uint32 frameIndex = IGraphicsAPI::instance().getFrameIndex();
  const IBuffer* buffer = uploadToFrameBuffer(Span<const uint8>(m_materialBytes),
                                              kInitialMaterialCapacity,
                                              m_materialBuffers[frameIndex],
                                              m_materialCapacities[frameIndex]);
  m_materialBufferIndex = buffer ? buffer->getBindlessIndex() : 0;
}

/*
 */
uint32
SpritePass::addFrameMaterial(const SpriteMaterialAsset& material)
{
  // A frame holds a few materials, so a search is cheaper than a map.
  for (SIZE_T slot = 0; slot < m_frameMaterials.size(); ++slot) {
    if (m_frameMaterials[slot] == &material) {
      return m_frameMaterialOffsets[slot];
    }
  }
  const uint32 offset = static_cast<uint32>(m_materialBytes.size());
  const Span<const uint8> bytes = material.getParameterData();
  m_frameMaterials.push_back(&material);
  m_frameMaterialOffsets.push_back(offset);
  m_materialBytes.insert(m_materialBytes.end(), bytes.begin(), bytes.end());
  return offset;
}

/*
 */
const IPipeline*
SpritePass::getMaterialPipeline(const SpriteMaterialAsset& material,
                                const SPtr<IShader>& vertexShader,
                                const SpriteBlendMode blendMode,
                                const Format colorFormat)
{
  if (!material.canDraw() || !vertexShader) {
    return nullptr;
  }
  ShaderAsset& shader = *material.getShader();
  const uint32 revision = shader.getRevision();
  MaterialPipeline* reusable = nullptr;
  for (MaterialPipeline& entry : m_materialPipelines) {
    if (entry.shader == &shader && entry.vertexShader == vertexShader.get() &&
        entry.blendMode == blendMode && entry.format == colorFormat) {
      if (entry.revision == revision) {
        return entry.pipeline;
      }
      reusable = &entry;
    }
  }
  const SPtr<IShader>& pixelShader = shader.getPixelShader();
  const IPipeline* pipeline =
      pixelShader ? m_pipelineCache
                        .getOrCreate(SpriteShader::makePipelineDesc(vertexShader, pixelShader,
                                                                    blendMode, colorFormat))
                        .get()
                  : nullptr;
  if (!reusable) {
    reusable = &m_materialPipelines.emplace_back();
  }
  *reusable = {.shader = &shader,
               .vertexShader = vertexShader.get(),
               .revision = revision,
               .blendMode = blendMode,
               .format = colorFormat,
               .pipeline = pipeline};
  return pipeline;
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
