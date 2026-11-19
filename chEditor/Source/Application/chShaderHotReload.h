/************************************************************************/
/**
 * @file chShaderHotReload.h
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Compiles project shaders again when their source files change.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

namespace chEngineSDK {

class ShaderAsset;

/**
 * Exists so a shader saved in an outside code editor shows at once: every
 * Editor.ShaderReloadInterval the write times of the source and of every file it included
 * are compared, for each loaded shader asset, and a change imports the source again into
 * the same asset. A compile that fails keeps the code that worked and leaves its error on
 * the asset for the Asset Inspector.
 */
class ShaderHotReload
{
 public:
  static void
  shutDown();

  /**
   * Call once per frame.
   */
  static void
  update(float deltaTime);

  /**
   * Imports the shader's source again now. False when it does not compile or has no source.
   */
  static bool
  compileAgain(const SPtr<ShaderAsset>& asset);
};

} // namespace chEngineSDK
