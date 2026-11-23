/************************************************************************/
/**
 * @file chAsyncLoadTests.cpp
 * @author AccelMR
 * @date 2026/11/21
 * @brief Tests of loading assets in the background through the asset manager.
 */
/************************************************************************/
#include "chPrerequisitesCore.h"

#if USING(CH_EDITOR)

#include <thread>

#include "chAnimationGraph.h"
#include "chAnimationGraphAsset.h"
#include "chAnimationTestHelpers.h"
#include "chAssetManager.h"
#include "chAudioClipAsset.h"
#include "chAudioTestHelpers.h"
#include "chConsoleVariable.h"
#include "chFileSystem.h"
#include "chFontAsset.h"
#include "chGameObject.h"
#include "chJobSystem.h"
#include "chScene.h"
#include "chSceneAsset.h"
#include "chSceneManager.h"
#include "chTextTestHelpers.h"

#include "catch.hpp"

using namespace chEngineSDK;

namespace {
constexpr const ANSICHAR* kMount = "AsyncLoadTest";
constexpr const ANSICHAR* kLevelMount = "AsyncLevelTest";

AssetMetadata
makeMountMetadata(const chEngineSDK::UUID& type, const ANSICHAR* name)
{
  AssetMetadata metadata;
  metadata.uuid = chEngineSDK::UUID::createRandom();
  metadata.assetType = type;
  StringUtils::copyToBuffer(metadata.name, name);
  StringUtils::copyToBuffer(metadata.assetPath, String("/") + kMount);
  return metadata;
}

// Pumps the manager like the application does, until nothing is pending (or a few seconds
// passed). Gives how many updates it took and the most assets one update finished.
uint32
pumpUntilDone(AssetManager& manager, uint32& outMostPerUpdate)
{
  outMostPerUpdate = 0;
  uint32 updates = 0;
  for (uint32 i = 0; i < 5000 && manager.getPendingLoadCount() > 0; ++i) {
    const SIZE_T before = manager.getPendingLoadCount();
    manager.update();
    const SIZE_T finished = before - manager.getPendingLoadCount();
    outMostPerUpdate = Math::max(outMostPerUpdate, static_cast<uint32>(finished));
    ++updates;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return updates;
}
SPtr<SceneAsset>
saveTestScene(AssetManager& manager, const ANSICHAR* name, Span<const ANSICHAR* const> objects)
{
  AssetMetadata metadata;
  metadata.uuid = chEngineSDK::UUID::createRandom();
  metadata.assetType = AssetTypeTraits<SceneAsset>::getTypeId();
  StringUtils::copyToBuffer(metadata.name, name);
  StringUtils::copyToBuffer(metadata.assetPath, String("/") + kLevelMount);
  const auto asset = chMakeShared<SceneAsset>(metadata);
  const auto scene = chMakeShared<Scene>(name, metadata.uuid);
  for (const ANSICHAR* object : objects) {
    static_cast<void>(scene->createGameObject(object));
  }
  asset->setScene(scene);
  REQUIRE(manager.saveAsset(asset));
  return asset;
}

bool
hasRoot(const Scene& scene, StringView name)
{
  for (const SPtr<GameObject>& root : scene.getRootGameObjects()) {
    if (StringView(root->getName()) == name) {
      return true;
    }
  }
  return false;
}

// Runs frames like an application: the asset loads, then the scene update that applies the
// level requests. Stops when nothing waits (or after a few seconds).
void
pumpFrames(AssetManager& assets, SceneManager& scenes)
{
  for (uint32 i = 0; i < 5000; ++i) {
    assets.update();
    scenes.update(1.0f / 60.0f);
    if (assets.getPendingLoadCount() == 0 && !scenes.isMapOpenRequested()) {
      // One more update applies what finished loading in this one.
      scenes.update(1.0f / 60.0f);
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}
} // namespace

TEST_CASE("chCore - Asynchronous asset loads", "[Assets]")
{
  const Path root = FileSystem::absolutePath(Path("AsyncLoadTestDir"));
  FileSystem::removeAll(root);
  REQUIRE(FileSystem::createDirectories(root));
  REQUIRE(FileSystem::mount(kMount, root, 0, true));
  JobSystem::startUp(2);
  AssetManager::startUp();
  AssetManager& manager = AssetManager::instance();
  manager.initialize();

  // A graph that references two clips, saved like the editor saves them.
  const chEngineSDK::UUID clipType = AssetTypeTraits<AnimationClipAsset>::getTypeId();
  const auto walk = chMakeShared<AnimationClipAsset>(
      makeMountMetadata(clipType, "A_Walk"), createGraphTestClip(0.0f, 1.0f, 1.0f)->getClip());
  const auto run = chMakeShared<AnimationClipAsset>(
      makeMountMetadata(clipType, "A_Run"), createGraphTestClip(0.0f, 3.0f, 0.5f)->getClip());
  const SPtr<AnimationGraph> graph = chMakeShared<AnimationGraph>();
  static_cast<void>(graph->addClip(walk->getUUID(), walk));
  static_cast<void>(graph->addClip(run->getUUID(), run));
  const auto graphAsset = chMakeShared<AnimationGraphAsset>(
      makeMountMetadata(AssetTypeTraits<AnimationGraphAsset>::getTypeId(), "AG_Async"), graph);
  REQUIRE(manager.saveAsset(walk));
  REQUIRE(manager.saveAsset(run));
  REQUIRE(manager.saveAsset(graphAsset));

  // Registered from the files, unloaded, with their references read from the files.
  manager.lazyLoadAssetsFromDirectory(Path(String("/") + kMount));
  const SPtr<IAsset> graphFile = manager.getAsset(graphAsset->getUUID());
  const SPtr<IAsset> walkFile = manager.getAsset(walk->getUUID());
  const SPtr<IAsset> runFile = manager.getAsset(run->getUUID());
  REQUIRE(graphFile);
  REQUIRE(walkFile);
  REQUIRE(runFile);
  REQUIRE(graphFile->getReferencedAssets().size() == 2);

  auto unloadAll = [&] {
    REQUIRE(manager.unloadAsset(graphFile));
    REQUIRE(manager.unloadAsset(walkFile));
    REQUIRE(manager.unloadAsset(runFile));
  };

  // A request comes back at once; the references are requested too and finish first.
  REQUIRE(manager.requestLoad(graphFile));
  REQUIRE(manager.getPendingLoadCount() == 3);
  REQUIRE(graphFile->isLoading());
  REQUIRE(manager.isLoadPending(walk->getUUID()));
  REQUIRE(manager.requestLoad(graphFile));
  REQUIRE(manager.getPendingLoadCount() == 3);
  uint32 mostPerUpdate = 0;
  pumpUntilDone(manager, mostPerUpdate);
  REQUIRE(manager.getPendingLoadCount() == 0);
  REQUIRE(graphFile->isLoaded());
  REQUIRE(walkFile->isLoaded());
  REQUIRE(runFile->isLoaded());
  // The graph found its clips loaded when it was read.
  const SPtr<AnimationGraph>& loadedGraph =
      std::static_pointer_cast<AnimationGraphAsset>(graphFile)->getGraph();
  REQUIRE(loadedGraph);
  REQUIRE(loadedGraph->getClips().size() == 2);
  REQUIRE(loadedGraph->getClips()[0].clip == walkFile);
  REQUIRE(manager.requestLoad(graphFile));
  REQUIRE(manager.getPendingLoadCount() == 0);

  // A synchronous load of a pending asset finishes it and its references at once.
  unloadAll();
  REQUIRE(manager.requestLoad(graphFile));
  REQUIRE(manager.syncLoadAsset(graphFile));
  REQUIRE(manager.getPendingLoadCount() == 0);
  REQUIRE(walkFile->isLoaded());
  REQUIRE(graphFile->isLoaded());

  // With no time budget each update still finishes one asset.
  unloadAll();
  IConsoleVariable* budget = ConsoleVariables::find("Asset.LoadBudgetMs");
  REQUIRE(budget);
  const String oldBudget = budget->toString();
  REQUIRE(budget->setFromString("0", ConsoleVariableSource::Code));
  REQUIRE(manager.requestLoad(graphFile));
  const uint32 updates = pumpUntilDone(manager, mostPerUpdate);
  REQUIRE(mostPerUpdate == 1);
  REQUIRE(updates >= 3);
  REQUIRE(graphFile->isLoaded());
  REQUIRE(budget->setFromString(oldBudget, ConsoleVariableSource::Code));

  // A reference whose file is gone fails; what references it still loads.
  unloadAll();
  REQUIRE(FileSystem::removeFile(Path(String("/") + kMount + "/A_Run.chAss")));
  REQUIRE(manager.requestLoad(graphFile));
  pumpUntilDone(manager, mostPerUpdate);
  REQUIRE(runFile->isFailed());
  REQUIRE(graphFile->isLoaded());
  REQUIRE_FALSE(manager.requestLoad(runFile));

  // Sounds and fonts do their slow part (decoding, drawing glyphs) on the load's worker.
  const auto sound = chMakeShared<AudioClipAsset>(
      makeMountMetadata(AssetTypeTraits<AudioClipAsset>::getTypeId(), "S_Beep"),
      makeTestWav(2, kTestAudioRate, 4800, kTestAudioValue),
      AudioClipSettings{.loadMode = AudioClipLoadMode::Decompress});
  const auto font = chMakeShared<FontAsset>(
      makeMountMetadata(AssetTypeTraits<FontAsset>::getTypeId(), "F_Test"), readTestFont(),
      FontSettings{});
  REQUIRE(manager.saveAsset(sound));
  REQUIRE(manager.saveAsset(font));
  manager.lazyLoadAssetsFromDirectory(Path(String("/") + kMount));
  const SPtr<IAsset> soundFile = manager.getAsset(sound->getUUID());
  const SPtr<IAsset> fontFile = manager.getAsset(font->getUUID());
  REQUIRE(soundFile);
  REQUIRE(fontFile);
  REQUIRE(manager.requestLoad(soundFile));
  REQUIRE(manager.requestLoad(fontFile));
  pumpUntilDone(manager, mostPerUpdate);
  REQUIRE(soundFile->isLoaded());
  REQUIRE(fontFile->isLoaded());
  const SPtr<AudioClip>& clip = std::static_pointer_cast<AudioClipAsset>(soundFile)->getClip();
  REQUIRE(clip);
  REQUIRE(clip->getInfo().frameCount == 4800);
  const SPtr<Font>& loadedFont = std::static_pointer_cast<FontAsset>(fontFile)->getFont();
  REQUIRE(loadedFont);
  REQUIRE(loadedFont->getLineHeight() > 0.0f);

  // Levels and maps: a request waits for its scene's load, then its objects come in one
  // update. In this test because the managers cannot start twice in one program.
  const Path levelRoot = FileSystem::absolutePath(Path("AsyncLevelTestDir"));
  FileSystem::removeAll(levelRoot);
  REQUIRE(FileSystem::createDirectories(levelRoot));
  REQUIRE(FileSystem::mount(kLevelMount, levelRoot, 0, true));
  AssetManager& assets = manager;
  const ANSICHAR* const mainObjects[] = {"Player", "Floor"};
  const ANSICHAR* const ruinsObjects[] = {"Pillar", "Lintel"};
  const ANSICHAR* const arenaObjects[] = {"Wall"};
  const chEngineSDK::UUID mainId = saveTestScene(assets, "Main", mainObjects)->getUUID();
  const chEngineSDK::UUID ruinsId = saveTestScene(assets, "Ruins", ruinsObjects)->getUUID();
  const chEngineSDK::UUID arenaId = saveTestScene(assets, "Arena", arenaObjects)->getUUID();
  const ANSICHAR* const loadingObjects[] = {"Spinner"};
  const chEngineSDK::UUID loadingId =
      saveTestScene(assets, "Loading", loadingObjects)->getUUID();

  // The scenes as a game finds them: registered from their files, not loaded.
  assets.lazyLoadAssetsFromDirectory(Path(String("/") + kLevelMount));
  const SPtr<SceneAsset> main = std::static_pointer_cast<SceneAsset>(assets.getAsset(mainId));
  const SPtr<SceneAsset> ruins =
      std::static_pointer_cast<SceneAsset>(assets.getAsset(ruinsId));
  const SPtr<SceneAsset> arena =
      std::static_pointer_cast<SceneAsset>(assets.getAsset(arenaId));
  REQUIRE(main);
  REQUIRE(ruins);
  REQUIRE(arena);
  REQUIRE_FALSE(ruins->isLoaded());

  // A local one: the module cannot start twice in one program, and another test starts it.
  SceneManager scenes;
  const SPtr<Scene> world = scenes.openMap(main, true).lock();
  REQUIRE(world);
  REQUIRE(hasRoot(*world, "Player"));

  // A level waits for its load, then all its objects come in one update.
  int32 calls = 0;
  bool bLoadedResult = false;
  scenes.requestLoadLevel(ruins, [&calls, &bLoadedResult](bool bLoaded) {
    ++calls;
    bLoadedResult = bLoaded;
  });
  REQUIRE(scenes.isLevelLoadRequested(ruinsId));
  REQUIRE(scenes.getLoadProgress(ruinsId) < 1.0f);
  REQUIRE(ruins->isLoading());
  pumpFrames(assets, scenes);
  REQUIRE(calls == 1);
  REQUIRE(bLoadedResult);
  REQUIRE_FALSE(scenes.isLevelLoadRequested(ruinsId));
  REQUIRE(scenes.getLoadProgress(ruinsId) == 1.0f);
  REQUIRE(world->findLevel(ruinsId));
  REQUIRE(hasRoot(*world, "Pillar"));
  REQUIRE(hasRoot(*world, "Lintel"));
  // Loaded for the level only, its objects moved out and the asset let go.
  REQUIRE_FALSE(ruins->isLoaded());

  // An unload asked after a load waits for it, so both apply in order.
  REQUIRE(scenes.unloadLevel(*world, ruinsId));
  scenes.update(1.0f / 60.0f);
  REQUIRE_FALSE(hasRoot(*world, "Pillar"));
  scenes.requestLoadLevel(ruins);
  scenes.requestUnloadLevel(ruinsId);
  pumpFrames(assets, scenes);
  REQUIRE_FALSE(world->findLevel(ruinsId));
  REQUIRE_FALSE(hasRoot(*world, "Pillar"));

  // A map change waits for its load too and replaces the active scene.
  bool bMapOpened = false;
  scenes.requestOpenMap(arena, [&bMapOpened](bool bLoaded) { bMapOpened = bLoaded; });
  REQUIRE(scenes.isMapOpenRequested());
  REQUIRE(scenes.getMapLoadProgress() < 1.0f);
  pumpFrames(assets, scenes);
  REQUIRE(bMapOpened);
  REQUIRE_FALSE(scenes.isMapOpenRequested());
  const SPtr<Scene> arenaScene = scenes.getActiveScene().lock();
  REQUIRE(arenaScene);
  REQUIRE(hasRoot(*arenaScene, "Wall"));
  REQUIRE_FALSE(hasRoot(*arenaScene, "Player"));

  // A loading scene replaces the old map once it is loaded and runs until the map is in and
  // its least time passed.
  const SPtr<SceneAsset> loadingScene =
      std::static_pointer_cast<SceneAsset>(assets.getAsset(loadingId));
  REQUIRE(loadingScene);
  bMapOpened = false;
  scenes.requestOpenMap(main, [&bMapOpened](bool bLoaded) { bMapOpened = bLoaded; },
                        {.loadingScene = loadingScene, .minLoadingTime = 0.1f});
  REQUIRE(scenes.getRequestedMap() == mainId);
  REQUIRE_FALSE(scenes.isLoadingScreenShown());
  uint32 shownUpdates = 0;
  for (uint32 i = 0; i < 5000 && scenes.isMapOpenRequested(); ++i) {
    assets.update();
    scenes.update(1.0f / 60.0f);
    if (scenes.isLoadingScreenShown()) {
      ++shownUpdates;
      const SPtr<Scene> shown = scenes.getActiveScene().lock();
      REQUIRE(shown);
      REQUIRE(hasRoot(*shown, "Spinner"));
      REQUIRE_FALSE(hasRoot(*shown, "Wall"));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  REQUIRE(bMapOpened);
  REQUIRE(shownUpdates >= 5);
  REQUIRE(scenes.getRequestedMap().isNull());
  const SPtr<Scene> mainAgain = scenes.getActiveScene().lock();
  REQUIRE(mainAgain);
  REQUIRE(hasRoot(*mainAgain, "Player"));
  REQUIRE_FALSE(hasRoot(*mainAgain, "Spinner"));
  // Shown as a copy, so it stays loaded for the next map change.
  REQUIRE(loadingScene->isLoaded());

  // A map already loaded opens at once, without the loading scene.
  REQUIRE(assets.syncLoadAsset(arena));
  bMapOpened = false;
  scenes.requestOpenMap(arena, [&bMapOpened](bool bLoaded) { bMapOpened = bLoaded; },
                        {.loadingScene = loadingScene});
  scenes.update(1.0f / 60.0f);
  REQUIRE(bMapOpened);
  REQUIRE_FALSE(scenes.isLoadingScreenShown());
  REQUIRE(hasRoot(*scenes.getActiveScene().lock(), "Wall"));

  // Requests made by the callback of a map change are for the new map and stay.
  bool bLevelLoaded = false;
  scenes.requestOpenMap(main, [&scenes, &ruins, &bLevelLoaded](bool) {
    scenes.requestLoadLevel(ruins, [&bLevelLoaded](bool bLoaded) { bLevelLoaded = bLoaded; });
  });
  pumpFrames(assets, scenes);
  pumpFrames(assets, scenes);
  REQUIRE(bLevelLoaded);
  REQUIRE(hasRoot(*scenes.getActiveScene().lock(), "Pillar"));

  REQUIRE(FileSystem::unmount(kLevelMount, levelRoot));
  FileSystem::removeAll(levelRoot);

  AssetManager::shutDown();
  JobSystem::shutDown();
  REQUIRE(FileSystem::unmount(kMount, root));
  FileSystem::removeAll(root);
}

#endif // USING(CH_EDITOR)
