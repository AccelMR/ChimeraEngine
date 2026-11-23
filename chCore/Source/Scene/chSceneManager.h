/************************************************************************/
/**
 * @file chSceneManager.h
 * @author AccelMR
 * @date 2025/04/20
 * @brief
 *  Scene Manager module for managing scene graph and game objects.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"
#include "chModule.h"

#include "chLogger.h"
#include "chScene.h"
#include "chUUID.h"

CH_LOG_DECLARE_EXTERN(CH_CORE_EXPORT, SceneManagerLog);

namespace chEngineSDK {

/**
 * How SceneManager::requestOpenMap shows the wait for the map.
 */
struct MapOpenOptions
{
  // Scene shown while the map loads; null takes Game.LoadingScene.
  SPtr<SceneAsset> loadingScene{};
  // Keeps the current map until the new one is in, even when Game.LoadingScene is set.
  bool bNoLoadingScene = false;
  // Least seconds the loading scene stays; negative takes Game.LoadingMinTime.
  float minLoadingTime = -1.0f;
};

class CH_CORE_EXPORT SceneManager : public Module<SceneManager>
{
 public:
  /**
   * Default constructor
   */
  SceneManager() = default;

  /**
   * Destructor
   */
  ~SceneManager() = default;

#if USING (CH_EDITOR)
  /**
   * Create a new empty scene
   *
   * @param name Name of the scene
   * @return Shared pointer to the created scene
   */
  WeakPtr<Scene>
  createAndLoadScene(const String& name);
#endif // USING (CH_EDITOR)

  /**
   * Load a scene from file
   *
   * @param uuid UUID of the scene asset to load
   * @return Shared pointer to the loaded scene, or nullptr if loading failed
   */
  WeakPtr<Scene>
  loadScene(WeakPtr<SceneAsset> asset);

  /**
   * Keeps a scene made elsewhere (the copy Play in the editor runs), so it can be made
   * active. False when a scene with its UUID is already kept.
   */
  NODISCARD bool
  addScene(const SPtr<Scene>& scene);

  /**
   * Forgets the scene; it is destroyed once nothing else holds it. Unloading the active
   * scene leaves no active scene.
   */
  void
  unloadScene(const Scene& scene);

  NODISCARD bool
  setActiveScene(WeakPtr<Scene> scene);

  /**
   * Get the currently active scene
   *
   * @return Shared pointer to the active scene
   */
  NODISCARD WeakPtr<Scene>
  getActiveScene() const {
    return m_activeScene;
  }

  /**
   * Get a scene by name
   *
   * @param UUID unique id of the scene you want
   * @return Shared pointer to the scene, or nullptr if not found
   */
  NODISCARD WeakPtr<Scene>
  getScene(const UUID& uuid) const;

  /**
   * Runs the fixed step of the active scene.
   */
  void
  fixedUpdate(float deltaTime);

  /**
   * Runs the per frame update of the active scene, then applies the level requests made
   * meanwhile.
   */
  void
  update(float deltaTime);

  /**
   * Makes a new scene holding the objects of asset as its map, plus the sublevels its list
   * loads at start, and makes it the active one; the scene it replaces is forgotten. A scene
   * asset already loaded (open in the editor) is copied and stays loaded; otherwise it is
   * unloaded once its objects moved. Never during an update. Null when asset cannot load.
   * Requests still waiting are dropped (called back with false).
   */
  WeakPtr<Scene>
  openMap(const SPtr<SceneAsset>& asset, bool bPlaying);

  /**
   * Adds the objects of asset to scene as a new level, after the others. Never during an
   * update of scene. False when it cannot load or is already a level of scene.
   */
  bool
  loadLevel(Scene& scene, const SPtr<SceneAsset>& asset);

  /**
   * Destroys the objects of the level asset loaded. Never during an update of scene. False
   * when it is not a sublevel of scene.
   */
  bool
  unloadLevel(Scene& scene, const UUID& asset);

  /**
   * Loads the sublevels the map of scene lists to load at start.
   */
  void
  loadStartLevels(Scene& scene);

  /**
   * Called when a requested load ends: true when the level or map is in, false when it could
   * not be loaded or a map change dropped the request.
   */
  using LevelLoadCallback = Function<void(bool bLoaded)>;

  /**
   * Replaces the active scene with asset's map once it and the sublevels it loads at start
   * are loaded in the background (AssetManager::requestLoad), at the end of an update; the
   * new one plays when the old one did. Requests made after it are dropped then.
   * With a loading scene, the old map goes as soon as that scene is loaded and a copy of it
   * is the active scene until the map is in (and at least the minimum time passed); an
   * active scene that already is that loading scene stays as it is.
   */
  void
  requestOpenMap(const SPtr<SceneAsset>& asset,
                 LevelLoadCallback onDone = {},
                 const MapOpenOptions& options = {});

  /**
   * The scene Game.LoadingScene names; null when it is empty or names no scene.
   */
  NODISCARD static SPtr<SceneAsset>
  getDefaultLoadingScene();

  /**
   * Loads asset in the background and adds it as a level of the active scene at the end of
   * the first update after it is loaded; its objects all appear in that update.
   */
  void
  requestLoadLevel(const SPtr<SceneAsset>& asset, LevelLoadCallback onDone = {});

  /**
   * Applied in order with the loads: after a load requested before it.
   */
  void
  requestUnloadLevel(const UUID& asset);

  /**
   * True from requestLoadLevel until the level is in (or failed).
   */
  NODISCARD bool
  isLevelLoadRequested(const UUID& asset) const;

  /**
   * How much of a requested load of asset (a level or a map) is loaded, 0 to 1 by assets;
   * 1 when none waits.
   */
  NODISCARD float
  getLoadProgress(const UUID& asset) const;

  /**
   * getLoadProgress of the map waiting to open; 1 when none waits.
   */
  NODISCARD float
  getMapLoadProgress() const;

  NODISCARD bool
  isMapOpenRequested() const;

  /**
   * The map a requestOpenMap waits for; null when none waits.
   */
  NODISCARD UUID
  getRequestedMap() const;

  /**
   * True while a loading scene is the active scene, waiting for a map.
   */
  NODISCARD bool
  isLoadingScreenShown() const;

 private:
  enum class LevelRequestType
  {
    OpenMap,
    Load,
    Unload
  };

  struct LevelRequest
  {
    LevelRequestType type = LevelRequestType::Load;
    UUID asset = UUID::null();
    // Every asset the request waits for: the scene, what it references and, for a map, the
    // sublevels it loads at start with theirs.
    Vector<UUID> assets{};
    // Scene assets this request loaded; their objects are moved out and they are unloaded.
    // A scene that was already loaded (open in the editor) is copied instead.
    Vector<UUID> takeObjectsFrom{};
    LevelLoadCallback onDone{};
    // Scene shown while a map loads; null for none, or once it turned out missing.
    UUID loadingScene = UUID::null();
    float minLoadingTime = 0.0f;
    // Seconds the loading scene has been shown.
    float loadingTime = 0.0f;
    bool bLoadingScreenShown = false;
    // A map lists its start sublevels in its own data, so they join once it is loaded.
    bool bStartLevelsAdded = false;
  };

  void
  applyLevelRequests(float deltaTime);

  /**
   * Makes a copy of the loading scene of the request at index the active scene, dropping
   * the old map. False while that scene still loads.
   */
  NODISCARD bool
  showLoadingScreen(SIZE_T index);

  /**
   * Calls every waiting request back with false and forgets them.
   */
  void
  dropLevelRequests();

  NODISCARD bool
  isRequestReady(const LevelRequest& request) const;

  void
  addLoadAssets(const SPtr<SceneAsset>& asset, LevelRequest& request);

  void
  addStartLevels(LevelRequest& request);

  NODISCARD const LevelRequest*
  findLoadRequest(const UUID& asset) const;

  NODISCARD static float
  getRequestProgress(const LevelRequest& request);

  WeakPtr<Scene>
  openMapTaking(const SPtr<SceneAsset>& asset, bool bPlaying, const Vector<UUID>* takeFrom);

  bool
  loadLevelTaking(Scene& scene, const SPtr<SceneAsset>& asset, bool bTakeObjects);

  void
  loadStartLevelsTaking(Scene& scene, const Vector<UUID>* takeFrom);

  /**
   * Puts the objects of asset into scene as objects of level: moved out of the asset, which
   * is unloaded, with bTakeObjects (or when it is not loaded yet), else copied. outSubLevels,
   * when given, gets the asset's list of sublevels.
   */
  NODISCARD bool
  readLevelObjects(const SPtr<SceneAsset>& asset,
                   Scene& scene,
                   SceneLevelId level,
                   Vector<SceneSubLevel>* outSubLevels,
                   bool bTakeObjects);

  UnorderedMap<UUID, SPtr<Scene>> m_loadedScenes;
  WeakPtr<Scene> m_activeScene;
  Vector<LevelRequest> m_levelRequests;
};

} // namespace chEngineSDK
