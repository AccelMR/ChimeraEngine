/************************************************************************/
/**
 * @file chEngineConsoleCommands.cpp
 * @author AccelMR
 * @date 2026/11/23
 * @brief Console commands that drive the engine: quitting, maps, levels and scripts.
 */
/************************************************************************/
#include "chPrerequisitesCore.h"

#if USING(CH_CONSOLE)

#include "chAssetManager.h"
#include "chBaseApplication.h"
#include "chConsole.h"
#include "chScene.h"
#include "chSceneAsset.h"
#include "chSceneManager.h"
#include "chScriptManager.h"
#include "chStringUtils.h"

namespace chEngineSDK {
namespace {

/**
 * The scene a command names by path or name; logs and gives null when there is none.
 */
SPtr<SceneAsset>
findScene(StringView pathOrName)
{
  if (pathOrName.empty()) {
    CH_LOG_WARNING(ConsoleLog, "Name a scene: a path such as /Game/Maps/Main, or its name.");
    return nullptr;
  }
  if (!AssetManager::isStarted()) {
    CH_LOG_WARNING(ConsoleLog, "There are no assets to search.");
    return nullptr;
  }
  SPtr<SceneAsset> scene = AssetManager::instance().findScene(pathOrName);
  if (!scene) {
    CH_LOG_WARNING(ConsoleLog, "There is no scene {0}.", pathOrName);
  }
  return scene;
}

NODISCARD bool
checkSceneManager()
{
  if (!SceneManager::isStarted()) {
    CH_LOG_WARNING(ConsoleLog, "Scenes are not available in this program.");
    return false;
  }
  return true;
}

void
runQuit(const ConsoleArguments& arguments)
{
  CH_PARAMETER_UNUSED(arguments);
  if (BaseApplication::isStarted()) {
    BaseApplication::instance().requestExit("console");
  }
}

void
runOpenMap(const ConsoleArguments& arguments)
{
  const SPtr<SceneAsset> map = findScene(arguments.get(0));
  if (!map || !checkSceneManager()) {
    return;
  }
  MapOpenOptions options;
  const StringView loading = arguments.get(1);
  if (StringUtils::equalsIgnoreCase(loading, "none")) {
    options.bNoLoadingScene = true;
  }
  else if (!loading.empty()) {
    options.loadingScene = findScene(loading);
    if (!options.loadingScene) {
      return;
    }
  }
  const String name = map->getName();
  SceneManager::instance().requestOpenMap(
      map,
      [name](const bool bLoaded) {
        if (bLoaded) {
          CH_LOG_INFO(ConsoleLog, "Opened the map {0}.", name);
        }
        else {
          CH_LOG_WARNING(ConsoleLog, "The map {0} did not open.", name);
        }
      },
      options);
}

void
runLoadLevel(const ConsoleArguments& arguments)
{
  const SPtr<SceneAsset> level = findScene(arguments.get(0));
  if (!level || !checkSceneManager()) {
    return;
  }
  const String name = level->getName();
  SceneManager::instance().requestLoadLevel(level, [name](const bool bLoaded) {
    if (bLoaded) {
      CH_LOG_INFO(ConsoleLog, "Loaded the level {0}.", name);
    }
    else {
      CH_LOG_WARNING(ConsoleLog, "The level {0} did not load.", name);
    }
  });
}

void
runUnloadLevel(const ConsoleArguments& arguments)
{
  const SPtr<SceneAsset> level = findScene(arguments.get(0));
  if (!level || !checkSceneManager()) {
    return;
  }
  SceneManager::instance().requestUnloadLevel(level->getUUID());
}

void
runLua(const ConsoleArguments& arguments)
{
  if (arguments.text.empty()) {
    CH_LOG_WARNING(ConsoleLog,
                   "lua needs code to run, such as lua print(Scene.getMapName()).");
    return;
  }
  if (!ScriptManager::isStarted()) {
    CH_LOG_WARNING(ConsoleLog, "Scripts are not available in this program.");
    return;
  }
  ScriptManager& scripts = ScriptManager::instance();
  // As a script of the active scene, so Scene.find and the like work.
  Scene* const previousScene = scripts.getCurrentScene();
  const SPtr<Scene> active =
      SceneManager::isStarted() ? SceneManager::instance().getActiveScene().lock() : nullptr;
  scripts.setCurrentScene(active.get());
  // Errors are logged by the script manager.
  static_cast<void>(scripts.runString(arguments.text, "=console"));
  scripts.setCurrentScene(previousScene);
}

ConsoleCommand g_commandQuit("quit",
                             "",
                             "Closes the program; the editor asks about unsaved work first.",
                             &runQuit);
ConsoleCommand g_commandOpenMap("openMap",
                                "<map> [loadingScene|none]",
                                "Opens a map in the background, behind a loading scene when "
                                "one is given or set in Game.LoadingScene.",
                                &runOpenMap);
ConsoleCommand g_commandLoadLevel("loadLevel",
                                  "<level>",
                                  "Loads a scene as a sublevel of the active map.",
                                  &runLoadLevel);
ConsoleCommand g_commandUnloadLevel("unloadLevel",
                                    "<level>",
                                    "Unloads a sublevel of the active map.",
                                    &runUnloadLevel);
ConsoleCommand g_commandLua("lua",
                            "<code>",
                            "Runs Lua code as a script of the active scene.",
                            &runLua);

} // namespace
} // namespace chEngineSDK

#endif // USING(CH_CONSOLE)
