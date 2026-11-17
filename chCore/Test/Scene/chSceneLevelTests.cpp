/************************************************************************/
/**
 * @file chSceneLevelTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of maps and sublevels.
 */
/************************************************************************/
#include "chCamera.h"
#include "chCameraComponent.h"
#include "chGameObjectSerializer.h"
#include "chLightComponent.h"
#include "chScene.h"
#include "chSceneHistory.h"
#include "chScriptManager.h"
#include "chScriptMathTypes.h"
#include "chScriptObjectTypes.h"
#include "chSkyLightComponent.h"
#include "chUUID.h"
#include "chVector3.h"

#include "catch.hpp"
#include "chSceneTestHelpers.h"
#include "chScriptTestHelpers.h"

using namespace chEngineSDK;

TEST_CASE("chCore - Scene levels", "[Scene]")
{
  Scene world("World", chEngineSDK::UUID::createRandom());
  const chEngineSDK::UUID mapAsset = chEngineSDK::UUID::createRandom();
  world.setMapLevel("Map", mapAsset);
  world.setPlaying(true);
  const SPtr<GameObject> player = world.createGameObject("Player");
  REQUIRE(world.getLevels().size() == 1);
  REQUIRE(player->getLevel() == Scene::MAP_LEVEL);
  REQUIRE(world.findLevel(mapAsset)->id == Scene::MAP_LEVEL);

  // A sublevel read into a scene of its own, as its asset makes it.
  Scene caveSource("Cave", chEngineSDK::UUID::createRandom());
  const SPtr<GameObject> rock = caveSource.createGameObject("Rock");
  rock->getTransform().setLocalPosition(Vector3(5.0f, 0.0f, 0.0f));
  const SPtr<GameObject> pebble = caveSource.createGameObject("Pebble", rock.get());
  TestCounterComponent& counter = pebble->addComponent<TestCounterComponent>();
  const chEngineSDK::UUID caveAsset = chEngineSDK::UUID::createRandom();
  const SceneLevelId cave = world.addLevel("Cave", caveAsset);
  world.moveRootObjectsFrom(caveSource, cave);

  SECTION("Moved objects belong to the level and update in the scene")
  {
    REQUIRE(caveSource.getRootGameObjects().empty());
    REQUIRE(rock->getScene() == &world);
    REQUIRE(pebble->getScene() == &world);
    REQUIRE(pebble->getLevel() == cave);
    REQUIRE(world.findLevel(caveAsset)->name == "Cave");
    REQUIRE(world.getLevelRank(cave) == 1);
    world.update(0.1f);
    REQUIRE(counter.updates == 1);
    world.updateTransforms();
    REQUIRE(pebble->getTransform().getWorldPosition().nearEqual(Vector3(5.0f, 0.0f, 0.0f)));
  }

  SECTION("Removing a level destroys only its objects")
  {
    REQUIRE(world.removeLevel(cave));
    REQUIRE(rock->getScene() == nullptr);
    REQUIRE(pebble->getScene() == nullptr);
    REQUIRE(world.getRootGameObjects().size() == 1);
    REQUIRE(world.findLevel(caveAsset) == nullptr);
    REQUIRE(world.getLevelRank(cave) == Scene::MAX_LEVEL_RANK);
    REQUIRE_FALSE(world.removeLevel(Scene::MAP_LEVEL));
    REQUIRE_FALSE(world.removeLevel(cave));
  }

  SECTION("Objects move between levels")
  {
    REQUIRE(world.moveToLevel(*player, cave));
    REQUIRE(player->getLevel() == cave);

    // A child of a root of another level becomes a root, keeping where it stands.
    world.updateTransforms();
    REQUIRE(world.moveToLevel(*pebble, Scene::MAP_LEVEL));
    REQUIRE(pebble->getParent() == nullptr);
    REQUIRE(pebble->getLevel() == Scene::MAP_LEVEL);
    world.updateTransforms();
    REQUIRE(pebble->getTransform().getWorldPosition().nearEqual(Vector3(5.0f, 0.0f, 0.0f)));

    // Taken out of its parent, an object keeps its root's level.
    const SPtr<GameObject> torch = world.createGameObject("Torch", rock.get());
    world.updateTransforms();
    REQUIRE(world.setParent(*torch, nullptr));
    REQUIRE(torch->getLevel() == cave);

    REQUIRE(world.removeLevel(cave));
    REQUIRE(player->getScene() == nullptr);
    REQUIRE(torch->getScene() == nullptr);
    REQUIRE(pebble->getScene() == &world);
    REQUIRE_FALSE(world.moveToLevel(*pebble, cave));
  }

  SECTION("Ids are reused, never one still in use")
  {
    // More loads than ids: the counter wraps and must step over the cave and the map.
    for (uint32 i = 0; i < 70000; ++i) {
      const SceneLevelId id = world.addLevel("Passing", chEngineSDK::UUID::createRandom());
      REQUIRE(id != cave);
      REQUIRE(id != Scene::MAP_LEVEL);
      REQUIRE(world.removeLevel(id));
    }
    REQUIRE(world.getLevels().size() == 2);
    REQUIRE(pebble->getLevel() == cave);
    REQUIRE(world.findLevelById(cave)->name == "Cave");
  }

  SECTION("The first level's sky light and camera win")
  {
    const SPtr<GameObject> caveSky = world.createGameObject("Cave Sky", rock.get());
    caveSky->addComponent<SkyLightComponent>();
    REQUIRE(world.getActiveSkyLight()->owner == caveSky.get());
    const SPtr<GameObject> mapSky = world.createGameObject("Map Sky");
    mapSky->addComponent<SkyLightComponent>();
    REQUIRE(world.getActiveSkyLight()->owner == mapSky.get());
    // Registered after the map's, the cave's still loses.
    caveSky->getComponent<SkyLightComponent>()->setEnabled(false);
    caveSky->getComponent<SkyLightComponent>()->setEnabled(true);
    REQUIRE(world.getActiveSkyLight()->owner == mapSky.get());

    const SPtr<GameObject> mapCamera = world.createGameObject("Map Camera");
    mapCamera->addComponent<CameraComponent>();
    const SPtr<GameObject> caveCamera = world.createGameObject("Cave Camera", rock.get());
    CameraComponent& caveCameraComponent = caveCamera->addComponent<CameraComponent>();
    REQUIRE(world.getActiveCamera()->getOwner() == mapCamera.get());
    caveCameraComponent.setPriority(1);
    REQUIRE(world.getActiveCamera()->getOwner() == caveCamera.get());
  }

  SECTION("Scripts see the levels")
  {
    ScriptManager& scripts = getTestScriptManager();
    scripts.setCurrentScene(&world);
    REQUIRE(scripts.runString(R"lua(
      assert(Scene.getMapName() == "Map")
      local levels = Scene.getLevels()
      assert(#levels == 2 and levels[1] == "Map" and levels[2] == "Cave")
      assert(Scene.find("Pebble"):getLevel() == "Cave")
      assert(Scene.find("Player"):getLevel() == "Map")
    )lua"));
    // Without an AssetManager no scene can be named.
    REQUIRE_FALSE(scripts.runString("Scene.loadLevel('Cave')"));
    scripts.setCurrentScene(nullptr);
  }

  SECTION("The list of sublevels is copied and undone")
  {
    const chEngineSDK::UUID otherAsset = chEngineSDK::UUID::createRandom();
    world.setSubLevels({{.asset = caveAsset, .bLoadAtStart = true}});

    Scene copy("Copy", chEngineSDK::UUID::createRandom());
    REQUIRE(GameObjectSerializer::copyScene(world, copy));
    REQUIRE(copy.getSubLevels() == world.getSubLevels());
    REQUIRE(copy.getLevels()[0].asset == mapAsset);
    REQUIRE(copy.getLevels()[0].name == "Map");

    SceneHistory history;
    history.recordSubLevels(world, "Add Level");
    world.setSubLevels({{.asset = caveAsset, .bLoadAtStart = true},
                        {.asset = otherAsset, .bLoadAtStart = false}});
    history.commit(world);
    REQUIRE(history.canUndo());
    REQUIRE(history.getUndoName() == "Add Level");
    chEngineSDK::UUID focus = chEngineSDK::UUID::createRandom();
    REQUIRE(history.undo(world, focus));
    REQUIRE(focus.isNull());
    REQUIRE(world.getSubLevels().size() == 1);
    REQUIRE(history.redo(world, focus));
    REQUIRE(world.getSubLevels().size() == 2);
    REQUIRE_FALSE(world.getSubLevels()[1].bLoadAtStart);

    // A list set back to how it was leaves no step.
    history.recordSubLevels(world, "Remove Level");
    const Vector<SceneSubLevel> before = world.getSubLevels();
    world.setSubLevels({});
    world.setSubLevels(before);
    history.commit(world);
    REQUIRE(history.getUndoName() == "Add Level");
  }
}
