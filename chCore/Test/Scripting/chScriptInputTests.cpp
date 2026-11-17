/************************************************************************/
/**
 * @file chScriptInputTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of the Input table in scripts.
 */
/************************************************************************/
#include "chConfigFile.h"
#include "chInputManager.h"
#include "chScriptManager.h"
#include "chScriptMathTypes.h"
#include "chScriptObjectTypes.h"
#include "chVector2.h"

#include "catch.hpp"
#include "chScriptTestHelpers.h"

using namespace chEngineSDK;

TEST_CASE("chCore - Script input", "[Scripting]")
{
  ScriptManager& scripts = getTestScriptManager();
  // A local object, so every section starts clean; the module cannot start twice.
  InputManager input;
  struct InputScope
  {
    explicit InputScope(ScriptManager& inScripts, InputManager& localInput)
      : scripts(inScripts)
    {
      scripts.setInputManager(&localInput);
    }

    ~InputScope()
    {
      scripts.setInputManager(nullptr);
    }

    ScriptManager& scripts;
  } inputScope(scripts, input);

  ConfigFile defaults;
  defaults.parse("[Move]\nType=Axis2D\nKeys=W:+Y, S:-Y, D:+X, A:-X\n"
                 "[Jump]\nKeys=Space, GamepadSouth\n"
                 "[Zoom]\nType=Axis1D\nKeys=MouseWheel\n"
                 "[Pause]\nKeys=P\nContext=Menu\n");
  input.loadBindings(defaults, ConfigFile());

  const auto frame = [&input](std::initializer_list<DisplayEvent> events) {
    input.beginFrame();
    for (const DisplayEvent& event : events) {
      input.processEvent(event);
    }
    input.updateActions();
  };
  const auto key = [](InputKey inputKey, bool bDown) {
    return DisplayEvent(KeyEvent{.key = inputKey, .bDown = bDown, .modifiers = {}});
  };

  SECTION("Actions by id and by name")
  {
    frame({key(InputKey::W, true), key(InputKey::Space, true)});
    REQUIRE(scripts.runString(R"lua(
      local move = Input.findAction("move")
      local jump = Input.findAction("Jump")
      assert(move and jump and Input.findAction("Nope") == nil)
      assert(Input.getActionCount() == 4)
      assert(Input.getActionName(jump) == "Jump" and Input.getActionType(move) == "Axis2D")
      assert(Input.getActionContext("Pause") == "Menu")
      assert(Input.isActionDown(jump) and Input.wasActionPressed("Jump"))
      assert(Input.isActionDown(jump, 1) and not Input.isActionDown(jump, 2))
      assert(Input.getActionValue(jump) == 1)
      assert(Input.getActionValue2D(move) == Vector2(0, 1))

      local out = Vector2()
      assert(rawequal(Input.getActionValue2D(move, out), out) and out == Vector2(0, 1))
      assert(rawequal(Input.getActionValue2D(move, 1, out), out))
    )lua"));
    REQUIRE_FALSE(scripts.runString("Input.isActionDown('Nope')"));
    REQUIRE_FALSE(scripts.runString("Input.isActionDown(99)"));
    REQUIRE_FALSE(scripts.runString("Input.isActionDown('Jump', 5)"));

    frame({key(InputKey::Space, false)});
    REQUIRE(scripts.runString(R"lua(
      assert(not Input.isActionDown("Jump") and Input.wasActionReleased("Jump"))
    )lua"));
  }

  SECTION("Contexts")
  {
    REQUIRE(scripts.runString(R"lua(
      Input.setContextEnabled("Menu", false)
      assert(not Input.isContextEnabled("Menu") and Input.isContextEnabled("Default"))
    )lua"));
    frame({key(InputKey::P, true)});
    REQUIRE(scripts.runString("assert(not Input.isActionDown('Pause'))"));
  }

  SECTION("Keys read as up while game input is off")
  {
    frame({key(InputKey::A, true),
           DisplayEvent(MouseMoveEvent{.x = 5.0f, .y = 6.0f, .deltaX = 2.0f, .deltaY = 0.0f})});
    REQUIRE(scripts.runString(R"lua(
      assert(Key.A and Key.GamepadSouth and Key.Nope == nil)
      assert(Input.isKeyDown(Key.A) and Input.isKeyDown("a") and Input.wasKeyPressed(Key.A))
      assert(not Input.isKeyDown(Key.B))
      assert(Input.getKeyName(Key.A) == "A")
      assert(Input.getMouseDelta().x == 2 and Input.getMousePosition() == Vector2(5, 6))
      assert(Input.isGameInputEnabled())
    )lua"));
    REQUIRE_FALSE(scripts.runString("Input.isKeyDown('Banana')"));

    input.setGameInputEnabled(false);
    REQUIRE(scripts.runString(R"lua(
      assert(not Input.isKeyDown(Key.A) and not Input.wasKeyPressed(Key.A))
      assert(Input.getMouseDelta() == Vector2(0, 0) and Input.getAxis(Key.MouseX) == 0)
      assert(Input.getMousePosition() == Vector2(5, 6))
    )lua"));
    frame({});
    REQUIRE(scripts.runString("assert(not Input.isActionDown('Move'))"));
  }

  SECTION("Gamepads count from 1")
  {
    frame({DisplayEvent(GamepadConnectionEvent{
               .slot = 1, .bConnected = true, .type = GamepadType::PlayStation}),
           DisplayEvent(GamepadButtonEvent{
               .slot = 1, .button = InputKey::GamepadSouth, .bDown = true})});
    REQUIRE(scripts.runString(R"lua(
      assert(Input.isGamepadConnected(2) and not Input.isGamepadConnected(1))
      assert(Input.getGamepadType(2) == "PlayStation" and Input.getLastGamepad() == 2)
      assert(Input.isKeyDown(Key.GamepadSouth, 2) and not Input.isKeyDown(Key.GamepadSouth, 1))
      assert(Input.isActionDown("Jump"))
    )lua"));
    REQUIRE_FALSE(scripts.runString("Input.isGamepadConnected(9)"));
  }

  SECTION("Rebinding")
  {
    REQUIRE(scripts.runString(R"lua(
      local jump = Input.findAction("Jump")
      local keys = Input.getBindings(jump)
      assert(#keys == 2 and keys[1] == "Space" and keys[2] == "GamepadSouth")
      assert(Input.setBinding(jump, 1, "J"))
      assert(Input.setBinding(jump, 3, "K"))
      assert(#Input.getBindings(jump) == 3 and Input.getBindings(jump)[1] == "J")
      assert(not Input.setBinding("Move", 1, "W"))
      assert(Input.removeBinding(jump, 3) and not Input.removeBinding(jump, 9))
      local users = Input.findActionsUsingKey(Key.J)
      assert(#users == 1 and users[1] == jump)
      Input.resetBindings(jump)
      assert(Input.getBindings(jump)[1] == "Space")
      assert(#Input.getDefaultBindings("Move") == 4)
    )lua"));
    REQUIRE_FALSE(scripts.runString("Input.setBinding('Jump', 1, 'Banana')"));
    REQUIRE_FALSE(scripts.runString("Input.setBinding('Jump', 5, 'J')"));
  }

  SECTION("A captured key goes to the function only")
  {
    REQUIRE(scripts.runString(R"lua(
      Captured = nil
      Input.captureNextKey(function(key) Captured = key end)
      assert(Input.isCapturingKey())
    )lua"));
    frame({key(InputKey::Space, true)});
    REQUIRE(scripts.runString(R"lua(
      assert(Captured == Key.Space and not Input.isCapturingKey())
      assert(not Input.isActionDown("Jump"))
    )lua"));
    // Still held, the captured key does not reach its action until it is released.
    frame({});
    REQUIRE_FALSE(input.isActionDown(input.findAction("Jump")));
    frame({key(InputKey::Space, false)});
    frame({key(InputKey::Space, true)});
    REQUIRE(input.wasActionPressed(input.findAction("Jump")));

    // Stop resets the game state, which drops a capture still waiting.
    REQUIRE(scripts.runString(R"lua(
      Captured = nil
      Input.captureNextKey(function(key) Captured = key end)
    )lua"));
    input.resetGameState();
    frame({key(InputKey::J, true)});
    REQUIRE(scripts.runString("assert(Captured == nil and not Input.isCapturingKey())"));

    // An error in the function is logged and the capture is over.
    REQUIRE(scripts.runString("Input.captureNextKey(function() error('boom') end)"));
    frame({key(InputKey::K, true)});
    REQUIRE_FALSE(input.isCapturingKey());
  }

  SECTION("Cursor and players")
  {
    REQUIRE(scripts.runString(R"lua(
      Input.setCursorMode("Hidden")
      assert(Input.getCursorMode() == "Hidden")
      Input.setPlayerCount(2)
      Input.setPlayerDevices(2, false, {1})
      assert(Input.getPlayerCount() == 2)
    )lua"));
    REQUIRE_FALSE(scripts.runString("Input.setCursorMode('Sideways')"));
    REQUIRE_FALSE(scripts.runString("Input.setPlayerCount(9)"));
    input.setCursorMode(CursorMode::Normal);
  }
}
