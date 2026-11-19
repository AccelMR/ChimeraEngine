/************************************************************************/
/**
 * @file chScriptObjectTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of scene objects in scripts and spawning.
 */
/************************************************************************/
#include <chrono>

#include "chAnimationGraph.h"
#include "chAnimatorComponent.h"
#include "chCamera.h"
#include "chCameraComponent.h"
#include "chDebugDraw.h"
#include "chFileStream.h"
#include "chGameObjectSerializer.h"
#include "chLightComponent.h"
#include "chModel.h"
#include "chModelAsset.h"
#include "chModelComponent.h"
#include "chQuaternion.h"
#include "chRay.h"
#include "chRotator.h"
#include "chScene.h"
#include "chScriptAsset.h"
#include "chScriptBinder.h"
#include "chScriptComponent.h"
#include "chScriptManager.h"
#include "chScriptMathTypes.h"
#include "chScriptObjectTypes.h"
#include "chShapeComponent.h"
#include "chSphere.h"
#include "chUUID.h"
#include "chVector3.h"

#include "catch.hpp"
#include "chAnimationTestHelpers.h"
#include "chScriptTestHelpers.h"
#include "chModelTestHelpers.h"

using namespace chEngineSDK;

CH_LOG_DECLARE_STATIC(ScriptTestLog, All);

/*
 * Not run by default (ctest skips hidden tags); run it in Release:
 *   chCoreTest "chCore - Spawn benchmark"
 * Spawns 100 bullets per frame that live 60 frames, so 6000 are alive once it runs steady:
 * copied and destroyed from C++, the same from Lua, and objects reused by turning their
 * components off and on (the least a pool could cost).
 */
TEST_CASE("chCore - Spawn benchmark", "[Scripting][.benchmark]")
{
  constexpr uint32 kSpawnsPerFrame = 100;
  constexpr uint32 kLifetimeFrames = 60;
  constexpr uint32 kAlive = kSpawnsPerFrame * kLifetimeFrames;
  constexpr uint32 kWarmUpFrames = kLifetimeFrames + 30;
  constexpr uint32 kFrames = 300;
  constexpr float kDeltaTime = 1.0f / 60.0f;
  using Clock = std::chrono::steady_clock;
  const auto toMilliseconds = [](Clock::duration duration) {
    return std::chrono::duration<double, std::milli>(duration).count() / kFrames;
  };

  ScriptManager& scripts = getTestScriptManager();
  Scene scene("SpawnBenchmark", chEngineSDK::UUID::createRandom());
  scene.setPlaying(true);
  scripts.setCurrentScene(&scene);

  // A sphere with a light and a box child, like a bullet with its trail.
  const SPtr<GameObject> bullet = scene.createGameObject("Bullet");
  bullet->addComponent<ShapeComponent>(
      ShapeDesc{.type = ShapeType::Sphere, .radius = 0.1f, .segments = 8, .rings = 6});
  bullet->addComponent<PointLightComponent>();
  const SPtr<GameObject> trail = scene.createGameObject("Trail", bullet.get());
  trail->addComponent<ShapeComponent>(ShapeDesc{.size = Vector3(0.5f, 0.05f, 0.05f)});
  scene.updateTransforms();

  const auto setComponentsEnabled = [](GameObject& gameObject, bool bEnabled) {
    for (const UniquePtr<Component>& component : gameObject.getComponents()) {
      component->setEnabled(bEnabled);
    }
    for (const SPtr<GameObject>& child : gameObject.getChildren()) {
      for (const UniquePtr<Component>& component : child->getComponents()) {
        component->setEnabled(bEnabled);
      }
    }
  };

  SECTION("Copy and destroy from C++")
  {
    Vector<SPtr<GameObject>> ring(kAlive);
    uint32 head = 0;
    Clock::duration spawnTime{};
    Clock::duration updateTime{};
    for (uint32 frame = 0; frame < kWarmUpFrames + kFrames; ++frame) {
      if (frame == kWarmUpFrames) {
        spawnTime = updateTime = Clock::duration{};
      }
      const auto start = Clock::now();
      for (uint32 spawn = 0; spawn < kSpawnsPerFrame; ++spawn) {
        SPtr<GameObject>& slot = ring[head++ % kAlive];
        if (slot) {
          scene.requestDestroy(*slot);
        }
        slot = GameObjectSerializer::copyTo(*bullet, nullptr);
        slot->getTransform().setLocalPosition(
            Vector3(static_cast<float>(spawn), static_cast<float>(frame % 100), 0.0f));
      }
      const auto spawned = Clock::now();
      scene.update(kDeltaTime);
      scene.updateTransforms();
      spawnTime += spawned - start;
      updateTime += Clock::now() - spawned;
    }
    REQUIRE(scene.getRootGameObjects().size() == kAlive + 1);
    CH_LOG_INFO(ScriptTestLog,
                "C++ copy: {:.3f} ms spawning, {:.3f} ms update (destroys, transforms) per frame",
                toMilliseconds(spawnTime), toMilliseconds(updateTime));
  }

  SECTION("Clone and destroy from Lua")
  {
    REQUIRE(scripts.runString(R"lua(
      Bench = { ring = {}, head = 0, template = Scene.find("Bullet"), position = Vector3() }
      function Bench.step(frame, spawns, alive)
        local ring, template, position = Bench.ring, Bench.template, Bench.position
        for spawn = 1, spawns do
          local index = Bench.head % alive + 1
          Bench.head = Bench.head + 1
          local old = ring[index]
          if old then old:destroy() end
          position:set(spawn, frame % 100, 0)
          ring[index] = template:clone(nil, position)
        end
      end
    )lua"));
    lua_State* const state = scripts.getState();
    uint64 allocationsBefore = 0;
    int32 kilobytesBefore = 0;
    Clock::duration spawnTime{};
    Clock::duration updateTime{};
    for (uint32 frame = 0; frame < kWarmUpFrames + kFrames; ++frame) {
      if (frame == kWarmUpFrames) {
        spawnTime = updateTime = Clock::duration{};
        scripts.collectGarbage();
        allocationsBefore = scripts.getAllocationCount();
        kilobytesBefore = lua_gc(state, LUA_GCCOUNT, 0);
      }
      const auto start = Clock::now();
      lua_getglobal(state, "Bench");
      lua_getfield(state, -1, "step");
      lua_pushinteger(state, frame);
      lua_pushinteger(state, kSpawnsPerFrame);
      lua_pushinteger(state, kAlive);
      lua_call(state, 3, 0);
      lua_pop(state, 1);
      const auto spawned = Clock::now();
      scene.update(kDeltaTime);
      scene.updateTransforms();
      spawnTime += spawned - start;
      updateTime += Clock::now() - spawned;
    }
    scripts.collectGarbage();
    const int32 kilobytesAfter = lua_gc(state, LUA_GCCOUNT, 0);
    REQUIRE(scene.getRootGameObjects().size() == kAlive + 1);
    CH_LOG_INFO(ScriptTestLog,
                "Lua clone: {:.3f} ms spawning, {:.3f} ms update per frame, {:.0f} Lua "
                "allocations per frame, Lua memory {} KB -> {} KB",
                toMilliseconds(spawnTime), toMilliseconds(updateTime),
                static_cast<double>(scripts.getAllocationCount() - allocationsBefore) / kFrames,
                kilobytesBefore, kilobytesAfter);
    REQUIRE(scripts.runString("Bench = nil"));
    scripts.collectGarbage();
  }

  SECTION("Reuse objects by turning their components off and on")
  {
    Vector<GameObject*> freeObjects;
    freeObjects.reserve(kAlive + kSpawnsPerFrame);
    for (uint32 i = 0; i < kAlive + kSpawnsPerFrame; ++i) {
      GameObject& copy = *GameObjectSerializer::copyTo(*bullet, nullptr);
      setComponentsEnabled(copy, false);
      freeObjects.push_back(&copy);
    }
    Vector<GameObject*> ring(kAlive, nullptr);
    uint32 head = 0;
    Clock::duration spawnTime{};
    Clock::duration updateTime{};
    for (uint32 frame = 0; frame < kWarmUpFrames + kFrames; ++frame) {
      if (frame == kWarmUpFrames) {
        spawnTime = updateTime = Clock::duration{};
      }
      const auto start = Clock::now();
      for (uint32 spawn = 0; spawn < kSpawnsPerFrame; ++spawn) {
        GameObject*& slot = ring[head++ % kAlive];
        if (slot) {
          setComponentsEnabled(*slot, false);
          freeObjects.push_back(slot);
        }
        slot = freeObjects.back();
        freeObjects.pop_back();
        setComponentsEnabled(*slot, true);
        slot->getTransform().setLocalPosition(
            Vector3(static_cast<float>(spawn), static_cast<float>(frame % 100), 0.0f));
      }
      const auto spawned = Clock::now();
      scene.update(kDeltaTime);
      scene.updateTransforms();
      spawnTime += spawned - start;
      updateTime += Clock::now() - spawned;
    }
    REQUIRE(scene.getRootGameObjects().size() == kAlive + kSpawnsPerFrame + 1);
    CH_LOG_INFO(ScriptTestLog, "Reuse: {:.3f} ms spawning, {:.3f} ms update per frame",
                toMilliseconds(spawnTime), toMilliseconds(updateTime));
  }

  scripts.setCurrentScene(nullptr);
}

TEST_CASE("chCore - Script objects", "[Scripting]")
{
  ScriptManager& scripts = getTestScriptManager();
  REQUIRE(scripts.runString("Found = {}"));
  const SPtr<ScriptAsset> probe = createTestScriptAsset("Probe", R"lua(
    local Probe = {}
    function Probe:onStart()
      Found.self = self
      Found.object = self.gameObject
      Found.transform = self.transform
    end
    return Probe
  )lua");

  Scene scene("Objects", chEngineSDK::UUID::createRandom());
  scene.setPlaying(true);
  const SPtr<GameObject> parent = scene.createGameObject("Parent");
  const SPtr<GameObject> child = scene.createGameObject("Child", parent.get());
  const SPtr<GameObject> grandChild = scene.createGameObject("GrandChild", child.get());
  parent->getTransform().setLocalPosition(Vector3(10.0f, 0.0f, 0.0f));
  parent->getTransform().setLocalRotation(Quaternion(Rotator(0.0f, 90.0f, 0.0f)));
  child->getTransform().setLocalPosition(Vector3(1.0f, 0.0f, 0.0f));
  child->addComponent<ScriptComponent>(probe);
  scene.update(0.1f);
  REQUIRE(scripts.runString("assert(Found.object and Found.transform)"));

  SECTION("Handles are the same value for the same object")
  {
    REQUIRE(scripts.runString(R"lua(
      local object = Found.object
      assert(object == Scene.find("Child"))
      assert(object:getTransform() == Found.transform)
      assert(Found.transform:getGameObject() == object)
      assert(object:getName() == "Child" and object:isValid())
      assert(tostring(object) == "GameObject(Child)")
      assert(object:getParent():getName() == "Parent")
      assert(object:getParent():getParent() == nil)
      assert(object:getChildCount() == 1 and object:getChild(1):getName() == "GrandChild")
      assert(object:getChild(2) == nil)
      assert(Scene.find("Parent"):findChild("GrandChild") == nil)
      assert(Scene.find("Parent"):findDescendant("GrandChild"):getName() == "GrandChild")
      assert(Scene.find("Nobody") == nil)
    )lua"));
  }

  SECTION("Local and world placement")
  {
    // The parent stands at x = 10 turned 90 degrees right, so the child's local +X is world +Y.
    REQUIRE(scripts.runString(R"lua(
      local t = Found.transform
      assert(t:getLocalPosition() == Vector3(1, 0, 0))
      assert(t:getWorldPosition():nearEqual(Vector3(10, 1, 0)))
      assert(t:getForward():nearEqual(Vector3.RIGHT))

      local out = Vector3()
      assert(rawequal(t:getWorldPosition(out), out))
      assert(out:nearEqual(Vector3(10, 1, 0)))

      t:setWorldPosition(10, 5, 2)
      assert(t:getWorldPosition():nearEqual(Vector3(10, 5, 2)))
      assert(t:getLocalPosition():nearEqual(Vector3(5, 0, 2)))

      t:setLocalPosition(Vector3(1, 0, 0))
      t:translateLocal(2, 0, 0)
      assert(t:getWorldPosition():nearEqual(Vector3(10, 3, 0)))
      t:translateWorld(0, 0, 4)
      assert(t:getWorldPosition():nearEqual(Vector3(10, 3, 4)))

      t:setWorldRotation(Quaternion.IDENTITY)
      assert(t:getForward():nearEqual(Vector3.FORWARD))
      t:rotateLocal(0, 90, 0)
      assert(t:getForward():nearEqual(Vector3.RIGHT))

      t:lookAt(Vector3(10, 3, 10))
      assert(t:getForward():nearEqual(Vector3.UP))
      t:lookAt(Vector3(20, 3, 4))
      assert(t:getForward():nearEqual(Vector3.FORWARD))

      t:setLocalScale(2, 2, 2)
      assert(t:getLocalScale() == Vector3(2, 2, 2))
    )lua"));
    scene.updateTransforms();
    REQUIRE(child->getTransform().getWorldPosition().nearEqual(Vector3(10.0f, 3.0f, 4.0f),
                                                               0.001f));
  }

  SECTION("A destroyed object is an error, and isValid says so")
  {
    scene.destroyGameObject(*grandChild);
    REQUIRE(scripts.runString(R"lua(
      Found.gone = Found.object:getChild(1)
    )lua"));
    REQUIRE(scripts.runString("assert(Found.gone == nil)"));

    REQUIRE(scripts.runString("Found.parent = Found.object:getParent()"));
    scene.destroyGameObject(*parent);
    REQUIRE(scripts.runString(R"lua(
      assert(not Found.parent:isValid() and not Found.object:isValid())
      assert(tostring(Found.object) == "GameObject(destroyed)")
    )lua"));
    REQUIRE_FALSE(scripts.runString("return Found.object:getName()"));
    REQUIRE_FALSE(scripts.runString("return Found.transform:getWorldPosition()"));
  }

  SECTION("A handle outlives its object without dangling")
  {
    {
      const SPtr<GameObject> temporary = scene.createGameObject("Temporary");
      ScriptTraits<GameObject>::push(scripts.getState(), *temporary);
      lua_setglobal(scripts.getState(), "Temporary");
      scene.destroyGameObject(*temporary);
    }
    REQUIRE(scripts.runString("assert(not Temporary:isValid())"));
    REQUIRE(scripts.runString("assert(tostring(Temporary) == 'GameObject(destroyed)')"));
    REQUIRE_FALSE(scripts.runString("return Temporary:getName()"));
  }

  SECTION("Scripts reach each other through getScript")
  {
    const SPtr<ScriptAsset> health = createTestScriptAsset("Health", R"lua(
      local Health = { points = 100 }
      function Health:onStart() self.started = true end
      function Health:damage(amount) self.points = self.points - amount end
      return Health
    )lua");
    const SPtr<ScriptAsset> attacker = createTestScriptAsset("Attacker", R"lua(
      local Attacker = {}
      function Attacker:onStart()
        local target = Scene.find("GrandChild"):getScript("Health")
        assert(target and target.started)
        target:damage(30)
        Found.points = target.points
        assert(self.gameObject:getScript("Nope") == nil)
      end
      return Attacker
    )lua");
    // The parent updates before its descendants, so the attacker starts the target itself.
    parent->addComponent<ScriptComponent>(attacker);
    grandChild->addComponent<ScriptComponent>(health);
    scene.update(0.1f);
    REQUIRE(scripts.runString("assert(Found.points == 70)"));

    REQUIRE(scripts.runString(R"lua(
      local holder = Scene.find("GrandChild")
      assert(holder:isScriptEnabled("Health"))
      holder:setScriptEnabled("Health", false)
      assert(not holder:isScriptEnabled("Health") and holder:getScript("Health") == nil)
    )lua"));
    REQUIRE_FALSE(scripts.runString("Scene.find('Parent'):setScriptEnabled('Nope', true)"));
  }

  SECTION("Raycast")
  {
    // The triangle lies flat at z = 0 around x = -20, y 0..2.
    const SPtr<GameObject> floor = scene.createGameObject("Floor");
    floor->addComponent<ModelComponent>(createTestModelAsset(createTriangleModel()));
    floor->getTransform().setLocalPosition(Vector3(-20.0f, 0.0f, 0.0f));
    scene.updateTransforms();
    REQUIRE(scripts.runString(R"lua(
      local origin = Vector3(-20, 0.5, 5)
      local hit = Scene.raycast(origin, Vector3(0, 0, -2))
      assert(hit and hit.gameObject == Scene.find("Floor"))
      assert(math.abs(hit.distance - 5) < 1e-4)
      assert(hit.point:nearEqual(Vector3(-20, 0.5, 0)))
      assert(hit.normal:nearEqual(Vector3.UP))

      assert(Scene.raycast(origin, Vector3.DOWN, 4) == nil)
      assert(Scene.raycast(origin, Vector3.DOWN, 10, hit.gameObject) == nil)
      assert(Scene.raycast(origin, Vector3.UP) == nil)

      local point = Vector3(0, 0, 0)
      local out = { point = point }
      assert(rawequal(Scene.raycast(origin, Vector3.DOWN, nil, nil, out), out))
      assert(rawequal(out.point, point) and point:nearEqual(Vector3(-20, 0.5, 0)))
      assert(out.normal:nearEqual(Vector3.UP))

      local below = Scene.raycast(Vector3(-20, 0.5, -5), Vector3.UP)
      assert(below and below.normal:nearEqual(Vector3.DOWN))
    )lua"));
    REQUIRE_FALSE(scripts.runString("Scene.raycast(Vector3(0, 0, 0), Vector3(0, 0, 0))"));
    REQUIRE_FALSE(scripts.runString("Scene.raycast(Vector3(0, 0, 0), Vector3.UP, 1, 5)"));
  }

  SECTION("Active camera")
  {
    REQUIRE(scripts.runString("assert(Scene.getActiveCamera() == nil)"));
    const SPtr<GameObject> high = scene.createGameObject("High");
    high->addComponent<CameraComponent>().setPriority(5);
    const SPtr<GameObject> low = scene.createGameObject("Low");
    CameraComponent& lowCamera = low->addComponent<CameraComponent>();
    REQUIRE(scripts.runString(R"lua(
      assert(Scene.getActiveCamera() == Scene.find("High"))
      Scene.setActiveCamera(Scene.find("Low"))
      assert(Scene.getActiveCamera() == Scene.find("Low"))
      Scene.setActiveCamera(nil)
      assert(Scene.getActiveCamera() == Scene.find("High"))
      Scene.setActiveCamera(Scene.find("Low"))
    )lua"));
    REQUIRE(scene.getActiveCamera() == &lowCamera);
    REQUIRE_FALSE(scripts.runString("Scene.setActiveCamera(Scene.find('Parent'))"));
    lowCamera.setEnabled(false);
    REQUIRE_FALSE(scripts.runString("Scene.setActiveCamera(Scene.find('Low'))"));
    REQUIRE(scripts.runString("assert(Scene.getActiveCamera() == Scene.find('High'))"));
  }

  SECTION("Screen and world points")
  {
    scripts.setGameView(Vector2::ZERO, Vector2::ZERO);
    REQUIRE(scripts.runString("assert(Scene.screenToWorld(Vector2(0, 0)) == nil)"));

    // A 2D camera: orthographic, 10 units in front of the plane x = 0, 4 units of half height
    // over a 200x100 view placed at (50, 20) in the window.
    const SPtr<GameObject> eye = scene.createGameObject("Eye");
    CameraComponent& camera = eye->addComponent<CameraComponent>();
    camera.setProjectionType(CameraProjectionType::Orthographic);
    camera.setOrthographicSize(4.0f);
    camera.setPriority(100);
    eye->getTransform().setLocalPosition(Vector3(-10.0f, 3.0f, 1.0f));
    scene.updateTransforms();
    REQUIRE(scripts.runString("assert(Scene.screenToWorld(Vector2(0, 0)) == nil)"));

    scripts.setGameView(Vector2(50.0f, 20.0f), Vector2(200.0f, 100.0f));
    REQUIRE(scripts.runString(R"lua(
      local function close(a, b) return (a - b):length() < 1e-3 end
      local center = Scene.screenToWorld(Vector2(150, 70))
      assert(close(center, Vector2(3, 1)))
      -- The top right corner: 8 units across, 4 up from the center.
      local corner = Vector2(0, 0)
      assert(rawequal(Scene.screenToWorld(Vector2(250, 20), 0, corner), corner))
      assert(close(corner, Vector2(11, 5)))
      assert(close(Scene.screenToWorld(Vector2(150, 70), 5), Vector2(3, 1)))
      assert(Scene.screenToWorld(Vector2(150, 70), -20) == nil)

      local origin, direction = Scene.screenToRay(Vector2(50, 120))
      assert(close(direction, Vector3.FORWARD))
      assert(math.abs(origin.y - -5) < 1e-4 and math.abs(origin.z - -3) < 1e-4)

      assert(close(Scene.worldToScreen(Vector3(0, 11, 5)), Vector2(250, 20)))
      local out = Vector2(0, 0)
      assert(rawequal(Scene.worldToScreen(Vector3(0, 3, 1), out), out))
      assert(close(out, Vector2(150, 70)))
    )lua"));
    scripts.setGameView(Vector2::ZERO, Vector2::ZERO);
  }

  SECTION("Animator")
  {
    const SPtr<Model> model = chMakeShared<Model>();
    const uint32 baseNode =
        addTestNode(*model, "Base", ModelNode::NO_PARENT, Matrix4::IDENTITY, {});
    static_cast<void>(addTestNode(*model, "Arm", baseNode, Matrix4::IDENTITY, {}));
    const SPtr<GameObject> robot = scene.createGameObject("Robot");
    robot->addComponent<ModelComponent>(createTestModelAsset(model));
    REQUIRE(scripts.runString("assert(Scene.find('Robot'):getAnimator() == nil)"));

    AnimatorComponent& animator =
        robot->addComponent<AnimatorComponent>(createTestClipAsset(createTestClip()));
    REQUIRE(scripts.runString(R"lua(
      local animator = Scene.find("Robot"):getAnimator()
      assert(animator == Scene.find("Robot"):getAnimator())
      assert(animator:getGameObject() == Scene.find("Robot"))
      assert(tostring(animator) == "Animator(Robot)")
      assert(animator:getClipName() == "A_Test" and animator:isPlaying())
      assert(animator:getDuration() == 2)
      animator:pause()
      assert(not animator:isPlaying())
      animator:setTime(1.5)
      animator:setSpeed(-1)
      animator:setLooping(false)
      assert(animator:getTime() == 1.5 and animator:getSpeed() == -1)
      assert(not animator:isLooping())
      animator:resume()
      assert(animator:isPlaying())
      animator:stop(0.25)
      assert(animator:getClipName() == nil)
      Found.animator = animator
    )lua"));
    REQUIRE(animator.getSpeed() == -1.0f);
    REQUIRE_FALSE(animator.getClip());
    // Names are looked up in the project's assets, which the tests have none of.
    REQUIRE_FALSE(scripts.runString("Found.animator:play('Run')"));
    REQUIRE(scripts.runString("Found.animator:play(nil)"));

    robot->removeComponent(animator);
    REQUIRE_FALSE(scripts.runString("Found.animator:isPlaying()"));
    REQUIRE(scripts.runString("assert(Scene.find('Robot'):getAnimator() == nil)"));
  }

  SECTION("Animator with a graph")
  {
    // Speed takes Idle to Walk; Jump goes there from any state; UpperBody moves the Spine.
    const SPtr<AnimationGraph> graph = chMakeShared<AnimationGraph>();
    const uint32 speed = graph->addParameter({.name = "Speed"});
    const uint32 jump =
        graph->addParameter({.name = "Jump", .type = AnimationParameterType::Trigger});
    static_cast<void>(
        graph->addParameter({.name = "Armed", .type = AnimationParameterType::Bool}));
    static_cast<void>(
        graph->addParameter({.name = "Weapon", .type = AnimationParameterType::Int}));
    AnimationLayer base{.name = "Base"};
    base.states.push_back(
        {.name = "Idle", .clip = addGraphTestClip(*graph, 0.0f, 0.0f, 1.0f)});
    base.states.push_back(
        {.name = "Walk", .clip = addGraphTestClip(*graph, 1.0f, 1.0f, 1.0f)});
    base.states.push_back({.name = "Jump",
                           .clip = addGraphTestClip(*graph, 9.0f, 9.0f, 1.0f),
                           .bLooping = false});
    base.transitions.push_back({.source = 0,
                                .target = 1,
                                .duration = 0.0f,
                                .conditions = {{.parameter = speed,
                                                .op = AnimationConditionOp::Greater,
                                                .threshold = {.floatValue = 0.5f}}}});
    base.transitions.push_back({.source = AnimationGraph::ANY_STATE,
                                .target = 2,
                                .duration = 0.0f,
                                .conditions = {{.parameter = jump}}});
    static_cast<void>(graph->addLayer(base));
    AnimationLayer upper{.name = "UpperBody", .weight = 0.25f, .maskBones = {"Spine"}};
    upper.states.push_back(
        {.name = "Aim", .clip = addGraphTestClip(*graph, 5.0f, 5.0f, 1.0f)});
    static_cast<void>(graph->addLayer(upper));
    REQUIRE(graph->validate());

    const SPtr<GameObject> walker = scene.createGameObject("Walker");
    walker->addComponent<ModelComponent>(createTestModelAsset(createGraphTestModel()));
    AnimatorComponent& animator = walker->addComponent<AnimatorComponent>();
    animator.setGraph(createTestGraphAsset(graph));

    REQUIRE(scripts.runString(R"lua(
        local animator = Scene.find("Walker"):getAnimator()
        assert(animator:hasGraph() and animator:getGraphName() == "AG_Test")
        local speed = animator:findParameter("Speed")
        assert(speed == 0 and animator:findParameter("Nope") == nil)
        animator:setFloat(speed, 2)
        assert(animator:getFloat("speed") == 2)
        animator:setBool("Armed", true)
        assert(animator:getBool("Armed"))
        animator:setInt("Weapon", 3)
        assert(animator:getInt("Weapon") == 3 and animator:getFloat("Weapon") == 3)
        animator:setTrigger("Jump")
        assert(animator:getBool("Jump"))
        animator:resetTrigger("Jump")
        assert(not animator:getBool("Jump"))
        assert(animator:getState() == "Idle" and animator:getState("UpperBody") == "Aim")
        assert(animator:getState(2) == "Aim" and not animator:isInTransition())
        assert(animator:getLayerWeight(2) == 0.25)
        animator:setLayerWeight("UpperBody", 1)
        assert(animator:getLayerWeight("UpperBody") == 1)
        assert(not animator:isClipOverGraph())
        Found.animator = animator
      )lua"));
    REQUIRE(animator.getGraphInstance().getFloat(speed) == 2.0f);

    scene.update(0.1f);
    REQUIRE(scripts.runString(R"lua(
        local animator = Found.animator
        assert(animator:getState() == "Walk")
        assert(animator:getStateTime() == 0)
        animator:setTrigger(1)
      )lua"));
    scene.update(0.1f);
    REQUIRE(scripts.runString("assert(Found.animator:getState() == 'Jump')"));

    // Wrong names, types and layers raise.
    REQUIRE_FALSE(scripts.runString("Found.animator:setFloat('Jump', 1)"));
    REQUIRE_FALSE(scripts.runString("Found.animator:setFloat('Nope', 1)"));
    REQUIRE_FALSE(scripts.runString("Found.animator:setTrigger(99)"));
    REQUIRE_FALSE(scripts.runString("Found.animator:getState(3)"));
    REQUIRE_FALSE(scripts.runString("Found.animator:getState('Legs')"));
    // Graphs are looked up in the project's assets, which the tests have none of.
    REQUIRE_FALSE(scripts.runString("Found.animator:setGraph('AG_Walk')"));

    REQUIRE(scripts.runString(R"lua(
        Found.animator:setGraph(nil)
        assert(not Found.animator:hasGraph() and Found.animator:getGraphName() == nil)
      )lua"));
    REQUIRE_FALSE(scripts.runString("Found.animator:setFloat('Speed', 1)"));
  }

  SECTION("DebugDraw draws into the running script's scene")
  {
    // Lines drawn in update go to the frame list, those drawn in a fixed step to the other.
    scripts.beginUpdate(0.0f);
    REQUIRE(scripts.runString(R"lua(
      DebugDraw.line(Vector3(0, 0, 0), Vector3(1, 0, 0))
      DebugDraw.line(Vector3(0, 0, 0), Vector3(1, 0, 0), Color(1, 0, 0, 1), true)
      DebugDraw.ray(Vector3(0, 0, 0), Vector3(0, 0, 5), 2)
      DebugDraw.ray(Vector3(0, 0, 0), Vector3(0, 0, 0), 2)
      DebugDraw.box(Vector3(0, 0, 0), Vector3(1, 1, 1))
      DebugDraw.orientedBox(Vector3(0, 0, 0), Vector3(1, 2, 3), Quaternion.FromEuler(0, 45, 0))
      DebugDraw.sphere(Vector3(0, 0, 0), 2)
      DebugDraw.circle(Vector3(0, 0, 0), Vector3.UP, 2)
      DebugDraw.capsule(Vector3(0, 0, 0), Vector3(0, 0, 2), 0.5)
      DebugDraw.axes(Vector3(0, 0, 0), Quaternion(), 1)
    )lua"));
    scripts.beginFixedUpdate(1.0f / 60.0f);
    REQUIRE(scripts.runString("DebugDraw.line(Vector3(0, 0, 0), Vector3(0, 1, 0))"));
    scripts.beginUpdate(0.0f);

#if USING(CH_DEBUG_DRAW)
    REQUIRE(scene.getDebugDraw().getVertices(DebugDepth::OnTop).size() == 2);
    // Two lines, one ray, a box, an oriented box and the three axes, plus the round shapes.
    REQUIRE(scene.getDebugDraw().getLineCount() > 2 + 1 + 12 + 12 + 3);
    REQUIRE(scene.getFixedDebugDraw().getLineCount() == 1);
    REQUIRE_FALSE(scripts.runString("DebugDraw.line(Vector3(0, 0, 0))"));
#else
    // Without debug lines the arguments are not even read.
    REQUIRE(scene.getDebugDraw().getLineCount() == 0);
    REQUIRE(scripts.runString("DebugDraw.line(Vector3(0, 0, 0))"));
#endif
  }
}

TEST_CASE("chCore - Script spawning", "[Scripting]")
{
  ScriptManager& scripts = getTestScriptManager();
  REQUIRE(scripts.runString("Spawn = { updates = 0, destroyed = 0 }"));
  const SPtr<ScriptAsset> spawner = createTestScriptAsset("Spawner", R"lua(
    local Spawner = {}
    function Spawner:update(dt)
      Spawn.updates = Spawn.updates + 1
      if Spawn.updates == 1 then
        local copy = self.gameObject:clone(nil, Vector3(5, 0, 0))
        Spawn.copy = copy
        Spawn.copyValid = copy:isValid()
        Spawn.kid = self.gameObject:getChild(1)
        self.gameObject:destroy()
        Spawn.selfValid = self.gameObject:isValid()
        Spawn.kidValid = Spawn.kid:isValid()
        -- Still usable until the update ends.
        Spawn.nameAfterDestroy = self.gameObject:getName()
      end
    end
    function Spawner:onDestroy()
      Spawn.destroyed = Spawn.destroyed + 1
    end
    return Spawner
  )lua");

  Scene scene("Spawning", chEngineSDK::UUID::createRandom());
  scene.setPlaying(true);
  scripts.setCurrentScene(&scene);

  SECTION("A script clones and destroys its own object during the update")
  {
    const SPtr<GameObject> original = scene.createGameObject("Original");
    original->getTransform().setLocalPosition(Vector3(1.0f, 2.0f, 3.0f));
    scene.createGameObject("Kid", original.get());
    original->addComponent<ScriptComponent>(spawner);

    scene.update(0.1f);
    // The copy is added during the update and only updates from the next one.
    REQUIRE(scripts.runString(R"lua(
      assert(Spawn.updates == 1, Spawn.updates)
      assert(Spawn.copyValid and not Spawn.selfValid and not Spawn.kidValid)
      assert(Spawn.nameAfterDestroy == "Original")
      assert(Spawn.destroyed == 1)
      assert(Spawn.copy:isValid() and Spawn.copy:getChildCount() == 1)
      assert(Spawn.copy:getTransform():getWorldPosition():nearEqual(Vector3(5, 0, 0)))
    )lua"));
    REQUIRE(original->getScene() == nullptr);
    REQUIRE(scene.getRootGameObjects().size() == 1);
    const SPtr<GameObject> copy = scene.getRootGameObjects()[0];
    REQUIRE(copy->getName() == "Original");
    REQUIRE(copy->getUUID() != original->getUUID());
    REQUIRE(copy->getChildren()[0]->getName() == "Kid");
    REQUIRE_FALSE(scripts.runString("return Spawn.kid:getName()"));

    scene.update(0.1f);
    // Without an AssetManager the copy's script component cannot find its script, so only the
    // original ran.
    REQUIRE(scripts.runString("assert(Spawn.updates == 1 and Spawn.copy:isValid())"));

    // Asked outside an update, it waits for the next one to end.
    REQUIRE(scripts.runString("Spawn.copy:destroy()"));
    REQUIRE(scripts.runString("assert(not Spawn.copy:isValid())"));
    REQUIRE(scene.getRootGameObjects().size() == 1);
    REQUIRE(copy->isDestroyRequested());
    scene.update(0.1f);
    REQUIRE(scene.getRootGameObjects().empty());
    REQUIRE(scripts.runString("assert(not Spawn.copy:isValid())"));
    REQUIRE_FALSE(scripts.runString("Spawn.copy:destroy()"));
  }

  SECTION("A clone under another parent keeps its world placement")
  {
    const SPtr<GameObject> parent = scene.createGameObject("Parent");
    parent->getTransform().setLocalPosition(Vector3(10.0f, 0.0f, 0.0f));
    parent->getTransform().setLocalRotation(Quaternion(Rotator(0.0f, 90.0f, 0.0f)));
    const SPtr<GameObject> source = scene.createGameObject("Source");
    source->getTransform().setLocalPosition(Vector3(1.0f, 0.0f, 0.0f));
    REQUIRE(scripts.runString(R"lua(
      local source = Scene.find("Source")
      local parent = Scene.find("Parent")
      local copy = source:clone(parent)
      assert(copy:getParent() == parent)
      assert(copy:getTransform():getWorldPosition():nearEqual(Vector3(1, 0, 0)))
      assert(copy:getTransform():getForward():nearEqual(Vector3.FORWARD))

      local placed = source:clone(parent, Vector3(0, 0, 4), Quaternion.FromEuler(0, 90, 0))
      assert(placed:getTransform():getWorldPosition():nearEqual(Vector3(0, 0, 4)))
      assert(placed:getTransform():getForward():nearEqual(Vector3.RIGHT))

      -- A root clone by default stands where the source stands.
      assert(source:clone():getTransform():getWorldPosition():nearEqual(Vector3(1, 0, 0)))
    )lua"));
    REQUIRE(parent->getChildren().size() == 2);
    REQUIRE(scene.getRootGameObjects().size() == 3);

    Scene other("Other", chEngineSDK::UUID::createRandom());
    const SPtr<GameObject> stranger = other.createGameObject("Stranger");
    ScriptTraits<GameObject>::push(scripts.getState(), *stranger);
    lua_setglobal(scripts.getState(), "Stranger");
    REQUIRE_FALSE(scripts.runString("Scene.find('Source'):clone(Stranger)"));
    REQUIRE(scripts.runString("Stranger = nil"));
  }

  SECTION("Without assets, prefabs cannot be found")
  {
    REQUIRE_FALSE(scripts.runString("Scene.instantiate('/Game/Enemies/Orc')"));
    REQUIRE_FALSE(scripts.runString("Scene.findPrefab('Orc')"));
    REQUIRE_FALSE(scripts.runString("Prefab()"));
  }

  SECTION("A Prefab property gives a Prefab value and is a reference")
  {
    const SPtr<ScriptAsset> gun = createTestScriptAsset("Gun", R"lua(
      local Gun = {}
      Gun.properties = { { "bullet", type = "Prefab" } }
      function Gun:onStart()
        Spawn[self.gameObject:getName()] = self.bullet
      end
      return Gun
    )lua");
    const ScriptClassInfo* scriptClass = scripts.getClass(*gun);
    REQUIRE(scriptClass);
    REQUIRE(scriptClass->bValid);
    REQUIRE(scriptClass->properties[0].defaultValue.type == ScriptPropertyType::Prefab);
    REQUIRE(scriptClass->properties[0].defaultValue.object.isNull());

    const SPtr<GameObject> empty = scene.createGameObject("Empty");
    const SPtr<GameObject> loaded = scene.createGameObject("Loaded");
    const SPtr<GameObject> again = scene.createGameObject("Again");
    empty->addComponent<ScriptComponent>(gun);
    ScriptPropertyValue bullet = ScriptPropertyValue::makeDefault(ScriptPropertyType::Prefab);
    bullet.object = chEngineSDK::UUID::createRandom();
    ScriptComponent& loadedScript = loaded->addComponent<ScriptComponent>(gun);
    loadedScript.setOverride("bullet", bullet);
    again->addComponent<ScriptComponent>(gun).setOverride("bullet", bullet);
    scene.update(0.1f);
    REQUIRE(scripts.runString(R"lua(
      assert(Spawn.Empty == nil)
      assert(Spawn.Loaded ~= nil and Spawn.Loaded == Spawn.Again)
      assert(not Spawn.Loaded:isValid() and Spawn.Loaded:getName() == nil)
      assert(tostring(Spawn.Loaded) == "Prefab(missing)")
    )lua"));

    Vector<chEngineSDK::UUID> references;
    loadedScript.collectReferences(references);
    REQUIRE(references.size() == 2);
    REQUIRE(references[1] == bullet.object);

    MemoryDataStream stream;
    ScriptPropertyUtils::writeValue(stream, bullet);
    stream.seek(0);
    ScriptPropertyValue read;
    REQUIRE(ScriptPropertyUtils::readValue(stream, read));
    REQUIRE(read == bullet);
  }
  scripts.setCurrentScene(nullptr);
  scripts.forgetClass(spawner->getUUID());
}
