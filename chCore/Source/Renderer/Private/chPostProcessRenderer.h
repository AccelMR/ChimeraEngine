/************************************************************************/
/**
 * @file chPostProcessRenderer.h
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Adds the post-process passes of a render frame to the render graph.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chGraphicsTypes.h"
#include "chPipelineCache.h"
#include "chPostProcessItem.h"
#include "chRenderGraph.h"

namespace chEngineSDK {

class IBuffer;
class IPipeline;
class IShader;
class ISampler;
class RenderFrame;

/**
 * Exists so scene renderers run the project's post-process shaders the same way in every
 * path. Each pass reads the image the one before wrote and writes a new one of the same size
 * and format; a pass whose weight is below 1 is blended with what it read by one more pass.
 * The parameter bytes of the whole frame go into one storage buffer per frame slot, and the
 * pipelines are kept per shader revision and format, so a frame builds no descriptions.
 */
class CH_CORE_EXPORT PostProcessRenderer
{
 public:
  PostProcessRenderer();

  ~PostProcessRenderer();

  PostProcessRenderer(const PostProcessRenderer&) = delete;

  PostProcessRenderer&
  operator=(const PostProcessRenderer&) = delete;

  /**
   * Call once per frame before addPasses: uploads the parameters and moves the clock.
   */
  void
  beginFrame(const RenderFrame& frame);

  /**
   * Finds the pipelines of the frame's passes of stage for images of that format; returns
   * how many can draw. Call before addPasses, so a caller knows whether to make an image for
   * them to read.
   */
  uint32
  prepare(PostProcessStage stage, Format format);

  /**
   * Adds the passes prepare found for stage, starting from source. With a valid finalOutput
   * the last pass writes it; returns the image the last pass wrote (source when none).
   */
  RGTextureHandle
  addPasses(RenderGraph& graph,
            PostProcessStage stage,
            RGTextureHandle source,
            RGTextureHandle finalOutput = {});

 private:
  struct PreparedPass
  {
    const PostProcessPassItem* pass = nullptr;
    const IPipeline* pipeline = nullptr;
  };

  struct PipelineEntry
  {
    const ShaderAsset* shader = nullptr;
    uint32 revision = 0;
    Format format = Format::Unknown;
    const IPipeline* pipeline = nullptr;
  };

  NODISCARD const IPipeline*
  findPipeline(ShaderAsset& shader, Format format);

  NODISCARD const IPipeline*
  findBlendPipeline(Format format);

  void
  addBlendPass(RenderGraph& graph,
               RGTextureHandle source,
               RGTextureHandle effect,
               RGTextureHandle target,
               float weight);

  SPtr<IShader> m_vertexShader;
  SPtr<IShader> m_blendVertexShader;
  SPtr<IShader> m_blendFragmentShader;
  SPtr<ISampler> m_linearSampler;
  SPtr<ISampler> m_pointSampler;
  PipelineCache m_pipelineCache;
  Vector<PipelineEntry> m_pipelines;
  Vector<PipelineEntry> m_blendPipelines;
  Vector<SPtr<IBuffer>> m_parameterBuffers;
  Vector<uint32> m_parameterCapacities;
  Span<const PostProcessPassItem> m_passes;
  // Per PostProcessStage, reused every frame.
  Array<Vector<PreparedPass>, static_cast<SIZE_T>(PostProcessStage::COUNT)> m_prepared;
  uint32 m_parameterBufferIndex = 0;
  uint32 m_frameCount = 0;
  float m_time = 0.0f;
  float m_deltaTime = 0.0f;
  int64 m_lastTicks = 0;
};

} // namespace chEngineSDK
