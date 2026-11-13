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
   * Draws the sprites the camera sees into output, cleared first to clearColor. The sprites
   * and the camera are read here, not when the graph runs.
   */
  void
  addPass(RenderGraph& graph,
          const Camera& camera,
          Span<const SpriteItem> sprites,
          Span<const GlyphRunItem> glyphRuns,
          Span<const GlyphRunBatch> glyphRunBatches,
          const LinearColor& clearColor,
          RGTextureHandle output);

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
  getPipeline(SpriteBlendMode blendMode, bool bGlyphRuns, Format colorFormat);

  SPtr<IShader> m_vertexShader;
  SPtr<IShader> m_fragmentShader;
  SPtr<IShader> m_glyphVertexShader;
  SPtr<IShader> m_glyphFragmentShader;
  SPtr<ISampler> m_pointSampler;
  SPtr<ISampler> m_linearSampler;
  PipelineCache m_pipelineCache;
  // Sprites first, then glyph runs, per blend mode.
  Array<const IPipeline*, static_cast<SIZE_T>(SpriteBlendMode::COUNT) * 2> m_pipelines{};
  Format m_pipelineFormat = Format::Unknown;
  SpriteBatcher m_batcher;
  Array<SPtr<IBuffer>, GraphicsLimits::MAX_FRAMES_IN_FLIGHT> m_instanceBuffers;
  // In instances.
  Array<uint32, GraphicsLimits::MAX_FRAMES_IN_FLIGHT> m_capacities{};
  Array<SPtr<IBuffer>, GraphicsLimits::MAX_FRAMES_IN_FLIGHT> m_glyphRunBuffers;
  Array<uint32, GraphicsLimits::MAX_FRAMES_IN_FLIGHT> m_glyphRunCapacities{};
};

} // namespace chEngineSDK
