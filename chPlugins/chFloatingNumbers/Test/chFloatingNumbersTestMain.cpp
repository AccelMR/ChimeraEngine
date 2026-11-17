/************************************************************************/
/**
 * @file chFloatingNumbersTestMain.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Tests of the floating numbers plugin, built with its sources.
 */
/************************************************************************/
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <bit>
#include <chrono>

#include "chFileStream.h"
#include "chFileSystem.h"
#include "chFloat16.h"
#include "chFloatingNumberBindings.h"
#include "chFloatingNumberSystemComponent.h"
#include "chFont.h"
#include "chGameObject.h"
#include "chLogger.h"
#include "chMath.h"
#include "chScene.h"
#include "chScriptManager.h"

#include "lua.h"

using namespace chEngineSDK;

CH_LOG_DECLARE_STATIC(FloatingNumbersTestLog, All);

namespace {
/*
 * Tests have no /Engine, so the default font comes from the source tree.
 */
void
useTestFont()
{
  static bool s_bSet = false;
  if (!s_bSet) {
    s_bSet = true;
    Vector<uint8> data =
        FileSystem::fastRead(FileSystem::absolutePath(Path(CH_TEST_FONT_FILE)));
    REQUIRE_FALSE(data.empty());
    Font::setDefault(chMakeShared<Font>(std::move(data), FontSettings{}));
  }
}

ScriptManager&
getScripts()
{
  if (!ScriptManager::isStarted()) {
    ScriptManager::addBindings(&FloatingNumberBindings::registerBindings);
    ScriptManager::startUp();
  }
  return ScriptManager::instance();
}

NODISCARD float
lowHalf(uint32 packed)
{
  return Float16::toFloat(static_cast<uint16>(packed & 0xFFFFu));
}

NODISCARD float
highHalf(uint32 packed)
{
  return Float16::toFloat(static_cast<uint16>(packed >> 16));
}

struct TestSystem
{
  Scene scene{"Numbers", chEngineSDK::UUID::createRandom()};
  SPtr<GameObject> object = scene.createGameObject("Numbers", nullptr);
  FloatingNumberSystemComponent& system =
      object->addComponent<FloatingNumberSystemComponent>();
};
} // namespace

TEST_CASE("Floating numbers - tiers")
{
  useTestFont();
  TestSystem test;
  FloatingNumberSystemComponent& system = test.system;
  REQUIRE(system.getTiers().size() == 1);

  // Given out of order; kept sorted.
  const FloatingNumberTier tiers[] = {{.minValue = 10000.0, .scale = 2.0f},
                                      {.minValue = 0.0},
                                      {.minValue = 1000.0, .scale = 1.5f}};
  system.setTiers(tiers);
  REQUIRE(system.getTiers().size() == 3);
  REQUIRE(system.getTiers()[1].minValue == 1000.0);
  REQUIRE(system.findTier(5.0) == 0);
  REQUIRE(system.findTier(1000.0) == 1);
  REQUIRE(system.findTier(9999.0) == 1);
  REQUIRE(system.findTier(1e9) == 2);

  system.update(0.0f);
  REQUIRE(system.spawn(1.0, Vector2::ZERO));
  system.update(0.0f);
  REQUIRE(system.getGpuMotion().tierCount == 3);
  REQUIRE(system.getGpuMotion().tiers[2].scale == Approx(2.0f));

  // More than the GPU holds are dropped.
  const Vector<FloatingNumberTier> many(12);
  system.setTiers(many);
  REQUIRE(system.getTiers().size() == GpuFloatingNumberMotion::MAX_TIERS);
  system.setTiers({});
  REQUIRE(system.getTiers().size() == 1);
}

TEST_CASE("Floating numbers - spawning")
{
  useTestFont();
  TestSystem test;
  FloatingNumberSystemComponent& system = test.system;
  system.setMotion({.angle = 90.0f, .spread = 0.0f, .speed = 4.0f, .speedVariance = 0.0f,
                    .lifetime = 1.0f, .lifetimeVariance = 0.0f});
  system.setNumberFormat({.notation = NumberNotation::Suffix, .decimals = 1});
  const FloatingNumberTier tiers[] = {{.minValue = 0.0},
                                      {.minValue = 1000.0, .outlineWidth = 2.0f}};
  system.setTiers(tiers);
  system.update(0.5f);

  SECTION("A number is one run that flies along the angle")
  {
    REQUIRE(system.spawn(12.0, Vector2(1.0f, 2.0f)));
    REQUIRE(system.getLiveCount() == 1);
    const GpuGlyphRun& run = system.getRuns()[0];
    REQUIRE(run.glyphCount == 2);
    REQUIRE(run.position.y == Approx(1.0f));
    REQUIRE(run.position.z == Approx(2.0f));
    REQUIRE(std::bit_cast<float>(run.userData[0]) == Approx(0.5f));
    REQUIRE(lowHalf(run.userData[1]) == Approx(0.0f).margin(1e-3f));
    REQUIRE(highHalf(run.userData[1]) == Approx(4.0f));
    REQUIRE(std::bit_cast<float>(run.userData[2]) == Approx(1.0f));
    REQUIRE((run.userData[3] & FloatingNumberGpu::TIER_MASK) == 0);
    REQUIRE((run.userData[3] & FloatingNumberGpu::OUTLINE_BIT) == 0);
  }

  SECTION("An outlined tier writes the outline first")
  {
    REQUIRE(system.spawn(1500.0, Vector2::ZERO));
    REQUIRE(system.getLiveCount() == 2);
    const GpuGlyphRun& outline = system.getRuns()[0];
    const GpuGlyphRun& fill = system.getRuns()[1];
    // "1.5K"
    REQUIRE(fill.glyphCount == 4);
    REQUIRE((outline.userData[3] & FloatingNumberGpu::OUTLINE_BIT) != 0);
    REQUIRE((fill.userData[3] & FloatingNumberGpu::TIER_MASK) == 1);
    REQUIRE(outline.userData[1] == fill.userData[1]);
    REQUIRE(((outline.textureAndFlags >> GpuSpriteInstance::EDGE_SHIFT) &
             GpuSpriteInstance::EDGE_MASK) > 0);
  }

  SECTION("Options and text")
  {
    REQUIRE(system.spawn(5.0, Vector2::ZERO, {.tier = 1, .tint = LinearColor(1, 0, 0, 1),
                                              .scale = 2.0f}));
    const GpuGlyphRun& fill = system.getRuns()[1];
    REQUIRE((fill.userData[3] & FloatingNumberGpu::TIER_MASK) == 1);
    REQUIRE(fill.size == Approx(system.getSize() * 2.0f));
    REQUIRE(highHalf(fill.colorRG) == Approx(0.0f));
    REQUIRE(system.spawnText("CRIT!", Vector2::ZERO));
    REQUIRE(system.getRuns()[2].glyphCount == 5);
  }

  SECTION("They die after their lifetime")
  {
    REQUIRE(system.spawn(1.0, Vector2::ZERO));
    system.update(0.5f);
    REQUIRE(system.getLiveCount() == 1);
    system.update(0.6f);
    REQUIRE(system.getLiveCount() == 0);
  }

  SECTION("A full ring writes over the oldest")
  {
    system.setCapacity(4);
    for (int32 i = 0; i < 6; ++i) {
      REQUIRE(system.spawn(static_cast<double>(i), Vector2::ZERO));
    }
    REQUIRE(system.getLiveCount() == 4);
    REQUIRE(system.getWriteCursor() == 2);
    system.clear();
    REQUIRE(system.getLiveCount() == 0);
  }
}

TEST_CASE("Floating numbers - saving")
{
  useTestFont();
  TestSystem test;
  FloatingNumberSystemComponent& system = test.system;
  system.setSize(0.7f);
  system.setCapacity(1234);
  system.setSortingLayer(5);
  system.setBlendMode(SpriteBlendMode::Additive);
  system.setMotion({.angle = 45.0f, .speed = 7.0f, .gravity = Vector2(1.0f, -3.0f)});
  system.setNumberFormat({.notation = NumberNotation::Scientific, .decimals = 3});
  const FloatingNumberTier tiers[] = {
      {.minValue = 0.0},
      {.minValue = 500.0, .color = LinearColor(1, 0.5f, 0, 1), .popScale = 2.0f,
       .shakeAmplitude = 0.1f, .shakeDuration = 0.3f, .flashDuration = 0.2f}};
  system.setTiers(tiers);

  MemoryDataStream stream;
  system.serialize(stream);
  stream.seek(0);
  FloatingNumberSystemComponent loaded;
  REQUIRE(loaded.deserialize(stream));
  REQUIRE(loaded.getSize() == Approx(0.7f));
  REQUIRE(loaded.getCapacity() == 1234);
  REQUIRE(loaded.getSortingLayer() == 5);
  REQUIRE(loaded.getBlendMode() == SpriteBlendMode::Additive);
  REQUIRE(loaded.getMotion().angle == Approx(45.0f));
  REQUIRE(loaded.getMotion().gravity.x == Approx(1.0f));
  REQUIRE(loaded.getNumberFormat().notation == NumberNotation::Scientific);
  REQUIRE(loaded.getTiers().size() == 2);
  REQUIRE(loaded.getTiers()[1].minValue == 500.0);
  REQUIRE(loaded.getTiers()[1].color.g == Approx(0.5f));
  REQUIRE(loaded.getTiers()[1].popScale == Approx(2.0f));
  REQUIRE(loaded.getTiers()[1].flashDuration == Approx(0.2f));

  Vector<uint8> bytes(stream.size() - 8);
  stream.seek(0);
  REQUIRE(stream.read(bytes.data(), bytes.size()) == bytes.size());
  MemoryDataStream cut;
  REQUIRE(cut.write(bytes.data(), bytes.size()) == bytes.size());
  cut.seek(0);
  FloatingNumberSystemComponent broken;
  REQUIRE_FALSE(broken.deserialize(cut));
}

TEST_CASE("Floating numbers - scripts")
{
  useTestFont();
  ScriptManager& scripts = getScripts();
  Scene scene("Scripts", chEngineSDK::UUID::createRandom());
  struct SceneGuard
  {
    ScriptManager& scripts;
    ~SceneGuard() { scripts.setCurrentScene(nullptr); }
  } guard{scripts};
  scripts.setCurrentScene(&scene);
  const SPtr<GameObject> object = scene.createGameObject("Damage", nullptr);

  REQUIRE(scripts.runString(R"lua(
    local object = Scene.find("Damage")
    assert(object:getFloatingNumbers() == nil)
    local numbers = object:addFloatingNumbers()
    assert(numbers == object:getFloatingNumbers())
    assert(numbers:getGameObject() == object)
    numbers:configure{ size = 0.5, capacity = 100, angle = 90, spread = 30, speed = 5,
                       gravity = Vector2(0, -12), lifetime = 0.8, notation = "Suffix",
                       decimals = 1, layer = 9 }
    numbers:setTiers{
      { min = 0, color = Color(1, 1, 1, 1) },
      { min = 1000, color = Color(1, 0.6, 0, 1), scale = 1.4, pop = 1.8, outline = 2 },
      { min = 100000, color = Color(1, 0.1, 0.1, 1), scale = 2, pop = 2.5, shake = 0.15,
        shakeDuration = 0.3, flash = 0.3, flashColor = Color(1, 1, 1, 1) },
    }
    assert(numbers:spawn(12, 0, 0))
    assert(numbers:spawn(25000, 1, 1))
    assert(numbers:spawn(3, 0, 0, { tier = 3, scale = 2 }))
    assert(numbers:spawnText("CRIT", 0, 0, { tier = 2 }))
    assert(numbers:getCapacity() == 100)
    Count = numbers:getCount()
  )lua"));
  FloatingNumberSystemComponent* system = object->getComponent<FloatingNumberSystemComponent>();
  REQUIRE(system != nullptr);
  REQUIRE(system->getTiers().size() == 3);
  REQUIRE(system->getSortingLayer() == 9);
  // The two tier 2 spawns are outlined: 12, outline and fill of 25000, the forced tier 3,
  // outline and fill of CRIT.
  REQUIRE(system->getLiveCount() == 6);
  REQUIRE((system->getRuns()[3].userData[3] & FloatingNumberGpu::TIER_MASK) == 2);
  REQUIRE((system->getRuns()[4].userData[3] & FloatingNumberGpu::OUTLINE_BIT) != 0);

  REQUIRE_FALSE(scripts.runString("Scene.find('Damage'):getFloatingNumbers():configure{ sped = 1 }"));
  REQUIRE_FALSE(scripts.runString("Scene.find('Damage'):getFloatingNumbers():setTiers{ { mni = 1 } }"));
  REQUIRE_FALSE(scripts.runString("Scene.find('Damage'):getFloatingNumbers():spawn(1, 0, 0, { tier = 9 })"));
}

TEST_CASE("Floating numbers - benchmark", "[.]")
{
  useTestFont();
  TestSystem test;
  FloatingNumberSystemComponent& system = test.system;
  constexpr uint32 kCount = 100000;
  system.setCapacity(kCount * 2);
  const FloatingNumberTier tiers[] = {{.minValue = 0.0}, {.minValue = 1000.0, .outlineWidth = 2.0f}};
  system.setTiers(tiers);
  system.update(0.0f);

  const auto start = std::chrono::steady_clock::now();
  for (uint32 i = 0; i < kCount; ++i) {
    CH_PARAMETER_UNUSED(system.spawn(static_cast<double>(i) * 7.3, Vector2::ZERO));
  }
  const auto spawned = std::chrono::steady_clock::now();
  system.update(1.0f / 60.0f);
  const auto updated = std::chrono::steady_clock::now();
  system.update(1.0f / 60.0f);
  const auto idle = std::chrono::steady_clock::now();
  using Milliseconds = std::chrono::duration<double, std::milli>;
  const double spawnMicroseconds = Milliseconds(spawned - start).count() / kCount * 1000.0;
  const double firstUpdate = Milliseconds(updated - spawned).count();
  const double nextUpdate = Milliseconds(idle - updated).count();
  CH_LOG_INFO(FloatingNumbersTestLog,
              "{0} spawns: {1:.3f} us each; the update that uploads them {2:.3f} ms, the "
              "next one {3:.3f} ms",
              kCount, spawnMicroseconds, firstUpdate, nextUpdate);
  REQUIRE(system.getLiveCount() > kCount);
}
