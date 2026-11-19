/************************************************************************/
/**
 * @file chSpritePass.h
 * @author AccelMR
 * @date 2026/11/12
 * @brief Draws the sprites of a frame as instanced quads.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chGraphicsTypes.h"
#include "chPipelineCache.h"
#include "chRenderGraph.h"
#include "chSpriteBatcher.h"

namespace chEngineSDK {
class Camera;
class IBuffer;
class ShaderAsset;
class SpriteMaterialAsset;
class ISampler;
class IShader;

/**
 * Exists to draw any number of sprites with a few draw calls: the visible sprites, sorted by
 * SpriteBatcher, go into a storage buffer of the frame slot (made bigger when a frame has
 * more), and each run of one blend mode is one instanced draw of a quad built in the vertex
 * shader. The color target is written in linear values, so it can be any target a path
 * blends into.
 */
class CH_CORE_EXPORT SpritePass
{
 public:
  /**
   * Needs a started IGraphicsAPI.
   */
  SpritePass();

  ~SpritePass();

  /**
   * Draws the sprites the camera sees into output. loadOp Clear clears it to clearColor
   * first; Load keeps what an earlier pass wrote, for overlays. bEncodeOutput writes sRGB
   * values instead of linear ones, for a UNORM target the tonemap does not go through (the
   * UI overlay on the back buffer); custom batches keep their own shaders and are not
   * encoded. The sprites, the batches and the camera are read here, not when the graph runs.
   */
  void
  addPass(RenderGraph& graph,
          const Camera& camera,
          Span<const SpriteItem> sprites,
          Span<const GlyphRunItem> glyphRuns,
          Span<const GlyphRunBatch> glyphRunBatches,
          Span<const CustomSpriteBatch> customBatches,
          const LinearColor& clearColor,
          RGTextureHandle output,
          LoadOp loadOp = LoadOp::Clear,
          bool bEncodeOutput = false,
          StringView passName = "Sprites");

  /**
   * Instances drawn by the last addPass, for the profiler.
   */
  NODISCARD FORCEINLINE uint32
  getLastInstanceCount() const noexcept
  {
    return static_cast<uint32>(m_batcher.getInstances().size());
  }

  /**
   * Glyph runs drawn by the last addPass, batches not counted.
   */
  NODISCARD FORCEINLINE uint32
  getLastGlyphRunCount() const noexcept
  {
    return static_cast<uint32>(m_batcher.getGlyphRuns().size());
  }

 private:
  NODISCARD const IPipeline&
  getPipeline(SpriteBlendMode blendMode, bool bGlyphRuns, Format colorFormat, bool bEncodeOutput);

  /**
   * The pipeline of a material's shader with the sprite vertex shader; null when it cannot
   * draw, so its sprites draw with the engine's shader.
   */
  NODISCARD const IPipeline*
  getMaterialPipeline(const SpriteMaterialAsset& material,
                      const SPtr<IShader>& vertexShader,
                      SpriteBlendMode blendMode,
                      Format colorFormat);

  /**
   * Gives every material the runs use a place in the frame's parameter bytes and fills
   * m_materialDraws, in the order of the runs with a material (sprites and custom batches).
   */
  void
  prepareMaterials(Span<const CustomSpriteBatch> customBatches, Format colorFormat);

  /**
   * Where the material's bytes start in the frame's buffer, added on first use.
   */
  NODISCARD uint32
  addFrameMaterial(const SpriteMaterialAsset& material);

  SPtr<IShader> m_vertexShader;
  SPtr<IShader> m_fragmentShader;
  SPtr<IShader> m_encodedVertexShader;
  SPtr<IShader> m_encodedFragmentShader;
  SPtr<IShader> m_glyphVertexShader;
  SPtr<IShader> m_glyphFragmentShader;
  SPtr<IShader> m_encodedGlyphVertexShader;
  SPtr<IShader> m_encodedGlyphFragmentShader;
  SPtr<ISampler> m_pointSampler;
  SPtr<ISampler> m_linearSampler;
  PipelineCache m_pipelineCache;
  // Blend mode, glyph runs or sprites, linear or encoded output.
  static constexpr SIZE_T kPipelineCount = static_cast<SIZE_T>(SpriteBlendMode::COUNT) * 4;
  Array<const IPipeline*, kPipelineCount> m_pipelines{};
  Format m_pipelineFormat = Format::Unknown;
  struct CustomDraw
  {
    // Null when the batch's shaders could not be loaded: nothing is drawn.
    const IPipeline* pipeline;
    uint32 vertexCount;
    uint32 instanceCount;
    Array<uint8, CustomSpriteBatch::MAX_PARAMETER_BYTES> parameters;
    uint32 materialParametersOffset;
  };

  struct MaterialPipeline
  {
    const ShaderAsset* shader = nullptr;
    const IShader* vertexShader = nullptr;
    uint32 revision = 0;
    SpriteBlendMode blendMode = SpriteBlendMode::Alpha;
    Format format = Format::Unknown;
    const IPipeline* pipeline = nullptr;
  };

  struct MaterialDraw
  {
    // Null draws the run with the engine's shader.
    const IPipeline* pipeline = nullptr;
    uint32 parametersOffset = 0;
  };

  SpriteBatcher m_batcher;
  // The custom runs of the last addPass, in the order of the batcher's runs.
  Vector<CustomDraw> m_customDraws;
  Vector<MaterialPipeline> m_materialPipelines;
  // The material runs of the last addPass, in the order of the batcher's runs.
  Vector<MaterialDraw> m_materialDraws;
  // Every material of the frame and where its bytes start, reused each frame.
  Vector<const SpriteMaterialAsset*> m_frameMaterials;
  Vector<uint32> m_frameMaterialOffsets;
  Vector<uint8> m_materialBytes;
  Array<SPtr<IBuffer>, GraphicsLimits::MAX_FRAMES_IN_FLIGHT> m_materialBuffers;
  Array<uint32, GraphicsLimits::MAX_FRAMES_IN_FLIGHT> m_materialCapacities{};
  uint32 m_materialBufferIndex = 0;
  int64 m_startTicks = 0;
  Array<SPtr<IBuffer>, GraphicsLimits::MAX_FRAMES_IN_FLIGHT> m_instanceBuffers;
  // In instances.
  Array<uint32, GraphicsLimits::MAX_FRAMES_IN_FLIGHT> m_capacities{};
  Array<SPtr<IBuffer>, GraphicsLimits::MAX_FRAMES_IN_FLIGHT> m_glyphRunBuffers;
  Array<uint32, GraphicsLimits::MAX_FRAMES_IN_FLIGHT> m_glyphRunCapacities{};
};

} // namespace chEngineSDK
