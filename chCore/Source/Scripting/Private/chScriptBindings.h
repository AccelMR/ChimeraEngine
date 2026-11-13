/************************************************************************/
/**
 * @file chScriptBindings.h
 * @author AccelMR
 * @date 2026/10/21
 * @brief
 *  The engine types and functions every script sees.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

struct lua_State;

namespace chEngineSDK {
class GameObject;

/**
 * Exists so each area of the engine registers its script API in its own file, and the script
 * runtime only calls them when it starts.
 */
class ScriptBindings
{
 public:
  /**
   * Vector2, Vector3, Vector4, Quaternion, Rotator, Color.
   */
  static void
  registerMath(lua_State* state);

  /**
   * GameObject, Transform, Animator, Scene.find and Scene.raycast.
   */
  static void
  registerObjects(lua_State* state);

  /**
   * Input (actions, keys, rebinding, rumble, cursor) and Key.
   */
  static void
  registerInput(lua_State* state);

  /**
   * DebugDraw, which draws nothing in builds without debug lines.
   */
  static void
  registerDebugDraw(lua_State* state);

  /**
   * Texture, SpriteStyle and Sprites (sprites drawn each frame without GameObjects).
   */
  static void
  registerSprites(lua_State* state);

  /**
   * Pushes the handle of one part of gameObject (its transform, a component), kept in the
   * GameObject handle's user value userValue and made on first use, so the same part is
   * always the same Lua value and dies with the object.
   */
  static void
  pushPartHandle(lua_State* state,
                 GameObject& gameObject,
                 int32 userValue,
                 const ANSICHAR* typeName);

  /**
   * The object of the part handle at index; raises when it is not one of typeName or the
   * object was destroyed.
   */
  NODISCARD static GameObject&
  checkPartOwner(lua_State* state, int32 index, const ANSICHAR* typeName);

  /**
   * gameObject:getSprite() and gameObject:getSpriteAnimator(), registered with the
   * GameObject methods.
   */
  static int32
  getSprite(lua_State* state);

  static int32
  getSpriteAnimator(lua_State* state);

  // The GameObject handle's user values, each the handle of one part.
  static constexpr int32 TRANSFORM_USER_VALUE = 1;
  static constexpr int32 ANIMATOR_USER_VALUE = 2;
  static constexpr int32 SPRITE_USER_VALUE = 3;
  static constexpr int32 SPRITE_ANIMATOR_USER_VALUE = 4;
  // A table of the handles ScriptPartHandle made, by type name.
  static constexpr int32 PLUGIN_PARTS_USER_VALUE = 5;
  static constexpr int32 USER_VALUE_COUNT = 5;
};

} // namespace chEngineSDK
