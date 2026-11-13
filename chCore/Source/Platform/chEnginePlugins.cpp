/************************************************************************/
/**
 * @file chEnginePlugins.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Loads the engine plugins a project enables.
 */
/************************************************************************/
#include "chEnginePlugins.h"

#include "chAlgorithm.h"
#include "chDynamicLibManager.h"
#include "chDynamicLibrary.h"
#include "chEnginePaths.h"
#include "chLogger.h"
#include "chProjectDescriptor.h"

namespace chEngineSDK {

CH_LOG_DECLARE_STATIC(EnginePluginsLog, All);

namespace {
Vector<String>&
getLoadedNames()
{
  static Vector<String> names;
  return names;
}
} // namespace

/*
 */
uint32
EnginePlugins::loadEnabled()
{
  Vector<String>& loaded = getLoadedNames();
  const Path& directory = EnginePaths::getEnginePluginDirectory();
  uint32 count = 0;
  for (const String& name : EnginePaths::getProjectDescriptor().plugins) {
    if (Algorithm::contains(loaded, name)) {
      continue;
    }

    const WeakPtr<DynamicLibrary> library =
        DynamicLibraryManager::instance().loadDynLibrary(name, directory);
    if (library.expired()) {
      CH_LOG_ERROR(EnginePluginsLog,
                   "The project enables the plugin {0}, but it could not be loaded from {1}.",
                   name, directory);
      continue;
    }
    using LoadPluginFunction = void (*)();
    const auto loadPlugin = library.lock()->getSymbol<LoadPluginFunction>("loadPlugin");
    if (!loadPlugin) {
      CH_LOG_ERROR(EnginePluginsLog, "The plugin {0} has no loadPlugin function.", name);
      continue;
    }
    loadPlugin();
    loaded.push_back(name);
    ++count;
    CH_LOG_INFO(EnginePluginsLog, "Loaded the plugin {0}.", name);
  }
  return count;
}

/*
 */
const Vector<String>&
EnginePlugins::getLoaded()
{
  return getLoadedNames();
}

} // namespace chEngineSDK
