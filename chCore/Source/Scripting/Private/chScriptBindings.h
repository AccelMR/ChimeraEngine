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
struct ShaderParameter;
class Vector4;

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
   * Font, TextStyle, Text (texts drawn each frame) and the Text handle of a GameObject.
   */
  static void
  registerText(lua_State* state);

  /**
   * AudioClip, AudioVoice, Audio (sounds played without GameObjects, bus volumes) and the
   * AudioSource handle of a GameObject.
   */
  static void
  registerAudio(lua_State* state);

  /**
   * Material values, MaterialInstance (materials made at run time) and the Model and Shape
   * handles of a GameObject, which change the materials an object draws with.
   */
  static void
  registerMaterials(lua_State* state);

  /**
   * The PostProcess handle of a GameObject: passes, weights and shader values.
   */
  static void
  registerPostProcess(lua_State* state);

  /**
   * The Console table: commands scripts add, running a console line, the cheat setting.
   */
  static void
  registerConsole(lua_State* state);

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
   * The value at index for a shader parameter of that type (number, integer, boolean,
   * Vector2, Vector3, Vector4 or Color), as shader values are kept. Raises on another type.
   */
  NODISCARD static Vector4
  checkShaderValue(lua_State* state, const ShaderParameter& parameter, int32 index);

  static void
  pushShaderValue(lua_State* state, const ShaderParameter& parameter, const Vector4& value);

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
