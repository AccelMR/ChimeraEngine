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
#include "chConsoleVariable.h"
#include "chFileSystem.h"
#include "chJobSystem.h"

#include "catch.hpp"

using namespace chEngineSDK;

namespace {
constexpr const ANSICHAR* kMount = "AsyncLoadTest";

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

  AssetManager::shutDown();
  JobSystem::shutDown();
  REQUIRE(FileSystem::unmount(kMount, root));
  FileSystem::removeAll(root);
}

#endif // USING(CH_EDITOR)
