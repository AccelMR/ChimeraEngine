/************************************************************************/
/**
 * @file chInputTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of input keys, state, bindings, actions and key events.
 */
/************************************************************************/
#include "chConfigFile.h"
#include "chInputManager.h"
#include "chVector2.h"

#include "catch.hpp"

using namespace chEngineSDK;

TEST_CASE("chCore - Input keys", "[Input]")
{
  for (uint32 i = 1; i < InputKeyUtils::KEY_COUNT; ++i) {
    const InputKey key = static_cast<InputKey>(i);
    const StringView name = InputKeyUtils::getName(key);
    REQUIRE_FALSE(name.empty());
    REQUIRE(InputKeyUtils::fromName(name) == key);
    REQUIRE(InputKeyUtils::getDevice(key) != InputDevice::None);
    REQUIRE(InputKeyUtils::getType(key) != InputKeyType::None);
  }
  REQUIRE(InputKeyUtils::fromName("leftshift") == InputKey::LeftShift);
  REQUIRE(InputKeyUtils::fromName("NotAKey") == InputKey::None);
  REQUIRE(InputKeyUtils::fromName("") == InputKey::None);
  REQUIRE(InputKeyUtils::getName(InputKey::None).empty());

  REQUIRE(InputKeyUtils::getDevice(InputKey::Numpad9) == InputDevice::Keyboard);
  REQUIRE(InputKeyUtils::getDevice(InputKey::MouseWheelUp) == InputDevice::Mouse);
  REQUIRE(InputKeyUtils::getDevice(InputKey::GamepadSouth) == InputDevice::Gamepad);
  REQUIRE(InputKeyUtils::getType(InputKey::MouseWheelUp) == InputKeyType::Button);
  REQUIRE(InputKeyUtils::getType(InputKey::MouseY) == InputKeyType::Axis1D);
  REQUIRE(InputKeyUtils::getType(InputKey::MouseDelta) == InputKeyType::Axis2D);
  REQUIRE(InputKeyUtils::getType(InputKey::GamepadRightTrigger) == InputKeyType::Axis1D);
  REQUIRE(InputKeyUtils::getType(InputKey::GamepadLeftStick) == InputKeyType::Axis2D);
  REQUIRE(InputKeyUtils::getGamepadButton(0) == InputKey::GamepadSouth);
  REQUIRE(InputKeyUtils::getGamepadAxis(5) == InputKey::GamepadRightTrigger);
  REQUIRE(InputKeyUtils::GAMEPAD_BUTTON_COUNT == 21);
  REQUIRE(InputKeyUtils::GAMEPAD_AXIS_COUNT == 6);
}

TEST_CASE("chCore - Input state", "[Input]")
{
  InputState state;

  SECTION("Down, pressed and released by frame")
  {
    state.beginFrame();
    state.setKey(InputKey::W, true);
    REQUIRE(state.isDown(InputKey::W));
    REQUIRE(state.wasPressed(InputKey::W));
    REQUIRE_FALSE(state.wasReleased(InputKey::W));
    REQUIRE(state.getFirstPressedKey() == InputKey::W);

    state.beginFrame();
    state.setKey(InputKey::W, true);
    REQUIRE(state.isDown(InputKey::W));
    REQUIRE_FALSE(state.wasPressed(InputKey::W));
    REQUIRE(state.getFirstPressedKey() == InputKey::None);

    state.beginFrame();
    state.setKey(InputKey::W, false);
    REQUIRE_FALSE(state.isDown(InputKey::W));
    REQUIRE(state.wasReleased(InputKey::W));
  }

  SECTION("A tap inside one frame is pressed and released")
  {
    state.beginFrame();
    state.setKey(InputKey::Space, true);
    state.setKey(InputKey::Space, false);
    REQUIRE_FALSE(state.isDown(InputKey::Space));
    REQUIRE(state.wasPressed(InputKey::Space));
    REQUIRE(state.wasReleased(InputKey::Space));
  }

  SECTION("Fixed steps see a press once, also from a frame without a step")
  {
    state.beginFrame();
    state.setKey(InputKey::E, true);
    state.beginFrame();
    state.beginFixedStep();
    REQUIRE(state.wasPressed(InputKey::E));
    state.endFixedStep();
    state.beginFixedStep();
    REQUIRE_FALSE(state.wasPressed(InputKey::E));
    REQUIRE(state.isDown(InputKey::E));
    state.endFixedStep();
    REQUIRE_FALSE(state.isInFixedStep());
  }

  SECTION("Mouse movement and wheel")
  {
    state.beginFrame();
    state.addMouseMove(10.0f, 20.0f, 2.5f, 1.0f);
    state.addMouseMove(11.0f, 22.0f, 0.5f, 2.0f);
    state.addMouseWheel(0.0f, 0.25f);
    REQUIRE(state.getMousePosition() == Vector2(11.0f, 22.0f));
    REQUIRE(state.getMouseDelta() == Vector2(3.0f, 3.0f));
    // Axes point up, window positions down.
    REQUIRE(state.getAxis(InputKey::MouseY) == -3.0f);
    REQUIRE(state.getAxis2D(InputKey::MouseDelta) == Vector2(3.0f, -3.0f));
    REQUIRE(state.getAxis(InputKey::MouseWheel) == 0.25f);
    REQUIRE(state.wasPressed(InputKey::MouseWheelUp));
    REQUIRE(state.wasReleased(InputKey::MouseWheelUp));
    REQUIRE_FALSE(state.isDown(InputKey::MouseWheelUp));
    REQUIRE_FALSE(state.wasPressed(InputKey::MouseWheelDown));

    state.beginFrame();
    REQUIRE(state.getMouseDelta() == Vector2::ZERO);
    REQUIRE(state.getMouseWheel() == Vector2::ZERO);
    REQUIRE_FALSE(state.wasPressed(InputKey::MouseWheelUp));
  }

  SECTION("Gamepads")
  {
    state.setAnalogSettings(
        {.stickDeadZone = 0.2f, .triggerDeadZone = 0.1f, .buttonThreshold = 0.5f});
    state.beginFrame();
    state.setGamepadButton(0, InputKey::GamepadSouth, true);
    REQUIRE_FALSE(state.isDown(InputKey::GamepadSouth));

    state.setGamepadConnected(1, true, GamepadType::PlayStation);
    REQUIRE(state.getConnectedGamepads() == 0b10u);
    REQUIRE(state.getLastGamepad() == 1);
    state.setGamepadButton(1, InputKey::GamepadSouth, true);
    REQUIRE(state.isDown(InputKey::GamepadSouth));
    REQUIRE(state.wasPressed(InputKey::GamepadSouth));
    REQUIRE(state.isDown(InputKey::GamepadSouth, 0b10u));
    REQUIRE_FALSE(state.isDown(InputKey::GamepadSouth, 0b01u));

    // Inside the dead zone.
    state.setGamepadAxis(1, InputKey::GamepadLeftX, 0.1f);
    state.setGamepadAxis(1, InputKey::GamepadLeftY, 0.1f);
    REQUIRE(state.getAxis2D(InputKey::GamepadLeftStick) == Vector2::ZERO);

    // Past it the rest of the travel is stretched back to 0..1 along the same direction.
    state.setGamepadAxis(1, InputKey::GamepadLeftX, 0.6f);
    state.setGamepadAxis(1, InputKey::GamepadLeftY, 0.0f);
    REQUIRE(state.getAxis(InputKey::GamepadLeftX) == Approx(0.5f));
    REQUIRE(state.getAxis2D(InputKey::GamepadLeftStick).y == 0.0f);
    REQUIRE(state.isDown(InputKey::GamepadLeftX));

    state.setGamepadAxis(1, InputKey::GamepadRightTrigger, 0.55f);
    REQUIRE(state.getAxis(InputKey::GamepadRightTrigger) == Approx(0.5f));

    // The gamepad that moves most wins.
    state.setGamepadConnected(2, true, GamepadType::Xbox);
    state.setGamepadAxis(2, InputKey::GamepadLeftX, -1.0f);
    REQUIRE(state.getAxis(InputKey::GamepadLeftX) == Approx(-1.0f));
    REQUIRE(state.getAxis(InputKey::GamepadLeftX, 0b10u) == Approx(0.5f));

    state.beginFrame();
    state.setGamepadConnected(1, false, GamepadType::Unknown);
    REQUIRE(state.wasReleased(InputKey::GamepadSouth));
    REQUIRE_FALSE(state.isDown(InputKey::GamepadSouth));
    REQUIRE(state.getAxis(InputKey::GamepadRightTrigger) == 0.0f);
    REQUIRE(state.getConnectedGamepads() == 0b100u);
  }

  SECTION("Losing the focus releases everything")
  {
    state.beginFrame();
    state.setKey(InputKey::A, true);
    state.setKey(InputKey::MouseLeft, true);
    state.beginFrame();
    state.releaseAll();
    REQUIRE_FALSE(state.isDown(InputKey::A));
    REQUIRE(state.wasReleased(InputKey::A));
    REQUIRE(state.wasReleased(InputKey::MouseLeft));
  }
}

TEST_CASE("chCore - Input bindings", "[Input]")
{
  InputBinding binding;
  REQUIRE(InputManager::parseBinding("W:+Y", binding));
  REQUIRE(binding == InputBinding{.key = InputKey::W, .target = InputAxisTarget::PositiveY});
  REQUIRE(InputManager::parseBinding(" GamepadRightStick * 4 ", binding));
  REQUIRE(binding == InputBinding{.key = InputKey::GamepadRightStick, .scale = 4.0f});
  REQUIRE(InputManager::parseBinding("s:-", binding));
  REQUIRE(binding == InputBinding{.key = InputKey::S, .target = InputAxisTarget::NegativeX});
  REQUIRE(InputManager::parseBinding("MouseDelta*0.5", binding));
  REQUIRE(binding.scale == 0.5f);

  REQUIRE_FALSE(InputManager::parseBinding("Banana", binding));
  REQUIRE_FALSE(InputManager::parseBinding("W:Z", binding));
  REQUIRE_FALSE(InputManager::parseBinding("W*", binding));
  REQUIRE_FALSE(InputManager::parseBinding("W*2x", binding));
  REQUIRE_FALSE(InputManager::parseBinding("", binding));

  const auto roundTrip = [](StringView text, InputActionType type) {
    InputBinding parsed;
    REQUIRE(InputManager::parseBinding(text, parsed));
    return InputManager::bindingToString(parsed, type);
  };
  REQUIRE(roundTrip("W:+Y", InputActionType::Axis2D) == "W:+Y");
  REQUIRE(roundTrip("S:-", InputActionType::Axis1D) == "S:-");
  REQUIRE(roundTrip("GamepadRightStick*4", InputActionType::Axis2D) == "GamepadRightStick*4");
  REQUIRE(roundTrip("space", InputActionType::Button) == "Space");

  for (const InputActionType type :
       {InputActionType::Button, InputActionType::Axis1D, InputActionType::Axis2D}) {
    InputActionType parsedType = InputActionType::Button;
    REQUIRE(InputManager::parseActionType(InputManager::getActionTypeName(type), parsedType));
    REQUIRE(parsedType == type);
  }
  InputActionType parsedType = InputActionType::Axis2D;
  REQUIRE(InputManager::parseActionType("", parsedType));
  REQUIRE(parsedType == InputActionType::Button);
  REQUIRE(InputManager::parseActionType("axis1d", parsedType));
  REQUIRE(parsedType == InputActionType::Axis1D);
  REQUIRE_FALSE(InputManager::parseActionType("Trigger", parsedType));

  using Type = InputActionType;
  const auto valid = [](StringView text, Type type) {
    InputBinding parsed;
    REQUIRE(InputManager::parseBinding(text, parsed));
    return InputManager::isBindingValid(parsed, type);
  };
  REQUIRE(valid("Space", Type::Button));
  REQUIRE(valid("GamepadLeftTrigger", Type::Button));
  REQUIRE_FALSE(valid("Space:+X", Type::Button));
  REQUIRE(valid("W", Type::Axis1D));
  REQUIRE(valid("MouseWheel", Type::Axis1D));
  REQUIRE_FALSE(valid("MouseDelta", Type::Axis1D));
  REQUIRE_FALSE(valid("W:+Y", Type::Axis1D));
  REQUIRE(valid("W:+Y", Type::Axis2D));
  REQUIRE(valid("MouseX:-Y", Type::Axis2D));
  REQUIRE_FALSE(valid("W", Type::Axis2D));
  REQUIRE(valid("GamepadLeftStick", Type::Axis2D));
  REQUIRE_FALSE(valid("GamepadLeftStick:+X", Type::Axis2D));
}

TEST_CASE("chCore - Input actions", "[Input]")
{
  // A local object, so every section starts clean; the module cannot start twice.
  InputManager input;

  ConfigFile defaults;
  defaults.parse("[Move]\nType=Axis2D\nKeys=W:+Y, S:-Y, D:+X, A:-X, GamepadLeftStick\n"
                 "[Jump]\nKeys=Space, GamepadSouth\n"
                 "[Zoom]\nType=Axis1D\nKeys=MouseWheel, Q:-*2\n"
                 "[Pause]\nKeys=P\nContext=Menu\n"
                 "[Broken]\nType=Axis3D\nKeys=Space\n"
                 "[Partly]\nType=Axis2D\nKeys=W, Banana, D:+X\n");
  input.loadBindings(defaults, ConfigFile());

  const InputActionId move = input.findAction("move");
  const InputActionId jump = input.findAction("Jump");
  const InputActionId zoom = input.findAction("Zoom");
  const InputActionId pause = input.findAction("Pause");
  REQUIRE(input.getActionCount() == 5);
  REQUIRE(input.findAction("Broken") == InputManager::INVALID_ACTION);
  REQUIRE(input.getBindings(input.findAction("Partly")).size() == 1);
  REQUIRE(input.getActionType(move) == InputActionType::Axis2D);
  REQUIRE(input.getActionContext(pause) == "Menu");

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

  SECTION("Keys held together are not faster on the diagonal")
  {
    frame({key(InputKey::W, true), key(InputKey::D, true)});
    const Vector2 value = input.getActionValue2D(move);
    REQUIRE(value.magnitude() == Approx(1.0f));
    REQUIRE(value.x == Approx(value.y));
    REQUIRE(input.isActionDown(move));
    REQUIRE(input.wasActionPressed(move));

    frame({key(InputKey::D, false)});
    REQUIRE(input.getActionValue2D(move) == Vector2(0.0f, 1.0f));
    REQUIRE_FALSE(input.wasActionPressed(move));

    frame({key(InputKey::W, false)});
    REQUIRE_FALSE(input.isActionDown(move));
    REQUIRE(input.wasActionReleased(move));
  }

  SECTION("Sticks and scales")
  {
    frame({DisplayEvent(GamepadConnectionEvent{
               .slot = 0, .bConnected = true, .type = GamepadType::Xbox}),
           DisplayEvent(GamepadAxisEvent{
               .slot = 0, .axis = InputKey::GamepadLeftY, .value = 1.0f})});
    REQUIRE(input.getActionValue2D(move).y == Approx(1.0f));

    frame({key(InputKey::Q, true)});
    REQUIRE(input.getActionValue(zoom) == Approx(-2.0f));
    frame({DisplayEvent(MouseWheelEvent{.deltaX = 0.0f, .deltaY = 1.0f})});
    REQUIRE(input.getActionValue(zoom) == Approx(-1.0f));
  }

  SECTION("A tap inside a frame presses and releases a button")
  {
    frame({key(InputKey::Space, true), key(InputKey::Space, false)});
    REQUIRE_FALSE(input.isActionDown(jump));
    REQUIRE(input.wasActionPressed(jump));
    REQUIRE(input.wasActionReleased(jump));
  }

  SECTION("Fixed steps see an action press once")
  {
    frame({key(InputKey::Space, true)});
    frame({});
    input.beginFixedStep();
    REQUIRE(input.wasActionPressed(jump));
    input.endFixedStep();
    input.beginFixedStep();
    REQUIRE_FALSE(input.wasActionPressed(jump));
    REQUIRE(input.isActionDown(jump));
    input.endFixedStep();
  }

  SECTION("Contexts and game input")
  {
    frame({key(InputKey::P, true), key(InputKey::Space, true)});
    REQUIRE(input.isActionDown(pause));

    input.setContextEnabled("menu", false);
    frame({});
    REQUIRE_FALSE(input.isActionDown(pause));
    REQUIRE(input.wasActionReleased(pause));
    REQUIRE(input.isActionDown(jump));

    input.setGameInputEnabled(false);
    frame({});
    REQUIRE_FALSE(input.isActionDown(jump));
    // The device state keeps working for the editor.
    REQUIRE(input.getState().isDown(InputKey::Space));

    input.setGameInputEnabled(true);
    input.resetGameState();
    REQUIRE(input.isContextEnabled("Menu"));
    frame({});
    REQUIRE(input.isActionDown(pause));
  }

  SECTION("Rebinding, saving and loading the keys of the player")
  {
    REQUIRE(input.setBinding(jump, 0, InputBinding{.key = InputKey::Enter}));
    REQUIRE_FALSE(input.setBinding(
        jump, 0,
        InputBinding{.key = InputKey::GamepadLeftStick, .target = InputAxisTarget::PositiveX}));
    REQUIRE_FALSE(input.setBinding(jump, 5, InputBinding{.key = InputKey::E}));
    REQUIRE(input.setBinding(jump, 2, InputBinding{.key = InputKey::MouseLeft}));
    REQUIRE(input.getBindings(jump).size() == 3);

    frame({key(InputKey::Space, true)});
    REQUIRE_FALSE(input.isActionDown(jump));
    frame({key(InputKey::Enter, true)});
    REQUIRE(input.isActionDown(jump));

    Vector<InputActionId> users;
    input.findActionsUsingKey(InputKey::GamepadSouth, users);
    REQUIRE(users == Vector<InputActionId>{jump});

    ConfigFile user;
    input.writeUserBindings(user);
    REQUIRE(user.getSections().size() == 1);
    REQUIRE(*user.getValue("Jump", "Keys") == "Enter, GamepadSouth, MouseLeft");

    input.resetBindings(jump);
    REQUIRE(input.getBindings(jump)[0].key == InputKey::Space);
    ConfigFile unchanged;
    input.writeUserBindings(unchanged);
    REQUIRE(unchanged.empty());

    input.loadBindings(defaults, user);
    REQUIRE(input.getBindings(jump).size() == 3);
    REQUIRE(input.getDefaultBindings(jump).size() == 2);
    REQUIRE(input.removeBinding(jump, 0));
    REQUIRE(input.getBindings(jump)[0].key == InputKey::GamepadSouth);
    input.resetAllBindings();
    REQUIRE(input.getBindings(jump)[0].key == InputKey::Space);
  }

  SECTION("Capturing the next key for a rebinding menu")
  {
    InputKey captured = InputKey::None;
    input.captureNextKey([&captured](InputKey pressed) { captured = pressed; });
    frame({});
    REQUIRE(input.isCapturingKey());
    frame({key(InputKey::Space, true)});
    REQUIRE(captured == InputKey::Space);
    REQUIRE_FALSE(input.isCapturingKey());
    // The key went to the menu, not to the game.
    REQUIRE_FALSE(input.isActionDown(jump));
    // Nor later while it is held, so it does not trigger what it was just bound to.
    frame({});
    REQUIRE_FALSE(input.isActionDown(jump));
    frame({key(InputKey::Space, false)});
    frame({key(InputKey::Space, true)});
    REQUIRE(input.isActionDown(jump));
  }

  SECTION("Players read only their devices")
  {
    input.setPlayerCount(2);
    input.setPlayerDevices(0, true, 0b01u);
    input.setPlayerDevices(1, false, 0b10u);
    frame({DisplayEvent(GamepadConnectionEvent{
               .slot = 0, .bConnected = true, .type = GamepadType::Xbox}),
           DisplayEvent(GamepadConnectionEvent{
               .slot = 1, .bConnected = true, .type = GamepadType::PlayStation}),
           DisplayEvent(GamepadButtonEvent{
               .slot = 1, .button = InputKey::GamepadSouth, .bDown = true}),
           key(InputKey::W, true)});
    REQUIRE_FALSE(input.isActionDown(jump, 0));
    REQUIRE(input.isActionDown(jump, 1));
    REQUIRE(input.isActionDown(move, 0));
    REQUIRE_FALSE(input.isActionDown(move, 1));
    REQUIRE(input.getKeyDisplayName(InputKey::GamepadSouth, 1) == "Cross");
    REQUIRE(input.getKeyDisplayName(InputKey::GamepadSouth, 0) == "A");
    REQUIRE(input.getKeyDisplayName(InputKey::GamepadLeftShoulder, 0) == "Left Shoulder");
    REQUIRE_FALSE(input.isActionDown(jump, 3));
  }
}

TEST_CASE("chCore - Input key events", "[Input]")
{
  InputManager input;
  uint32 pressed = 0;
  uint32 released = 0;
  HEvent onPressed = input.OnKeyPressed.connect([&pressed](const KeyEvent& event) {
    pressed += event.key == InputKey::S && event.hasControl() ? 1 : 0;
  });
  HEvent onReleased =
      input.OnKeyReleased.connect([&released](const KeyEvent&) { ++released; });

  input.beginFrame();
  input.processEvent(KeyEvent{.key = InputKey::S, .bDown = true,
                              .modifiers = KeyModifier::RightControl});
  REQUIRE(pressed == 1);
  REQUIRE(input.getState().isDown(InputKey::S));
  input.processEvent(KeyEvent{.key = InputKey::S, .bDown = false, .modifiers = {}});
  REQUIRE(released == 1);
  // Window events are not input.
  input.processEvent(ResizeEvent{.width = 10, .height = 10});
  REQUIRE(pressed == 1);
  onPressed.disconnect();
  onReleased.disconnect();
}
