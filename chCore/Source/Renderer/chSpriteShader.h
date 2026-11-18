/************************************************************************/
/**
 * @file chSpriteShader.h
 * @author AccelMR
 * @date 2026/11/16
 * @brief
 *  Shaders of a plugin that draw sprites sorted with the engine's.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chGraphicsTypes.h"
#include "chPath.h"
#include "chPipelineCache.h"
#include "chSpriteItem.h"

namespace chEngineSDK {
class IPipeline;
class IShader;

/**
 * Exists so a plugin can draw sprites with a vertex and pixel shader of its own, sorted with
 * every other sprite of the frame (see CustomSpriteBatch): it loads both shaders on first
 * use and keeps one pipeline per blend mode and target format, with the blend states of the
 * engine's sprites.
 *
 * Plugins are unloaded after the graphics API stops, so every SpriteShader is listed and
 * the application calls releaseAll() before that; one used again afterwards loads again.
 * Main thread only.
 */
class CH_CORE_EXPORT SpriteShader
{
 public:
  /**
   * directory holds one folder per shader format
   * (EnginePaths::getEnginePluginShaderDirectory); name is the .hlsl file without extension,
   * with a VSMain and a PSMain.
   */
  SpriteShader(Path directory, String name);

  ~SpriteShader();

  SpriteShader(const SpriteShader&) = delete;
  SpriteShader&
  operator=(const SpriteShader&) = delete;

  /**
   * Null when the shaders could not be loaded (logged once).
   */
  NODISCARD const IPipeline*
  getPipeline(SpriteBlendMode blendMode, Format colorFormat);

  NODISCARD FORCEINLINE const String&
  getName() const noexcept { return m_name; }

  /**
   * Loaded on first use; null when the shaders could not be loaded. Sprite materials run
   * their pixel shader behind it.
   */
  NODISCARD const SPtr<IShader>&
  getVertexShader();

  void
  releaseGpuObjects();

  /**
   * Releases the GPU objects of every SpriteShader.
   */
  static void
  releaseAll();

  /**
   * The pipeline every sprite shader of the engine uses: no culling, no depth, alpha or
   * additive blending into one color target.
   */
  NODISCARD static GraphicsPipelineDesc
  makePipelineDesc(const SPtr<IShader>& vertexShader,
                   const SPtr<IShader>& fragmentShader,
                   SpriteBlendMode blendMode,
                   Format colorFormat);

 private:
  NODISCARD bool
  loadShaders();

  Path m_directory;
  String m_name;
  SPtr<IShader> m_vertexShader;
  SPtr<IShader> m_fragmentShader;
  PipelineCache m_pipelineCache;
  Array<const IPipeline*, static_cast<SIZE_T>(SpriteBlendMode::COUNT)> m_pipelines{};
  Format m_pipelineFormat = Format::Unknown;
  bool m_bLoadFailed = false;
};

} // namespace chEngineSDK
