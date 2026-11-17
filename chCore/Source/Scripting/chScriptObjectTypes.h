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

#include "chAudioTypes.h"
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
 * Exists so scripts hold a sprite material as a plain value (SpriteMaterial.find gives one;
 * Sprites.makeStyle and Sprite:setMaterial take it). Only the asset's UUID is kept; the values
 * it sets belong to the shared asset, so every sprite using it changes.
 */
struct ScriptSpriteMaterial
{
  NODISCARD bool
  operator==(const ScriptSpriteMaterial& other) const { return material == other.material; }

  UUID material;
};

CH_SCRIPT_VALUE_TYPE(ScriptSpriteMaterial, "SpriteMaterial");

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

/**
 * Exists so what a sprite looks like is worked out once (Sprites.makeStyle) and then only
 * placed: by Sprites.draw, or by a plugin that draws many sprites of one look. The texture's
 * bindless index is read when the style is made.
 */
struct ScriptSpriteStyle
{
  UUID texture;
  // As GpuSpriteInstance::uvRect: the part of the texture shown, negative sizes flip.
  float uvRect[4];
  // Width and height in world units.
  float size[2];
  // (0, 0) is the bottom left corner.
  float pivot[2];
  float color[4];
  // World X.
  float depth;
  uint32 textureAndFlags;
  int16 sortingLayer;
  int16 order;
  // A SpriteBlendMode and a SpriteFilter.
  uint8 blendMode;
  uint8 filter;
  uint8 padding[2];
  // A SpriteMaterialAsset, null for the engine's shader.
  UUID material;
};

CH_SCRIPT_VALUE_TYPE(ScriptSpriteStyle, "SpriteStyle");

/**
 * Exists so scripts hold a font as a plain value (Font.find and Font properties give one;
 * Text.makeStyle and Text:setFont take it). Only the asset's UUID is kept; a null one is the
 * default font.
 */
struct ScriptFont
{
  NODISCARD bool
  operator==(const ScriptFont& other) const { return font == other.font; }

  UUID font;
};

CH_SCRIPT_VALUE_TYPE(ScriptFont, "Font");

/**
 * Exists so scripts hold an audio clip as a plain value (AudioClip.find and AudioClip
 * properties give one; Audio.play and AudioSource:setClip take it). Only the asset's UUID is
 * kept, so it never keeps the asset loaded and never dangles.
 */
struct ScriptAudioClip
{
  NODISCARD bool
  operator==(const ScriptAudioClip& other) const { return clip == other.clip; }

  UUID clip;
};

CH_SCRIPT_VALUE_TYPE(ScriptAudioClip, "AudioClip");
// The handle Audio.play gives: a stale one (its sound ended) does nothing.
CH_SCRIPT_VALUE_TYPE(AudioVoice, "AudioVoice");

/**
 * Exists so what a text looks like is worked out once (Text.makeStyle) and then only placed
 * by Text.draw. The font is kept by UUID and found again at each draw (one hash lookup), so
 * an unloaded font never dangles.
 */
struct ScriptTextStyle
{
  static constexpr uint8 MONOSPACE_DIGITS_BIT = 0x1u;
  static constexpr uint8 RICH_TEXT_BIT = 0x2u;
  static constexpr uint8 TRIM_ZEROS_BIT = 0x4u;

  UUID font;
  // Height of the font in world units.
  float size;
  float color[4];
  // (0, 0) is the bottom left corner.
  float pivot[2];
  float wrapWidth;
  float lineSpacing;
  float letterSpacing;
  // World X.
  float depth;
  float outlineWidth;
  float outlineColor[4];
  float shadowColor[4];
  float shadowOffset[2];
  // How numbers given to Text.draw are written (NumberFormatDesc).
  double plainBelow;
  int16 sortingLayer;
  int16 order;
  // A TextAlign, a SpriteBlendMode, the bits above and a NumberNotation.
  uint8 align;
  uint8 blendMode;
  uint8 flags;
  uint8 notation;
  int8 decimals;
  int8 plainDecimals;
  uint8 padding[6];
};

CH_SCRIPT_VALUE_TYPE(ScriptTextStyle, "TextStyle");
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

/**
 * Exists so a plugin gives scripts a handle to its own component of a GameObject the way the
 * engine does for Transform and Animator: the handle shares the object's, so it dies with
 * it, and the same component of an object is always the same Lua value. Register typeName
 * with ScriptClass first; its methods find the component through checkOwner at every call,
 * so a removed component is an error, never a dangling pointer.
 */
struct CH_CORE_EXPORT ScriptPartHandle
{
  static void
  push(lua_State* state, GameObject& gameObject, const ANSICHAR* typeName);

  /**
   * The object of the handle at index; raises when it is not one of typeName or the object
   * was destroyed.
   */
  NODISCARD static GameObject&
  checkOwner(lua_State* state, int32 index, const ANSICHAR* typeName);

  /**
   * The object of the handle at index, or null when it is not one of typeName or the object
   * died. Never raises.
   */
  NODISCARD static GameObject*
  testOwner(lua_State* state, int32 index, const ANSICHAR* typeName);
};

} // namespace chEngineSDK
