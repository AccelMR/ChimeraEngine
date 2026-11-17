/************************************************************************/
/**
 * @file chParticlesTestMain.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Tests of the particles plugin, built with its sources.
 */
/************************************************************************/
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <bit>
#include <chrono>

#include "chAgeMotion.h"
#include "chFileStream.h"
#include "chFileSystem.h"
#include "chFloat16.h"
#include "chFloatingNumberBindings.h"
#include "chFloatingNumberSystemComponent.h"
#include "chFont.h"
#include "chGpuAgeRing.h"
#include "chGameObject.h"
#include "chLogger.h"
#include "chMath.h"
#include "chParticleBindings.h"
#include "chParticleSystemComponent.h"
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
    ScriptManager::addBindings(&ParticleBindings::registerBindings);
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
struct TestParticles
{
  Scene scene{"Particles", chEngineSDK::UUID::createRandom()};
  SPtr<GameObject> object = scene.createGameObject("Particles", nullptr);
  ParticleSystemComponent& system = object->addComponent<ParticleSystemComponent>();
};

/*
 * A motion with no randomness, so particles are where the test expects.
 */
NODISCARD AgeMotion
makeFixedMotion()
{
  return {.angle = 90.0f, .spread = 0.0f, .speed = 2.0f, .speedVariance = 0.0f,
          .gravity = Vector2::ZERO, .lifetime = 1.0f, .lifetimeVariance = 0.0f};
}
} // namespace

TEST_CASE("Particles - age ring")
{
  GpuAgeRing<uint32> ring;
  ring.create(4);
  REQUIRE(ring.getCapacity() == 4);
  REQUIRE(ring.getBuffer() == nullptr);

  auto collectRanges = [&ring]() {
    Vector<uint32> ranges;
    ring.forEachLiveRange([&ranges](uint32 first, uint32 count) {
      ranges.push_back(first);
      ranges.push_back(count);
    });
    return ranges;
  };

  SECTION("Entries die in order and the clock moves")
  {
    REQUIRE(ring.write(10, 1.0f) == 0);
    REQUIRE(ring.write(11, 2.0f) == 1);
    REQUIRE(ring.advance(0.5f) == 2);
    REQUIRE(ring.getTime() == Approx(0.5f));
    REQUIRE(ring.advance(0.5f) == 1);
    REQUIRE(ring.getLiveEntry(0) == 11);
    REQUIRE(collectRanges() == Vector<uint32>{1, 1});
    REQUIRE(ring.advance(1.0f) == 0);
    REQUIRE(collectRanges().empty());
  }

  SECTION("A full ring writes over the oldest and is drawn in two ranges")
  {
    for (uint32 i = 0; i < 6; ++i) {
      ring.write(i, 10.0f);
    }
    REQUIRE(ring.getLiveCount() == 4);
    REQUIRE(ring.getWriteCursor() == 2);
    REQUIRE(ring.getLiveEntry(0) == 2);
    REQUIRE(ring.getLiveEntry(3) == 5);
    REQUIRE(collectRanges() == Vector<uint32>{2, 2, 0, 2});
    ring.upload();
  }

  SECTION("Clear keeps the clock")
  {
    ring.write(1, 1.0f);
    CH_PARAMETER_UNUSED(ring.advance(0.25f));
    ring.clear();
    REQUIRE(ring.getLiveCount() == 0);
    REQUIRE(ring.getWriteCursor() == 0);
    REQUIRE(ring.getTime() == Approx(0.25f));
  }
}

TEST_CASE("Particles - age motion")
{
  const Random random(7);
  const AgeMotion straight{.angle = 0.0f, .spread = 0.0f, .speed = 3.0f,
                           .speedVariance = 0.0f, .lifetime = 2.0f, .lifetimeVariance = 0.0f};
  const AgeBirth birth = straight.makeBirth(random);
  // 0 degrees is +Y.
  REQUIRE(birth.velocity.x == Approx(3.0f));
  REQUIRE(birth.velocity.y == Approx(0.0f).margin(1e-4));
  REQUIRE(birth.lifetime == Approx(2.0f));
  REQUIRE(birth.seed <= 0xFFFFu);
  REQUIRE(lowHalf(birth.getPackedVelocity()) == Approx(3.0f));
  REQUIRE(highHalf(birth.getPackedVelocity()) == Approx(0.0f).margin(1e-3));

  const AgeMotion wide{.angle = 90.0f, .spread = 30.0f, .speed = 5.0f, .speedVariance = 1.0f,
                       .lifetime = 1.0f, .lifetimeVariance = 0.5f};
  for (uint32 i = 0; i < 100; ++i) {
    const AgeBirth spread = wide.makeBirth(random);
    const float speed = spread.velocity.magnitude();
    REQUIRE(speed >= 4.0f - 1e-3f);
    REQUIRE(speed <= 6.0f + 1e-3f);
    // Within 30 degrees of up.
    REQUIRE(spread.velocity.y >= speed * 0.866f - 1e-3f);
    REQUIRE(spread.lifetime >= 0.5f - 1e-5f);
    REQUIRE(spread.lifetime <= 1.5f + 1e-5f);
  }

  const AgeMotion clamped = AgeMotion{.lifetime = 0.0f, .lifetimeVariance = 3.0f}.getClamped();
  REQUIRE(clamped.lifetime == Approx(AgeMotion::MIN_LIFETIME));
  REQUIRE(clamped.lifetimeVariance == Approx(AgeMotion::MIN_LIFETIME));

  const GpuAgeMotion gpu = AgeMotion{.gravity = Vector2(1.0f, -2.0f), .drag = -1.0f}.toGpu();
  REQUIRE(gpu.gravity[1] == Approx(-2.0f));
  REQUIRE(gpu.drag == 0.0f);
}

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

TEST_CASE("Particles - emission")
{
  TestParticles test;
  ParticleSystemComponent& system = test.system;
  system.setMotion(makeFixedMotion());
  system.setEmission({.rate = 10.0f, .duration = 1.0f, .bLoop = true, .bPlayOnStart = true});
  test.object->getTransform().setLocalPosition(Vector3(0.0f, 3.0f, 4.0f));
  test.scene.updateTransforms();

  SECTION("Nothing plays while the scene is edited, until play")
  {
    system.update(0.5f);
    REQUIRE_FALSE(system.isPlaying());
    REQUIRE(system.getLiveCount() == 0);
    system.play();
    system.update(0.5f);
    REQUIRE(system.getLiveCount() == 5);
  }

  SECTION("Play on start runs when the scene plays; rate and lifetime")
  {
    test.scene.setPlaying(true);
    system.update(0.25f);
    REQUIRE(system.isPlaying());
    // 2.5 particles: two now, the half waits.
    REQUIRE(system.getLiveCount() == 2);
    system.update(0.25f);
    REQUIRE(system.getLiveCount() == 5);
    system.update(0.6f);
    REQUIRE(system.getLiveCount() == 11);
    // The first two, born at 0.25 s, die at 1.25 s while two more are born.
    system.update(0.2f);
    REQUIRE(system.getLiveCount() == 11);
  }

  SECTION("World space places and turns particles once")
  {
    REQUIRE(system.emit(1) == 1);
    const GpuParticle& particle = system.getParticles()[0];
    REQUIRE(particle.position[0] == Approx(3.0f));
    REQUIRE(particle.position[1] == Approx(4.0f));
    // 90 degrees is up: +Z.
    REQUIRE(lowHalf(particle.velocity) == Approx(0.0f).margin(1e-3));
    REQUIRE(highHalf(particle.velocity) == Approx(2.0f));
    REQUIRE(particle.lifetime == Approx(1.0f));
  }

  SECTION("Local space keeps particles from the emitter")
  {
    ParticleEmission emission = system.getEmission();
    emission.bLocalSpace = true;
    system.setEmission(emission);
    REQUIRE(system.emit(1) == 1);
    REQUIRE(system.getParticles()[0].position[0] == Approx(0.0f));
    REQUIRE(system.emitAt(1, Vector2(5.0f, 4.0f)) == 1);
    REQUIRE(system.getParticles()[1].position[0] == Approx(2.0f));
    REQUIRE(system.getParticles()[1].position[1] == Approx(0.0f).margin(1e-5));
  }

  SECTION("A turned emitter turns the shape and the direction")
  {
    test.object->getTransform().setLocalRotation(Quaternion(Vector3::FORWARD, Degree(90.0f)));
    test.scene.updateTransforms();
    ParticleEmission emission = system.getEmission();
    emission.shape = ParticleShape::Line;
    emission.shapeSize = Vector2(1.0f, 0.0f);
    system.setEmission(emission);
    REQUIRE(system.emit(20) == 20);
    for (const GpuParticle& particle : system.getParticles().first(20)) {
      // The line now runs along Z and the flight along Y.
      REQUIRE(particle.position[0] == Approx(3.0f).margin(1e-4));
      REQUIRE(Math::abs(particle.position[1] - 4.0f) <= 1.0f + 1e-4f);
      REQUIRE(Math::abs(lowHalf(particle.velocity)) == Approx(2.0f).margin(1e-2));
      REQUIRE(highHalf(particle.velocity) == Approx(0.0f).margin(1e-2));
    }
  }

  SECTION("emitAt tints and places in the world")
  {
    REQUIRE(system.emitAt(3, Vector2(-1.0f, 2.0f), LinearColor(1.0f, 0.5f, 0.0f, 1.0f)) == 3);
    REQUIRE(system.getParticles()[2].position[0] == Approx(-1.0f));
    REQUIRE(lowHalf(system.getParticles()[2].tintRG) == Approx(1.0f));
    REQUIRE(highHalf(system.getParticles()[2].tintRG) == Approx(0.5f));
  }

  SECTION("Bursts per cycle, and a cycle that does not loop ends")
  {
    system.setEmission({.rate = 0.0f, .duration = 1.0f, .bLoop = false});
    const ParticleBurst bursts[] = {{.time = 0.0f, .count = 10},
                                    {.time = 0.5f, .count = 3, .cycles = 2, .interval = 0.2f}};
    system.setBursts(bursts);
    system.play();
    system.update(0.1f);
    REQUIRE(system.getLiveCount() == 10);
    system.update(0.5f);
    REQUIRE(system.getLiveCount() == 13);
    system.update(0.2f);
    REQUIRE(system.getLiveCount() == 16);
    system.update(0.3f);
    REQUIRE_FALSE(system.isPlaying());
  }

  SECTION("Bursts come back when the cycle loops")
  {
    system.setEmission({.rate = 0.0f, .duration = 0.5f, .bLoop = true});
    const ParticleBurst bursts[] = {{.time = 0.0f, .count = 4}};
    system.setBursts(bursts);
    system.play();
    system.update(0.1f);
    REQUIRE(system.getLiveCount() == 4);
    system.update(0.45f);
    REQUIRE(system.getLiveCount() == 8);
  }

  SECTION("Stop keeps the particles unless asked; a full ring writes over the oldest")
  {
    system.setCapacity(8);
    REQUIRE(system.emit(12) == 12);
    REQUIRE(system.getLiveCount() == 8);
    system.play();
    system.stop();
    REQUIRE(system.getLiveCount() == 8);
    system.stop(true);
    REQUIRE(system.getLiveCount() == 0);
  }
}

TEST_CASE("Particles - look")
{
  TestParticles test;
  ParticleSystemComponent& system = test.system;
  const ParticleColorKey colors[] = {{.time = 1.0f, .color = LinearColor::TRANSPARENT_BLACK},
                                     {.time = 0.0f, .color = LinearColor::WHITE}};
  system.setColorGradient(colors);
  REQUIRE(system.getColorGradient()[0].time == 0.0f);
  const ParticleSizeKey sizes[] = {{.time = 0.0f, .size = 0.5f}, {.time = 1.0f, .size = 2.0f}};
  system.setSizeCurve(sizes);
  ParticleLook look;
  look.size = Vector2(0.3f, 0.6f);
  look.rotationMode = ParticleRotation::FaceVelocity;
  look.spin = 180.0f;
  look.depth = 2.0f;
  system.setLook(look);
  system.setMotion({.gravity = Vector2(0.0f, -5.0f), .drag = 1.5f});
  REQUIRE(system.emit(1) == 1);
  system.update(0.01f);

  const GpuParticleLook& gpu = system.getGpuLook();
  REQUIRE(gpu.motion.gravity[1] == Approx(-5.0f));
  REQUIRE(gpu.motion.drag == Approx(1.5f));
  REQUIRE(gpu.colorKeyCount == 2);
  REQUIRE(gpu.colorKeys[1][3] == 0.0f);
  REQUIRE(gpu.sizeKeyCount == 2);
  REQUIRE(gpu.sizeKeys[1] == Approx(2.0f));
  REQUIRE(gpu.size[1] == Approx(0.6f));
  REQUIRE(gpu.spin == Approx(Math::PI));
  REQUIRE((gpu.flags & GpuParticleLook::FACE_VELOCITY_BIT) != 0);
  REQUIRE(gpu.depth == Approx(2.0f));
  // Without a flipbook the whole look is frame 0.
  REQUIRE(gpu.frameCount == 1);
  REQUIRE(gpu.frames[0][2] == Approx(1.0f));
  // No texture and the Linear filter.
  REQUIRE(gpu.textureAndFlags == GpuSpriteInstance::NO_TEXTURE);
}

TEST_CASE("Particles - saving")
{
  TestParticles test;
  ParticleSystemComponent& system = test.system;
  system.setCapacity(777);
  system.setEmission({.rate = 33.0f, .duration = 4.0f, .bLoop = false, .bPlayOnStart = false,
                      .bLocalSpace = true, .shape = ParticleShape::CircleEdge,
                      .shapeSize = Vector2(2.0f, 1.0f)});
  const ParticleBurst bursts[] = {{.time = 0.5f, .count = 40, .cycles = 3, .interval = 0.25f}};
  system.setBursts(bursts);
  system.setMotion({.angle = 45.0f, .speed = 7.0f, .gravity = Vector2(1.0f, -3.0f)});
  ParticleLook look;
  look.texture = chEngineSDK::UUID::createRandom();
  look.size = Vector2(0.5f, 0.25f);
  look.color2 = LinearColor(1.0f, 0.0f, 0.0f, 1.0f);
  look.spinVariance = 90.0f;
  look.sortingLayer = 4;
  look.blendMode = SpriteBlendMode::Additive;
  look.filter = SpriteFilter::Point;
  system.setLook(look);
  system.setFlipbook({.atlas = chEngineSDK::UUID::createRandom(), .firstFrame = 2,
                      .frameCount = 4, .framesPerSecond = 12.0f});
  const ParticleColorKey colors[] = {{.time = 0.0f},
                                     {.time = 0.7f, .color = LinearColor::BLACK}};
  system.setColorGradient(colors);
  const ParticleSizeKey sizes[] = {{.time = 0.3f, .size = 3.0f}};
  system.setSizeCurve(sizes);

  MemoryDataStream stream;
  system.serialize(stream);
  stream.seek(0);
  ParticleSystemComponent loaded;
  REQUIRE(loaded.deserialize(stream));
  REQUIRE(loaded.getCapacity() == 777);
  REQUIRE(loaded.getEmission().rate == Approx(33.0f));
  REQUIRE_FALSE(loaded.getEmission().bLoop);
  REQUIRE(loaded.getEmission().bLocalSpace);
  REQUIRE(loaded.getEmission().shape == ParticleShape::CircleEdge);
  REQUIRE(loaded.getEmission().shapeSize.x == Approx(2.0f));
  REQUIRE(loaded.getBursts().size() == 1);
  REQUIRE(loaded.getBursts()[0].cycles == 3);
  REQUIRE(loaded.getMotion().angle == Approx(45.0f));
  REQUIRE(loaded.getLook().texture == look.texture);
  REQUIRE(loaded.getLook().size.y == Approx(0.25f));
  REQUIRE(loaded.getLook().color2.g == 0.0f);
  REQUIRE(loaded.getLook().spinVariance == Approx(90.0f));
  REQUIRE(loaded.getLook().sortingLayer == 4);
  REQUIRE(loaded.getLook().blendMode == SpriteBlendMode::Additive);
  REQUIRE(loaded.getLook().filter == SpriteFilter::Point);
  REQUIRE(loaded.getFlipbook().firstFrame == 2);
  REQUIRE(loaded.getFlipbook().framesPerSecond == Approx(12.0f));
  REQUIRE(loaded.getColorGradient().size() == 2);
  REQUIRE(loaded.getColorGradient()[1].time == Approx(0.7f));
  REQUIRE(loaded.getSizeCurve()[0].size == Approx(3.0f));
  Vector<chEngineSDK::UUID> references;
  loaded.collectReferences(references);
  REQUIRE(references.size() == 2);

  Vector<uint8> bytes(stream.size() - 4);
  stream.seek(0);
  REQUIRE(stream.read(bytes.data(), bytes.size()) == bytes.size());
  MemoryDataStream cut;
  REQUIRE(cut.write(bytes.data(), bytes.size()) == bytes.size());
  cut.seek(0);
  ParticleSystemComponent broken;
  REQUIRE_FALSE(broken.deserialize(cut));
}

TEST_CASE("Particles - scripts")
{
  ScriptManager& scripts = getScripts();
  Scene scene("Scripts", chEngineSDK::UUID::createRandom());
  struct SceneGuard
  {
    ScriptManager& scripts;
    ~SceneGuard() { scripts.setCurrentScene(nullptr); }
  } guard{scripts};
  scripts.setCurrentScene(&scene);
  const SPtr<GameObject> object = scene.createGameObject("Sparks", nullptr);
  REQUIRE(scripts.runString(R"lua(
    local object = Scene.find('Sparks')
    assert(object:getParticles() == nil)
    local sparks = object:addParticles()
    assert(sparks == object:getParticles())
    assert(sparks:getGameObject() == object)
    sparks:configure{ capacity = 64, rate = 0, shape = "Circle", shapeSize = 0.5,
                      size = Vector2(0.1, 0.2), minScale = 0.5, maxScale = 1.5,
                      color = Color(1, 1, 0, 1), color2 = Color(1, 0, 0, 1),
                      faceVelocity = true, blend = "Additive", layer = 3,
                      angle = 0, spread = 180, speed = 4, gravity = Vector2(0, -9),
                      lifetime = 0.5, localSpace = false, loop = false }
    sparks:setColorGradient{ {0, Color(1, 1, 1, 1)}, {1, Color(1, 1, 1, 0)} }
    sparks:setSizeCurve{ {0, 1}, {1, 0.2} }
    sparks:setBursts{ { time = 0, count = 5 }, { time = 0.1, count = 2, cycles = 2,
                        interval = 0.05 } }
    assert(sparks:emit(10) == 10)
    assert(sparks:emit(3, 1, 2, Color(0, 1, 0, 1)) == 3)
    sparks:play()
    assert(sparks:isPlaying())
    sparks:stop()
    assert(not sparks:isPlaying())
    assert(sparks:getCount() == 13)
    assert(sparks:getCapacity() == 64)
    assert(tostring(sparks) == "Particles(Sparks)")
  )lua"));
  ParticleSystemComponent* system = object->getComponent<ParticleSystemComponent>();
  REQUIRE(system != nullptr);
  REQUIRE(system->getLook().sortingLayer == 3);
  REQUIRE(system->getLook().blendMode == SpriteBlendMode::Additive);
  REQUIRE(system->getLook().rotationMode == ParticleRotation::FaceVelocity);
  REQUIRE(system->getEmission().shape == ParticleShape::Circle);
  REQUIRE(system->getEmission().shapeSize.y == Approx(0.5f));
  REQUIRE(system->getColorGradient().size() == 2);
  REQUIRE(system->getSizeCurve()[1].size == Approx(0.2f));
  REQUIRE(system->getBursts().size() == 2);
  REQUIRE(system->getParticles()[12].position[0] == Approx(1.0f));

  REQUIRE_FALSE(scripts.runString("Scene.find('Sparks'):getParticles():configure{ rat = 1 }"));
  REQUIRE_FALSE(
      scripts.runString("Scene.find('Sparks'):getParticles():configure{ shape = 'Star' }"));
  REQUIRE_FALSE(
      scripts.runString("Scene.find('Sparks'):getParticles():setBursts{ { cuont = 1 } }"));
  REQUIRE_FALSE(scripts.runString("Scene.find('Sparks'):getParticles():setSizeCurve{ {0, 1}, "
                                  "{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, "
                                  "{0, 1} }"));
}

TEST_CASE("Particles - benchmark", "[.]")
{
  TestParticles test;
  ParticleSystemComponent& system = test.system;
  constexpr uint32 kCount = 200000;
  system.setCapacity(kCount);
  system.setEmission({.shape = ParticleShape::Circle});
  system.update(0.0f);
  const auto start = std::chrono::steady_clock::now();
  CH_PARAMETER_UNUSED(system.emit(kCount));
  const auto emitted = std::chrono::steady_clock::now();
  system.update(1.0f / 60.0f);
  const auto updated = std::chrono::steady_clock::now();
  using Milliseconds = std::chrono::duration<double, std::milli>;
  CH_LOG_INFO(FloatingNumbersTestLog,
              "{0} particles: {1:.3f} us each to emit; the update after {2:.3f} ms", kCount,
              Milliseconds(emitted - start).count() / kCount * 1000.0,
              Milliseconds(updated - emitted).count());
  REQUIRE(system.getLiveCount() == kCount);
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
