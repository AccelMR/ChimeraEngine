/************************************************************************/
/**
 * @file chProjectilesTests.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Tests of the projectiles plugin, built with its sources.
 */
/************************************************************************/
#include "catch.hpp"

#include <chrono>

#include "chAlgorithm.h"
#include "chComponentEditor.h"
#include "chComponentRegister.h"
#include "chFileStream.h"
#include "chGameObject.h"
#include "chMath.h"
#include "chProjectileBindings.h"
#include "chProjectileSystemComponent.h"
#include "chProjectileTargetComponent.h"
#include "chProjectilesEditor.h"
#include "chScene.h"
#include "chScriptApi.h"
#include "chScriptManager.h"
#include "chSpriteDraw.h"

#include "lua.h"

using namespace chEngineSDK;

namespace {
/*
 * The bindings must be added before the state is made, as loadPlugin does.
 */
ScriptManager&
getScripts()
{
  if (!ScriptManager::isStarted()) {
    ScriptManager::addBindings(&ProjectileBindings::registerBindings);
    ScriptManager::startUp();
  }
  return ScriptManager::instance();
}

/*
 * Reads a number global the scripts wrote.
 */
double
getGlobalNumber(ScriptManager& scripts, const ANSICHAR* name)
{
  lua_State* state = scripts.getState();
  lua_getglobal(state, name);
  const double value = lua_tonumber(state, -1);
  lua_pop(state, 1);
  return value;
}

GameObject&
makeTarget(Scene& scene, const ANSICHAR* name, const Vector2& position, const uint32 team)
{
  GameObject& object = *scene.createGameObject(name, nullptr);
  object.getTransform().setLocalPosition(Vector3(0.0f, position.x, position.y));
  object.addComponent<ProjectileTargetComponent>(0.5f, team);
  return object;
}

constexpr float kStep = 1.0f / 60.0f;
} // namespace

TEST_CASE("Projectiles - movement and lifetime", "[Projectiles]")
{
  Scene scene("Projectiles", chEngineSDK::UUID::createRandom());
  ProjectileSystemComponent& system =
      scene.createGameObject("Gun", nullptr)->addComponent<ProjectileSystemComponent>();

  SECTION("Projectiles move with their velocity and acceleration")
  {
    REQUIRE(system.spawn({.position = Vector2(1.0f, 2.0f),
                          .velocity = Vector2(3.0f, 0.0f),
                          .acceleration = Vector2(0.0f, -6.0f)}));
    REQUIRE(system.fire(Vector2::ZERO, 90.0f, 2.0f));
    system.step(0.5f);
    REQUIRE(system.getCount() == 2);
    // A step of zero only draws, so the sprite centers are the positions.
    scene.setPlaying(true);
    scene.fixedUpdate(0.0f);
    const Span<const SpriteItem> sprites = scene.getFixedSpriteDraw().getSprites();
    REQUIRE(sprites.size() == 2);
    // The velocity changes first, then moves the projectile (semi-implicit Euler).
    REQUIRE(sprites[0].instance.center.nearEqual(Vector3(0.0f, 2.5f, 0.5f), 1e-4f));
    REQUIRE(sprites[1].instance.center.nearEqual(Vector3(0.0f, 0.0f, 1.0f), 1e-4f));
  }

  SECTION("A projectile dies at the end of its lifetime")
  {
    REQUIRE(system.spawn({.velocity = Vector2(1.0f, 0.0f), .lifetime = 0.25f}));
    system.step(0.2f);
    REQUIRE(system.getCount() == 1);
    system.step(0.1f);
    REQUIRE(system.getCount() == 0);
  }

  SECTION("A full system drops new projectiles")
  {
    system.setCapacity(3);
    REQUIRE(system.fireRing(Vector2::ZERO, 5, 0.0f, 1.0f) == 3);
    REQUIRE_FALSE(system.fire(Vector2::ZERO, 0.0f, 1.0f));
    system.setCapacity(2);
    REQUIRE(system.getCount() == 2);
    system.clear();
    REQUIRE(system.getCount() == 0);
    REQUIRE(system.getCapacity() == 2);
    system.setCapacity(0);
    REQUIRE(system.getCapacity() == 1);
  }
}

TEST_CASE("Projectiles - patterns", "[Projectiles]")
{
  Scene scene("Projectiles", chEngineSDK::UUID::createRandom());
  GameObject& gun = *scene.createGameObject("Gun", nullptr);
  ProjectileSystemComponent& system = gun.addComponent<ProjectileSystemComponent>();
  // Hits are checked by making targets where the patterns must reach.
  scene.setPlaying(true);

  SECTION("A ring goes around evenly from its start angle")
  {
    makeTarget(scene, "Right", Vector2(2.0f, 0.0f), 1);
    makeTarget(scene, "Up", Vector2(0.0f, 2.0f), 1);
    makeTarget(scene, "Left", Vector2(-2.0f, 0.0f), 1);
    makeTarget(scene, "Down", Vector2(0.0f, -2.0f), 1);
    system.setDefaults({.radius = 0.05f});
    REQUIRE(system.fireRing(Vector2::ZERO, 4, 0.0f, 1.0f) == 4);
    uint32 hits = 0;
    for (uint32 i = 0; i < 120; ++i) {
      system.step(kStep);
      hits += static_cast<uint32>(system.getLastHits().size());
    }
    REQUIRE(hits == 4);
    REQUIRE(system.getCount() == 0);
  }

  SECTION("A spread is centered on its angle")
  {
    // Three shots over 90 degrees around up: 45, 90 and 135.
    GameObject& up = makeTarget(scene, "Up", Vector2(0.0f, 2.0f), 1);
    GameObject& diagonal = makeTarget(scene, "Diagonal", Vector2(1.414f, 1.414f), 1);
    system.setDefaults({.radius = 0.05f});
    REQUIRE(system.fireSpread(Vector2::ZERO, 3, 90.0f, 90.0f, 1.0f) == 3);
    Vector<const GameObject*> hitTargets;
    for (uint32 i = 0; i < 120; ++i) {
      system.step(kStep);
      for (const ProjectileHit& hit : system.getLastHits()) {
        hitTargets.push_back(hit.target);
      }
    }
    REQUIRE(hitTargets.size() == 2);
    REQUIRE(Algorithm::contains(hitTargets, &up));
    REQUIRE(Algorithm::contains(hitTargets, &diagonal));
    // The shot to the left (135 degrees) is still flying.
    REQUIRE(system.getCount() == 1);
    REQUIRE(system.fireSpread(Vector2::ZERO, 0, 0.0f, 90.0f, 1.0f) == 0);
    REQUIRE(system.fireSpread(Vector2::ZERO, 1, 0.0f, 90.0f, 1.0f) == 1);
  }
}

TEST_CASE("Projectiles - hits", "[Projectiles]")
{
  Scene scene("Projectiles", chEngineSDK::UUID::createRandom());
  GameObject& gun = *scene.createGameObject("Gun", nullptr);
  ProjectileSystemComponent& system = gun.addComponent<ProjectileSystemComponent>();
  GameObject& enemy = makeTarget(scene, "Enemy", Vector2(3.0f, 0.0f), 2);

  SECTION("A projectile that reaches a target is listed once and dies")
  {
    REQUIRE(system.spawn({.velocity = Vector2(6.0f, 0.0f), .radius = 0.1f, .damage = 4.0f}));
    uint32 hits = 0;
    for (uint32 i = 0; i < 60; ++i) {
      system.step(kStep);
      for (const ProjectileHit& hit : system.getLastHits()) {
        REQUIRE(hit.target == &enemy);
        REQUIRE(hit.damage == 4.0f);
        // Touching counts: the edges meet 2.4 units along.
        REQUIRE(hit.position.x >= 2.4f - 0.11f);
        ++hits;
      }
    }
    REQUIRE(hits == 1);
    REQUIRE(system.getCount() == 0);
  }

  SECTION("Pierce goes through targets, each hit once")
  {
    GameObject& second = makeTarget(scene, "Second", Vector2(5.0f, 0.0f), 2);
    REQUIRE(system.spawn({.velocity = Vector2(6.0f, 0.0f), .lifetime = 10.0f, .pierce = 1}));
    Vector<const GameObject*> hitTargets;
    for (uint32 i = 0; i < 120; ++i) {
      system.step(kStep);
      for (const ProjectileHit& hit : system.getLastHits()) {
        hitTargets.push_back(hit.target);
      }
    }
    REQUIRE(hitTargets.size() == 2);
    REQUIRE(hitTargets[0] == &enemy);
    REQUIRE(hitTargets[1] == &second);
    REQUIRE(system.getCount() == 0);
  }

  SECTION("Only the teams the system hits count, and never its own object")
  {
    gun.addComponent<ProjectileTargetComponent>(5.0f, 1);
    system.setHitTeams(1u << 0);
    REQUIRE(system.spawn({.velocity = Vector2(6.0f, 0.0f)}));
    for (uint32 i = 0; i < 60; ++i) {
      system.step(kStep);
      REQUIRE(system.getLastHits().empty());
    }
    system.setHitTeams(ProjectileSystemComponent::ALL_TEAMS);
    REQUIRE(system.spawn({.velocity = Vector2(6.0f, 0.0f)}));
    uint32 hits = 0;
    for (uint32 i = 0; i < 60; ++i) {
      system.step(kStep);
      hits += static_cast<uint32>(system.getLastHits().size());
    }
    REQUIRE(hits == 1);
  }

  SECTION("A disabled target and a target of another scene are not hit")
  {
    enemy.getComponent<ProjectileTargetComponent>()->setEnabled(false);
    Scene other("Other", chEngineSDK::UUID::createRandom());
    makeTarget(other, "Elsewhere", Vector2(3.0f, 0.0f), 2);
    REQUIRE(ProjectileTargetComponent::getTargets(scene).empty());
    REQUIRE(ProjectileTargetComponent::getTargets(other).size() == 1);
    REQUIRE(system.spawn({.velocity = Vector2(6.0f, 0.0f)}));
    for (uint32 i = 0; i < 60; ++i) {
      system.step(kStep);
      REQUIRE(system.getLastHits().empty());
    }
  }

  SECTION("Targets leave the list in any order")
  {
    GameObject& second = makeTarget(scene, "Second", Vector2(0.0f, 5.0f), 2);
    GameObject& third = makeTarget(scene, "Third", Vector2(0.0f, 7.0f), 2);
    REQUIRE(ProjectileTargetComponent::getTargets(scene).size() == 3);
    scene.destroyGameObject(second);
    REQUIRE(ProjectileTargetComponent::getTargets(scene).size() == 2);
    third.getComponent<ProjectileTargetComponent>()->setEnabled(false);
    enemy.getComponent<ProjectileTargetComponent>()->setEnabled(false);
    REQUIRE(ProjectileTargetComponent::getTargets(scene).empty());
  }
}

TEST_CASE("Projectiles - homing", "[Projectiles]")
{
  Scene scene("Projectiles", chEngineSDK::UUID::createRandom());
  ProjectileSystemComponent& system =
      scene.createGameObject("Gun", nullptr)->addComponent<ProjectileSystemComponent>();
  GameObject& goal = makeTarget(scene, "Goal", Vector2(0.0f, 4.0f), 1);

  SECTION("A turning projectile shot away from its target still reaches it")
  {
    system.setHomingTarget(&goal);
    REQUIRE(system.getHomingTarget() == &goal);
    REQUIRE(system.spawn({.velocity = Vector2(4.0f, 0.0f), .turnRate = 180.0f}));
    // Flies straight without a turn rate.
    REQUIRE(system.spawn({.velocity = Vector2(4.0f, 0.0f)}));
    uint32 hits = 0;
    for (uint32 i = 0; i < 240; ++i) {
      system.step(kStep);
      hits += static_cast<uint32>(system.getLastHits().size());
    }
    REQUIRE(hits == 1);
  }

  SECTION("Without a target every projectile flies straight")
  {
    system.setHomingTarget(&goal);
    scene.destroyGameObject(goal);
    REQUIRE(system.getHomingTarget() == nullptr);
    REQUIRE(system.spawn({.velocity = Vector2(4.0f, 0.0f), .turnRate = 180.0f}));
    for (uint32 i = 0; i < 240; ++i) {
      system.step(kStep);
      REQUIRE(system.getLastHits().empty());
    }
  }
}

TEST_CASE("Projectiles - drawing", "[Projectiles]")
{
  Scene scene("Projectiles", chEngineSDK::UUID::createRandom());
  ProjectileSystemComponent& system =
      scene.createGameObject("Gun", nullptr)->addComponent<ProjectileSystemComponent>();
  ProjectileLook look;
  look.size = Vector2(2.0f, 1.0f);
  look.depth = 3.0f;
  look.sortingLayer = 4;
  look.blendMode = SpriteBlendMode::Additive;
  system.setLook(look);

  SECTION("Outside Play projectiles move and draw but hit nothing")
  {
    scene.fixedUpdate(kStep);
    REQUIRE(scene.getFixedSpriteDraw().getSprites().empty());
    makeTarget(scene, "Target", Vector2(0.5f, 0.0f), 1);
    system.fire(Vector2::ZERO, 0.0f, 1.0f);
    for (uint32 i = 0; i < 60; ++i) {
      scene.fixedUpdate(kStep);
      REQUIRE(system.getLastHits().empty());
    }
    REQUIRE(system.getCount() == 1);
    REQUIRE(scene.getFixedSpriteDraw().getSprites().size() == 1);
  }

  SECTION("Each projectile is one sprite in the fixed list, facing its velocity")
  {
    scene.setPlaying(true);
    system.fire(Vector2(1.0f, 2.0f), 90.0f, 1.0f);
    system.fire(Vector2::ZERO, 0.0f, 1.0f);
    scene.fixedUpdate(kStep);
    const Span<const SpriteItem> sprites = scene.getFixedSpriteDraw().getSprites();
    REQUIRE(sprites.size() == 2);
    REQUIRE(sprites[0].sortingLayer == 4);
    REQUIRE(sprites[0].blendMode == SpriteBlendMode::Additive);
    // Going up, so its right side points up.
    REQUIRE(sprites[0].instance.halfAxisU.nearEqual(Vector3(0.0f, 0.0f, 1.0f), 1e-4f));
    REQUIRE(sprites[0].instance.halfAxisV.nearEqual(Vector3(0.0f, -0.5f, 0.0f), 1e-4f));
    REQUIRE(sprites[0].instance.center.x == 3.0f);
    REQUIRE(sprites[1].instance.halfAxisU.nearEqual(Vector3(0.0f, 1.0f, 0.0f), 1e-4f));
    REQUIRE(sprites[0].instance.textureAndFlags
            == (GpuSpriteInstance::NO_TEXTURE | GpuSpriteInstance::POINT_FILTER_BIT));

    look.bFaceVelocity = false;
    system.setLook(look);
    scene.fixedUpdate(kStep);
    REQUIRE(scene.getFixedSpriteDraw().getSprites()[0].instance.halfAxisU.nearEqual(
        Vector3(0.0f, 1.0f, 0.0f), 1e-4f));
  }
}

TEST_CASE("Projectiles - saving", "[Projectiles]")
{
  ComponentRegister::registerType<ProjectileSystemComponent>();
  ComponentRegister::registerType<ProjectileTargetComponent>();

  ProjectileSystemComponent source;
  ProjectileLook look;
  look.texture = chEngineSDK::UUID::createRandom();
  look.size = Vector2(0.5f, 0.25f);
  look.color = LinearColor(1.0f, 0.5f, 0.25f, 1.0f);
  look.sortingLayer = 2;
  look.order = -3;
  look.filter = SpriteFilter::Linear;
  look.bFaceVelocity = false;
  source.setLook(look);
  source.setDefaults({.speed = 9.0f, .damage = 3.0f, .turnRate = 45.0f, .pierce = 2});
  source.setCapacity(500);
  source.setHitTeams(0b110);
  source.setCellSize(0.75f);

  MemoryDataStream stream;
  source.serialize(stream);
  const SIZE_T size = stream.tell();

  SECTION("Settings come back")
  {
    stream.seek(0);
    ProjectileSystemComponent copy;
    REQUIRE(copy.deserialize(stream));
    REQUIRE(copy.getLook().texture == look.texture);
    REQUIRE(copy.getLook().size == look.size);
    REQUIRE(copy.getLook().color == look.color);
    REQUIRE(copy.getLook().order == -3);
    REQUIRE(copy.getLook().filter == SpriteFilter::Linear);
    REQUIRE_FALSE(copy.getLook().bFaceVelocity);
    REQUIRE(copy.getDefaults().speed == 9.0f);
    REQUIRE(copy.getDefaults().turnRate == 45.0f);
    REQUIRE(copy.getDefaults().pierce == 2);
    REQUIRE(copy.getCapacity() == 500);
    REQUIRE(copy.getHitTeams() == 0b110u);
    REQUIRE(copy.getCellSize() == 0.75f);

    Vector<chEngineSDK::UUID> references;
    copy.collectReferences(references);
    REQUIRE(references.size() == 1);
    REQUIRE(references[0] == look.texture);
  }

  SECTION("A cut file is refused")
  {
    MemoryDataStream cut;
    stream.seek(0);
    Vector<uint8> bytes(size);
    stream.read(bytes.data(), size - 1);
    cut.write(bytes.data(), size - 1);
    cut.seek(0);
    ProjectileSystemComponent copy;
    REQUIRE_FALSE(copy.deserialize(cut));
  }

  SECTION("Targets keep radius and team")
  {
    ProjectileTargetComponent target(2.5f, 7);
    MemoryDataStream targetStream;
    target.serialize(targetStream);
    targetStream.seek(0);
    ProjectileTargetComponent copy;
    REQUIRE(copy.deserialize(targetStream));
    REQUIRE(copy.getRadius() == 2.5f);
    REQUIRE(copy.getTeam() == 7);
    copy.setTeam(99);
    REQUIRE(copy.getTeam() == ProjectileTargetComponent::MAX_TEAMS);
  }

  SECTION("The register makes both types")
  {
    REQUIRE(ComponentRegister::create(TypeTraits<ProjectileSystemComponent>::getTypeId()));
    REQUIRE(ComponentRegister::create(TypeTraits<ProjectileTargetComponent>::getTypeId()));
  }
}

TEST_CASE("Projectiles - scripts", "[Projectiles]")
{
  ScriptManager& scripts = getScripts();
  Scene scene("Projectiles", chEngineSDK::UUID::createRandom());
  struct SceneGuard {
    ScriptManager& scripts;
    ~SceneGuard() { scripts.setCurrentScene(nullptr); }
  } guard{scripts};
  scripts.setCurrentScene(&scene);
  scene.setPlaying(true);
  scene.createGameObject("Gun", nullptr);
  scene.createGameObject("Enemy", nullptr)
      ->getTransform()
      .setLocalPosition(Vector3(0.0f, 3.0f, 0.0f));

  SECTION("A script fires and hears about hits")
  {
    REQUIRE(scripts.runString(R"lua(
      local gun = Scene.find("Gun")
      assert(gun:getProjectiles() == nil)
      local system = gun:addProjectiles()
      assert(gun:getProjectiles() == system)
      assert(tostring(system) == "ProjectileSystem(Gun)")
      assert(system:getGameObject() == gun)
      system:configure{ speed = 6, radius = 0.1, damage = 2.5, capacity = 64 }
      assert(system:getCapacity() == 64)
      system:setLook(Sprites.makeStyle{ size = Vector2(0.2, 0.2) })
      system:setHitTeams({ 2 })
      local teams = system:getHitTeams()
      assert(#teams == 1 and teams[1] == 2)

      local enemy = Scene.find("Enemy")
      local target = enemy:addProjectileTarget(0.5, 2)
      assert(target:getTeam() == 2 and target:getRadius() == 0.5)
      assert(tostring(target) == "ProjectileTarget(Enemy)")

      Hits, Damage, HitX = 0, 0, 0
      system:onHit(function(hitTarget, damage, x, y)
        assert(hitTarget == enemy)
        Hits = Hits + 1
        Damage = Damage + damage
        HitX = x
      end)
      assert(system:fire(0, 0, 0))
      assert(system:spawn{ x = 0, y = 1, angle = -90, speed = 1 })
      assert(system:getCount() == 2)
    )lua"));

    for (uint32 i = 0; i < 60; ++i) {
      scene.fixedUpdate(kStep);
    }
    REQUIRE(getGlobalNumber(scripts, "Hits") == 1.0);
    REQUIRE(getGlobalNumber(scripts, "Damage") == 2.5);
    REQUIRE(getGlobalNumber(scripts, "HitX") > 2.3);
    REQUIRE(scripts.runString("assert(Scene.find('Gun'):getProjectiles():getCount() == 1)"));
  }

#if USING(CH_EDITOR)
  SECTION("Code editors see the plugin's types and the GameObject methods it adds")
  {
    const String text = scripts.getApiDescription()->writeDefinitions();
    const auto contains = [&text](const ANSICHAR* part) {
      INFO(part);
      CHECK(text.find(part) != String::npos);
    };
    contains("---@class ProjectileSystem\n");
    contains("---@class ProjectileTarget\n");
    contains("---@class ProjectileOptions\n");
    contains("---@return ProjectileSystem?\nfunction GameObject:getProjectiles() end\n");
    contains("function ProjectileSystem:fireRing(x, y, count, startAngle, speed) end\n");
    // The engine's own methods are still there.
    contains("---@return string\nfunction GameObject:getName() end\n");
  }
#endif // USING(CH_EDITOR)

  SECTION("Mistakes raise")
  {
    REQUIRE(scripts.runString("Scene.find('Gun'):addProjectiles()"));
    REQUIRE_FALSE(
        scripts.runString("Scene.find('Gun'):getProjectiles():configure{ sped = 1 }"));
    REQUIRE_FALSE(scripts.runString("Scene.find('Gun'):getProjectiles():spawn{ x = 1 }"));
    REQUIRE_FALSE(scripts.runString("Scene.find('Gun'):getProjectiles():setHitTeams({ 40 })"));
    REQUIRE_FALSE(scripts.runString("Scene.find('Gun'):getProjectiles():fireRing(0, 0, -1)"));
    REQUIRE_FALSE(scripts.runString("Scene.find('Enemy'):addProjectileTarget(1, 0)"));
  }

  SECTION("A removed component is an error, not a crash")
  {
    REQUIRE(scripts.runString("Kept = Scene.find('Gun'):addProjectiles()"));
    GameObject* gun = nullptr;
    for (const SPtr<GameObject>& object : scene.getRootGameObjects()) {
      if (object->getName() == "Gun") {
        gun = object.get();
      }
    }
    REQUIRE(gun);
    gun->removeComponent(*gun->getComponent<ProjectileSystemComponent>());
    REQUIRE_FALSE(scripts.runString("Kept:getCount()"));
    REQUIRE(scripts.runString("assert(Scene.find('Gun'):getProjectiles() == nil)"));
  }
}

#if USING(CH_EDITOR)
TEST_CASE("Projectiles - editor", "[Projectiles]")
{
  ProjectilesEditor::registerDescriptions();
  const ComponentEditorDesc* systemDesc =
      ComponentEditorRegister::find(TypeTraits<ProjectileSystemComponent>::getTypeId());
  const ComponentEditorDesc* targetDesc =
      ComponentEditorRegister::find(TypeTraits<ProjectileTargetComponent>::getTypeId());
  REQUIRE(systemDesc);
  REQUIRE(targetDesc);
  REQUIRE(systemDesc->category == "Projectiles");

  const auto findEntry = [](const ComponentEditorDesc& desc, const ANSICHAR* label) {
    for (const ComponentEditorEntry& entry : desc.entries) {
      if (entry.label == label) {
        return &entry;
      }
    }
    return static_cast<const ComponentEditorEntry*>(nullptr);
  };

  Scene scene("Projectiles", chEngineSDK::UUID::createRandom());
  GameObject& gun = *scene.createGameObject("Gun", nullptr);
  gun.getTransform().setLocalPosition(Vector3(0.0f, 2.0f, 3.0f));
  ProjectileSystemComponent& system = gun.addComponent<ProjectileSystemComponent>();

  SECTION("Rows edit the look, the shots and the system")
  {
    const ComponentEditorEntry* blend = findEntry(*systemDesc, "Blend");
    REQUIRE(blend);
    REQUIRE(blend->widget == ComponentPropertyWidget::Enum);
    ScriptPropertyValue value;
    value.type = blend->type;
    blend->getter(system, value);
    value.integer = 1;
    blend->setter(system, value);
    REQUIRE(system.getLook().blendMode == SpriteBlendMode::Additive);

    const ComponentEditorEntry* speed = findEntry(*systemDesc, "Speed");
    REQUIRE(speed);
    value.type = speed->type;
    speed->getter(system, value);
    REQUIRE(value.number == 5.0);
    value.number = 7.0;
    speed->setter(system, value);
    REQUIRE(system.getDefaults().speed == 7.0f);

    const ComponentEditorEntry* teams = findEntry(*systemDesc, "Hit Teams");
    REQUIRE(teams);
    REQUIRE(teams->widget == ComponentPropertyWidget::Flags);
    REQUIRE(teams->names.size() == 16);
    value.type = teams->type;
    value.integer = 0b100;
    teams->setter(system, value);
    REQUIRE(system.getHitTeams() == 0b100u);

    const ComponentEditorEntry* team = findEntry(*targetDesc, "Team");
    REQUIRE(team);
    REQUIRE(team->maximum == 32.0);
  }

  SECTION("Test Fire shoots a ring from the object while the scene does not play")
  {
    const ComponentEditorEntry* testFire = findEntry(*systemDesc, "Test Fire");
    REQUIRE(testFire);
    REQUIRE(testFire->kind == ComponentEditorEntryKind::Action);
    testFire->action(system);
    REQUIRE(system.getCount() == ProjectileSystemComponent::TEST_FIRE_COUNT);
    scene.fixedUpdate(0.0f);
    const Span<const SpriteItem> sprites = scene.getFixedSpriteDraw().getSprites();
    REQUIRE(sprites.size() == ProjectileSystemComponent::TEST_FIRE_COUNT);
    REQUIRE(sprites[0].instance.center.nearEqual(Vector3(0.0f, 2.0f, 3.0f), 1e-4f));
    // They die at the end of their lifetime, also outside Play.
    for (uint32 i = 0; i < 6 * 60; ++i) {
      scene.fixedUpdate(kStep);
    }
    REQUIRE(system.getCount() == 0);
  }

  SECTION("Add Component makes the types by id")
  {
    ComponentRegister::registerType<ProjectileTargetComponent>();
    GameObject& enemy = *scene.createGameObject("Enemy", nullptr);
    REQUIRE(enemy.addComponentByType(TypeTraits<ProjectileTargetComponent>::getTypeId()));
    REQUIRE(ProjectileTargetComponent::getTargets(scene).size() == 1);
  }
}
#endif // USING(CH_EDITOR)

/*
 * Hidden: run it by name in a Release build to measure a step with many projectiles.
 */
TEST_CASE("Projectiles - benchmark", "[Projectiles][.]")
{
  constexpr uint32 kProjectiles = 20000;
  constexpr uint32 kTargets = 200;
  constexpr uint32 kSteps = 300;

  Scene scene("Benchmark", chEngineSDK::UUID::createRandom());
  scene.setPlaying(true);
  ProjectileSystemComponent& system =
      scene.createGameObject("Gun", nullptr)->addComponent<ProjectileSystemComponent>();
  system.setCapacity(kProjectiles);
  system.setCellSize(1.0f);
  system.setDefaults({.lifetime = 1000.0f, .radius = 0.05f});
  for (uint32 i = 0; i < kTargets; ++i) {
    const float angle = static_cast<float>(i) * 1.8f;
    makeTarget(scene, "Target",
               Vector2(Math::cos(Degree(angle)) * 40.0f, Math::sin(Degree(angle)) * 40.0f), 1);
  }

  uint32 hits = 0;
  const auto start = std::chrono::steady_clock::now();
  for (uint32 step = 0; step < kSteps; ++step) {
    // Keep the system full: what died is fired again from the middle.
    const uint32 missing = kProjectiles - system.getCount();
    system.fireRing(Vector2::ZERO, missing, static_cast<float>(step) * 7.0f, 8.0f);
    scene.fixedUpdate(kStep);
    hits += static_cast<uint32>(system.getLastHits().size());
  }
  const double milliseconds =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
          .count();
  WARN("Step with " << kProjectiles << " projectiles and " << kTargets << " targets (moved, "
                    << "hit and drawn): " << milliseconds / kSteps << " ms; hits " << hits);
  REQUIRE(hits > 0);
}
