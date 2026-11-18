/************************************************************************/
/**
 * @file chScriptRuntimeTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of the Lua allocator, the binder, plugin bindings and the definitions.
 */
/************************************************************************/
#include <chrono>
#include <cstring>

#include "chAngle.h"
#include "chAssetMeta.h"
#include "chGraphicsTypes.h"
#include "chMath.h"
#include "chQuaternion.h"
#include "chRotator.h"
#include "chScene.h"
#include "chScriptAllocator.h"
#include "chScriptApi.h"
#include "chScriptAsset.h"
#include "chScriptBinder.h"
#include "chScriptManager.h"
#include "chScriptMathTypes.h"
#include "chScriptObjectTypes.h"
#include "chStringUtils.h"
#include "chUUID.h"
#include "chVector2.h"
#include "chVector3.h"

#include "catch.hpp"

using namespace chEngineSDK;

namespace {
CH_LOG_DECLARE_STATIC(ScriptTestLog, All);

} // namespace

TEST_CASE("chCore - Script allocator", "[Scripting]")
{
  for (const bool bUsePools : {true, false}) {
    ScriptAllocator allocator(bUsePools);
    void* block = ScriptAllocator::allocate(&allocator, nullptr, 0, 24);
    REQUIRE(block != nullptr);
    REQUIRE(reinterpret_cast<SIZE_T>(block) % 16 == 0);
    std::memset(block, 0xAB, 24);
    REQUIRE(allocator.getBytesInUse() == 24);

    // Grows inside its size class, then into another one and past the pools, keeping the
    // bytes it had.
    block = ScriptAllocator::allocate(&allocator, block, 24, 30);
    block = ScriptAllocator::allocate(&allocator, block, 30, 200);
    REQUIRE(static_cast<uint8*>(block)[23] == 0xAB);
    block = ScriptAllocator::allocate(&allocator, block, 200, 1000);
    REQUIRE(static_cast<uint8*>(block)[0] == 0xAB);
    REQUIRE(allocator.getBytesInUse() == 1000);
    block = ScriptAllocator::allocate(&allocator, block, 1000, 16);
    REQUIRE(static_cast<uint8*>(block)[15] == 0xAB);

    REQUIRE(ScriptAllocator::allocate(&allocator, block, 16, 0) == nullptr);
    REQUIRE(allocator.getBytesInUse() == 0);

    if (bUsePools) {
      // A freed block is the next one given in its size class.
      void* const first = ScriptAllocator::allocate(&allocator, nullptr, 0, 40);
      REQUIRE(ScriptAllocator::allocate(&allocator, first, 40, 0) == nullptr);
      void* const second = ScriptAllocator::allocate(&allocator, nullptr, 0, 33);
      REQUIRE(second == first);
      REQUIRE(ScriptAllocator::allocate(&allocator, second, 33, 0) == nullptr);
    }
  }
}

namespace {
int32
pluginGreeting(lua_State* state)
{
  lua_pushliteral(state, "hello");
  return 1;
}

/*
 * What a plugin's loadPlugin hands to ScriptManager::addBindings.
 */
void
registerPluginBindings(lua_State* state)
{
  ScriptClass<Vector2>(state).rawMethod("greet", &pluginGreeting, "fun(): string");
  lua_pushinteger(state, 7);
  lua_setglobal(state, "PluginValue");
}
} // namespace

TEST_CASE("chCore - Script plugin bindings", "[Scripting]")
{
  SECTION("Bindings added before a state is made reach it, after the engine's")
  {
    ScriptManager::addBindings(&registerPluginBindings);
    ScriptManager::addBindings(&registerPluginBindings);
    ScriptManager scripts(true);
    ScriptManager::removeBindings(&registerPluginBindings);
    REQUIRE(scripts.runString(R"lua(
      assert(PluginValue == 7)
      -- The class grew: the engine's members, constructors and constants are still there.
      local v = Vector2(3, 4)
      assert(v:greet() == "hello")
      assert(v.x == 3 and v:length() == 5)
      assert(Vector2.ZERO.x == 0)
    )lua"));

    ScriptManager later(true);
    REQUIRE(later.runString("assert(PluginValue == nil)"));
    REQUIRE_FALSE(later.runString("Vector2(1, 2):greet()"));
  }
}

TEST_CASE("chCore - Script binder", "[Scripting]")
{
  ScriptManager scripts(true);

  SECTION("Vectors")
  {
    REQUIRE(scripts.runString(R"lua(
      local v = Vector3(1, 2, 3)
      assert(v.x == 1 and v.y == 2 and v.z == 3)
      v.y = 5
      assert(v.y == 5)
      local zero = Vector3()
      assert(zero.x == 0 and zero.y == 0 and zero.z == 0)

      local sum = Vector3(1, 0, 0) + Vector3(0, 2, 0)
      assert(sum == Vector3(1, 2, 0))
      assert(Vector3(2, 4, 6) * 0.5 == Vector3(1, 2, 3))
      assert(2 * Vector3(1, 2, 3) == Vector3(2, 4, 6))
      assert(-Vector3(1, 2, 3) == Vector3(-1, -2, -3))
      assert(Vector3(2, 4, 6) / 2 == Vector3(1, 2, 3))
      assert(Vector3(3, 4, 0):length() == 5)
      assert(Vector3(1, 0, 0):dot(Vector3(0, 1, 0)) == 0)
      assert(Vector3(1, 0, 0):cross(Vector3(0, 1, 0)) == Vector3(0, 0, 1))
      assert(Vector3(0, 0, 10):normalized() == Vector3(0, 0, 1))
      assert(tostring(Vector3(1, 2, 3)) == "Vector3(1.000, 2.000, 3.000)")
      assert(Vector3(1, 2, 3) ~= Vector3(1, 2, 4))
      assert(Vector3(1, 2, 3) ~= "Vector3")

      local function close(a, b) return (a - b):length() < 1e-3 end
      assert(Vector2(3, 4):lengthSquared() == 25)
      assert(math.abs(Vector2(0, 2):angle() - 90) < 1e-3)
      assert(math.abs(Vector2(-1, -1):angle() - -135) < 1e-3)
      assert(math.abs(Vector2(1, 0):angleTo(Vector2(0, -1)) - -90) < 1e-3)
      assert(close(Vector2(2, 0):rotated(90), Vector2(0, 2)))
      local r = Vector2(1, 0)
      assert(rawequal(r:rotate(180), r) and close(r, Vector2(-1, 0)))
      assert(close(Vector2.FromAngle(-90), Vector2(0, -1)))
    )lua"));
  }

  SECTION("In-place methods change the value and return it")
  {
    REQUIRE(scripts.runString(R"lua(
      local v = Vector3(1, 1, 1)
      local same = v:add(Vector3(1, 0, 0)):scale(2)
      assert(rawequal(same, v))
      assert(v == Vector3(4, 2, 2))
      v:subVectors(Vector3(5, 5, 5), Vector3(1, 2, 3)):normalize()
      assert(v:nearEqual(Vector3(4, 3, 2):normalized()))
      v:set(3, 4, 0):clampLength(1)
      assert(v:nearEqual(Vector3(0.6, 0.8, 0)))
      local c = v:copy()
      c.x = 100
      assert(v.x ~= 100)
    )lua"));
  }

  SECTION("Constants are copies")
  {
    REQUIRE(scripts.runString(R"lua(
      local up = Vector3.UP
      up:scale(5)
      assert(Vector3.UP == Vector3(0, 0, 1))
      assert(not rawequal(Vector3.UP, Vector3.UP))
    )lua"));
  }

  SECTION("Rotations and colors")
  {
    REQUIRE(scripts.runString(R"lua(
      assert(Quaternion() == Quaternion.IDENTITY)
      local yaw = Quaternion.FromEuler(0, 90, 0)
      assert(yaw:rotateVector(Vector3.FORWARD):nearEqual(Vector3.RIGHT))
      local v = Vector3(1, 0, 0)
      v:rotate(yaw)
      assert(v:nearEqual(Vector3.RIGHT))
      local euler = yaw:toEuler()
      assert(math.abs(euler.yaw - 90) < 0.001 and math.abs(euler.pitch) < 0.001)
      assert((yaw * yaw):rotateVector(Vector3.FORWARD):nearEqual(Vector3.BACKWARD))
      local rotator = Rotator(10, 20, 30)
      rotator.yaw = 45
      assert(rotator.yaw == 45)
      assert(Color(1, 0, 0) == Color.RED)
      assert(Color.Lerp(Color.BLACK, Color.WHITE, 0.5) == Color(0.5, 0.5, 0.5, 1))
    )lua"));
  }

  SECTION("Wrong values are script errors, not crashes")
  {
    REQUIRE_FALSE(scripts.runString("local v = Vector3(1, 2, 3); return v:dot(5)"));
    REQUIRE_FALSE(scripts.runString("local v = Vector3(); return v.lenght"));
    REQUIRE_FALSE(scripts.runString("local v = Vector3(); v.w = 1"));
    REQUIRE_FALSE(scripts.runString("return Vector3(1, 2)"));
    REQUIRE_FALSE(scripts.runString("return Vector3.NOPE"));
    REQUIRE_FALSE(scripts.runString("local q = Quaternion(); return Vector3.dot(q, q)"));
    REQUIRE(scripts.runString("assert(getmetatable(Vector3()) == 'Vector3')"));
  }

  SECTION("Sandbox")
  {
    REQUIRE(scripts.runString(R"lua(
      assert(io == nil and os == nil and package == nil and debug == nil and require == nil)
      assert(load == nil and loadfile == nil and dofile == nil)
      assert(string and table and math and utf8 and coroutine)
      print("print reaches the log", 1, true)
      Log.info("Log reaches the log")
    )lua"));
    REQUIRE_FALSE(scripts.runString("this is not lua"));
  }

  SECTION("Time")
  {
    scripts.resetTime();
    scripts.beginFixedUpdate(0.5f);
    REQUIRE(scripts.runString("assert(Time.deltaTime == 0.5 and Time.fixedDeltaTime == 0.5)"));
    scripts.beginUpdate(0.25f);
    scripts.beginUpdate(0.25f);
    REQUIRE(scripts.runString(R"lua(
      assert(Time.deltaTime == 0.25 and Time.time == 0.5 and Time.frameCount == 2)
    )lua"));
  }
}

/*
 * Not run by default (ctest skips hidden tags); run it in Release:
 *   chCoreTest "chCore - Script steering benchmark"
 * Moves 1000 agents toward a target with seek steering, written three ways.
 */
TEST_CASE("chCore - Script steering benchmark", "[Scripting][.benchmark]")
{
  constexpr const ANSICHAR* kSetup = R"lua(
    N = 1000
    local maxSpeed, maxForce = 5.0, 10.0
    target = Vector3(0, 0, 0)
    opAgents, ipAgents, numAgents = {}, {}, {}
    for i = 1, N do
      local x, y = math.cos(i) * 50, math.sin(i) * 50
      opAgents[i] = { pos = Vector3(x, y, 0), vel = Vector3() }
      ipAgents[i] = { pos = Vector3(x, y, 0), vel = Vector3(), desired = Vector3(),
                      steer = Vector3() }
      numAgents[i] = { px = x, py = y, pz = 0, vx = 0, vy = 0, vz = 0 }
    end

    function stepOperators(dt)
      local t = target
      for i = 1, N do
        local a = opAgents[i]
        local steer = (t - a.pos):normalized() * maxSpeed - a.vel
        local length = steer:length()
        if length > maxForce then steer = steer * (maxForce / length) end
        a.vel = a.vel + steer * dt
        a.pos = a.pos + a.vel * dt
      end
    end

    function stepInPlace(dt)
      local t = target
      for i = 1, N do
        local a = ipAgents[i]
        a.desired:subVectors(t, a.pos):normalize():scale(maxSpeed)
        a.steer:subVectors(a.desired, a.vel):clampLength(maxForce)
        a.vel:addScaled(a.steer, dt)
        a.pos:addScaled(a.vel, dt)
      end
    end

    function stepNumbers(dt)
      local tx, ty, tz = target.x, target.y, target.z
      local sqrt = math.sqrt
      for i = 1, N do
        local a = numAgents[i]
        local dx, dy, dz = tx - a.px, ty - a.py, tz - a.pz
        local length = sqrt(dx * dx + dy * dy + dz * dz)
        if length > 1e-8 then
          local k = maxSpeed / length
          dx, dy, dz = dx * k, dy * k, dz * k
        end
        local sx, sy, sz = dx - a.vx, dy - a.vy, dz - a.vz
        local steerLength = sqrt(sx * sx + sy * sy + sz * sz)
        if steerLength > maxForce then
          local k = maxForce / steerLength
          sx, sy, sz = sx * k, sy * k, sz * k
        end
        a.vx, a.vy, a.vz = a.vx + sx * dt, a.vy + sy * dt, a.vz + sz * dt
        a.px, a.py, a.pz = a.px + a.vx * dt, a.py + a.vy * dt, a.pz + a.vz * dt
      end
    end
  )lua";
  constexpr uint32 kWarmUpFrames = 30;
  constexpr uint32 kFrames = 300;
  constexpr float kDeltaTime = 1.0f / 60.0f;

  for (const bool bPool : {true, false}) {
    ScriptManager scripts(bPool);
    REQUIRE(scripts.runString(kSetup));
    lua_State* const state = scripts.getState();

    for (const ANSICHAR* style : {"stepOperators", "stepInPlace", "stepNumbers"}) {
      const auto runFrame = [state, style]() {
        lua_getglobal(state, style);
        lua_pushnumber(state, static_cast<lua_Number>(kDeltaTime));
        lua_call(state, 1, 0);
      };
      for (uint32 frame = 0; frame < kWarmUpFrames; ++frame) {
        runFrame();
      }
      scripts.collectGarbage();
      const uint64 allocationsBefore = scripts.getAllocationCount();
      const auto start = std::chrono::steady_clock::now();
      for (uint32 frame = 0; frame < kFrames; ++frame) {
        runFrame();
      }
      const auto end = std::chrono::steady_clock::now();
      const double milliseconds =
          std::chrono::duration<double, std::milli>(end - start).count() / kFrames;
      const double allocations =
          static_cast<double>(scripts.getAllocationCount() - allocationsBefore) / kFrames;
      CH_LOG_INFO(ScriptTestLog, "{} with {}: {:.3f} ms per frame, {:.0f} allocations per frame",
                  style, bPool ? "pools" : "malloc", milliseconds, allocations);
    }

    // The three ways move the agents the same (float vectors against double numbers).
    REQUIRE(scripts.runString(R"lua(
      for i = 1, N, 97 do
        local a, b, c = opAgents[i], ipAgents[i], numAgents[i]
        assert(a.pos:nearEqual(b.pos))
        assert(math.abs(a.pos.x - c.px) < 1e-2 and math.abs(a.pos.y - c.py) < 1e-2)
      end
    )lua"));
  }

  // The same work in C++, for scale.
  struct Agent
  {
    Vector3 position;
    Vector3 velocity;
  };
  Vector<Agent> agents(1000);
  for (uint32 i = 0; i < agents.size(); ++i) {
    const Radian angle(static_cast<float>(i + 1));
    agents[i] = {Vector3(Math::cos(angle) * 50.0f, Math::sin(angle) * 50.0f, 0.0f),
                 Vector3::ZERO};
  }
  const auto start = std::chrono::steady_clock::now();
  for (uint32 frame = 0; frame < kWarmUpFrames + kFrames; ++frame) {
    for (Agent& agent : agents) {
      Vector3 steer =
          (Vector3::ZERO - agent.position).getNormalized() * 5.0f - agent.velocity;
      const float length = steer.magnitude();
      if (length > 10.0f) {
        steer *= 10.0f / length;
      }
      agent.velocity += steer * kDeltaTime;
      agent.position += agent.velocity * kDeltaTime;
    }
  }
  const auto end = std::chrono::steady_clock::now();
  CH_LOG_INFO(ScriptTestLog, "C++: {:.3f} ms per frame",
              std::chrono::duration<double, std::milli>(end - start).count() /
                  (kWarmUpFrames + kFrames));
  REQUIRE(agents[0].position.magnitude() < 50.0f);
}

TEST_CASE("chCore - Script definitions", "[Scripting]")
{
  ScriptManager scripts(true);
  const ScriptApiDescription* description = scripts.getApiDescription();
#if USING(CH_EDITOR)
  REQUIRE(description);
  const String text = description->writeDefinitions();
  const auto contains = [&text](const ANSICHAR* part) {
    INFO(part);
    CHECK(text.find(part) != String::npos);
  };

  SECTION("Value types")
  {
    contains("---@meta\n");
    contains("---@class Vector3\n");
    contains("---@field x number\n");
    contains("---@field UP Vector3\n");
    contains("---@operator add(Vector3): Vector3\n");
    contains("---@operator mul(number): Vector3\n");
    contains("---@operator unm: Vector3\n");
    contains("---@overload fun(): Vector3\n");
    contains("---@overload fun(x: number, y: number, z: number): Vector3\nVector3 = {}\n");
    contains("---@param other Vector3\n---@return number\nfunction Vector3:dot(other) end\n");
    contains("---@return Vector3\nfunction Vector3:normalize() end\n");
    contains("function Vector3.Lerp(a, b, alpha) end\n");
    contains("---@param degrees number\n---@return Quaternion\n"
             "function Quaternion.FromAxisAngle(axis, degrees) end\n");
    contains("---@field pitch number\n");
    contains("---@class Color\n");
  }

  SECTION("Handles and libraries")
  {
    contains("---@return string\nfunction GameObject:getName() end\n");
    contains("---@return GameObject?\nfunction GameObject:getParent() end\n");
    contains("---@param position Vector3\n"
             "---@overload fun(self: Transform, x: number, y: number, z: number)\n"
             "function Transform:setLocalPosition(position) end\n");
    contains("---@param out? Vector3\n---@return Vector3\n"
             "function Transform:getWorldPosition(out) end\n");
    contains("function Scene.find(name) end\n");
    contains("---@param player? integer\n---@return boolean\n"
             "function Input.isActionDown(action, player) end\n");
    contains("---@param callback fun(key: Key)\nfunction Input.captureNextKey(callback) end\n");
    contains("---@alias InputAction integer|string\n");
    contains("---@enum Key\nKey = {\n");
    contains("  W = ");
    contains("---@field deltaTime number\n");
    contains("---@param ... any\nfunction Log.info(...) end\n");
    contains("---@class Script\n---@field gameObject GameObject\n");
    CHECK(text.find("Script = {}") == String::npos);
  }

  SECTION("2D")
  {
    contains("---@class SpriteStyle\n");
    contains("---@class Texture\n");
    contains("---@class Atlas\n");
    contains("---@class Sprite\n");
    contains("---@class SpriteAnimator\n");
    contains("function Sprites.draw(look, x, y, rotation, scale, color) end\n");
    contains("function Sprites.makeStyle(options) end\n");
    contains("function SpriteAnimator:play(animation, restart) end\n");
    contains("---@return Sprite?\nfunction GameObject:getSprite() end\n");
  }

  SECTION("Every function has a signature and the file is Lua")
  {
    for (const UniquePtr<ScriptApiType>& type : description->getTypes()) {
      for (const ScriptApiFunction& function : type->functions) {
        INFO(type->name << "." << function.name);
        CHECK_FALSE(function.signatures.empty());
      }
    }
    lua_State* state = scripts.getState();
    REQUIRE(luaL_loadbufferx(state, text.data(), text.size(), "=definitions", "t") == LUA_OK);
    lua_pop(state, 1);
  }

  SECTION("Signatures")
  {
    ScriptApiSignature signature;
    REQUIRE(ScriptApiDescription::parseSignature(
        " fun(a: number, b?: fun(k: Key): string): boolean, integer ", signature));
    CHECK(signature.parameters == "a: number, b?: fun(k: Key): string");
    CHECK(signature.results == "boolean, integer");
    REQUIRE(ScriptApiDescription::parseSignature("fun()", signature));
    CHECK(signature.parameters.empty());
    CHECK(signature.results.empty());
    CHECK_FALSE(ScriptApiDescription::parseSignature("function(a)", signature));
    CHECK_FALSE(ScriptApiDescription::parseSignature("fun(a", signature));
    CHECK_FALSE(ScriptApiDescription::parseSignature("fun(a) number", signature));
  }

  SECTION("LuaLS configuration")
  {
    const String config = ScriptApiDescription::writeLuaLsConfig("Saved/ScriptDefinitions");
    CHECK(config.find("\"workspace.library\": [\"Saved/ScriptDefinitions\"]") !=
          String::npos);
    CHECK(config.find("\"io\": \"disable\"") != String::npos);
  }
#else
  CHECK_FALSE(description);
#endif // USING(CH_EDITOR)
}
