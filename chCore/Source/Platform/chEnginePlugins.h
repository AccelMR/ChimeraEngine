/************************************************************************/
/**
 * @file chEnginePlugins.h
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Loads the engine plugins a project enables.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

namespace chEngineSDK {

/**
 * Exists so the editor and the game load the same engine plugins: the ones the project's
 * .chproject lists in [Plugins] Enabled, from EnginePaths::getEnginePluginDirectory(). Each
 * plugin registers what it brings (components, script bindings) in its loadPlugin(), so they
 * are loaded before the asset, script and scene managers start. The DynamicLibraryManager
 * unloads them at shut down, after everything that runs their code is gone.
 */
class CH_CORE_EXPORT EnginePlugins
{
 public:
  /**
   * Loads every enabled plugin once; a plugin that is missing or has no loadPlugin is
   * logged as an error and skipped. Returns how many were loaded.
   */
  static uint32
  loadEnabled();

  /**
   * Base names of the plugins loadEnabled loaded, in load order.
   */
  NODISCARD static const Vector<String>&
  getLoaded();
};

} // namespace chEngineSDK
