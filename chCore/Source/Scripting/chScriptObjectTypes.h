/************************************************************************/
/**
 * @file chScriptObjectTypes.h
 * @author AccelMR
 * @date 2026/10/21
 * @brief
 *  The scene objects scripts reach through handles.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chScriptBinder.h"
#include "chUUID.h"

namespace chEngineSDK {
class AnimatorComponent;
class GameObject;
class Transform;

/**
 * Exists so scripts hold a prefab as a plain value (Scene.findPrefab and Prefab properties
 * give one, Scene.instantiate takes it). Only the asset's UUID is kept, so it never keeps the
 * asset loaded and never dangles.
 */
struct ScriptPrefab
{
  NODISCARD bool
  operator==(const ScriptPrefab& other) const { return prefab == other.prefab; }

  UUID prefab;
};

/**
 * Exists so scripts hold a texture as a plain value (Texture.find and Texture properties
 * give one; Sprites.makeStyle and Sprites.draw take it). Only the asset's UUID is kept, so it
 * never keeps the asset loaded and never dangles.
 */
struct ScriptTexture
{
  NODISCARD bool
  operator==(const ScriptTexture& other) const { return texture == other.texture; }

  UUID texture;
};

CH_SCRIPT_VALUE_TYPE(ScriptPrefab, "Prefab");
CH_SCRIPT_VALUE_TYPE(ScriptTexture, "Texture");

/**
 * Exists so scripts hold a sprite atlas as a plain value (Atlas.find gives one;
 * Sprites.makeStyle, Sprite:setAtlas and SpriteAnimator:setAtlas take it). Only the asset's
 * UUID is kept.
 */
struct ScriptSpriteAtlas
{
  NODISCARD bool
  operator==(const ScriptSpriteAtlas& other) const { return atlas == other.atlas; }

  UUID atlas;
};

CH_SCRIPT_VALUE_TYPE(ScriptSpriteAtlas, "Atlas");
CH_SCRIPT_HANDLE_TYPE(GameObject, "GameObject");
CH_SCRIPT_HANDLE_TYPE(Transform, "Transform");
CH_SCRIPT_HANDLE_TYPE(AnimatorComponent, "Animator");

/**
 * Each GameObject has one handle, made the first time a script sees it and kept until the
 * object dies, so the same object is always the same Lua value (== and table keys work).
 * check raises a script error when the object was destroyed or left its scene.
 */
template<>
struct CH_CORE_EXPORT ScriptTraits<GameObject>
{
  static void
  push(lua_State* state, GameObject& gameObject);

  NODISCARD static GameObject&
  check(lua_State* state, int32 index);

  /**
   * The object at index, or null when it is not a GameObject or was destroyed. Never raises.
   */
  NODISCARD static GameObject*
  test(lua_State* state, int32 index);

  /**
   * Called when the object dies: scripts that still hold its handle get an error from then
   * on instead of a dangling pointer.
   */
  static void
  release(GameObject& gameObject) noexcept;
};

/**
 * The transform of a GameObject: a handle that shares the object's, so it dies with it.
 */
template<>
struct CH_CORE_EXPORT ScriptTraits<Transform>
{
  /**
   * Pushes the transform of gameObject (a Transform alone does not know its object).
   */
  static void
  pushOf(lua_State* state, GameObject& gameObject);

  NODISCARD static Transform&
  check(lua_State* state, int32 index);

  /**
   * The object whose transform is at index.
   */
  NODISCARD static GameObject&
  checkOwner(lua_State* state, int32 index);
};

/**
 * The animator of a GameObject: a handle that shares the object's like the transform, and
 * finds the component each time it is used, so removing the component never leaves it
 * dangling. check raises a script error when the object has no animator any more.
 */
template<>
struct CH_CORE_EXPORT ScriptTraits<AnimatorComponent>
{
  /**
   * Pushes the animator handle of gameObject, which must have an AnimatorComponent.
   */
  static void
  pushOf(lua_State* state, GameObject& gameObject);

  NODISCARD static AnimatorComponent&
  check(lua_State* state, int32 index);

  /**
   * The object whose animator is at index.
   */
  NODISCARD static GameObject&
  checkOwner(lua_State* state, int32 index);
};

} // namespace chEngineSDK
