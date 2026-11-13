/************************************************************************/
/**
 * @file chScriptSpriteBindings.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Textures and the sprites scripts draw each frame without GameObjects.
 */
/************************************************************************/
#include "chScriptBindings.h"

#include "chAssetManager.h"
#include "chGameObject.h"
#include "chLinearColor.h"
#include "chPath.h"
#include "chScene.h"
#include "chScriptManager.h"
#include "chScriptMathTypes.h"
#include "chScriptObjectTypes.h"
#include "chSpriteAnimatorComponent.h"
#include "chSpriteAtlasAsset.h"
#include "chSpriteComponent.h"
#include "chSpriteDraw.h"
#include "chStringUtils.h"
#include "chTextureAsset.h"

namespace chEngineSDK {
CH_SCRIPT_HANDLE_TYPE(SpriteComponent, "Sprite");
CH_SCRIPT_HANDLE_TYPE(SpriteAnimatorComponent, "SpriteAnimator");

/**
 * The sprite of a GameObject: a handle that shares the object's like the transform, and
 * finds the component each time it is used, so a removed sprite is an error, never a
 * dangling pointer.
 */
template<>
struct ScriptTraits<SpriteComponent>
{
  NODISCARD static SpriteComponent&
  check(lua_State* state, const int32 index)
  {
    GameObject& gameObject = ScriptBindings::checkPartOwner(
        state, index, ScriptHandleType<SpriteComponent>::NAME);
    SpriteComponent* sprite = gameObject.getComponent<SpriteComponent>();
    if (!sprite) {
      luaL_error(state, "GameObject '%s' has no Sprite", gameObject.getName().c_str());
    }
    return *sprite;
  }
};

template<>
struct ScriptTraits<SpriteAnimatorComponent>
{
  NODISCARD static SpriteAnimatorComponent&
  check(lua_State* state, const int32 index)
  {
    GameObject& gameObject = ScriptBindings::checkPartOwner(
        state, index, ScriptHandleType<SpriteAnimatorComponent>::NAME);
    SpriteAnimatorComponent* animator = gameObject.getComponent<SpriteAnimatorComponent>();
    if (!animator) {
      luaL_error(state, "GameObject '%s' has no SpriteAnimator", gameObject.getName().c_str());
    }
    return *animator;
  }
};

namespace {
constexpr const ANSICHAR* kBlendNames[] = {"Alpha", "Additive"};
static_assert(sizeof(kBlendNames) / sizeof(kBlendNames[0]) ==
              static_cast<SIZE_T>(SpriteBlendMode::COUNT));
constexpr const ANSICHAR* kFilterNames[] = {"Point", "Linear"};
static_assert(sizeof(kFilterNames) / sizeof(kFilterNames[0]) ==
              static_cast<SIZE_T>(SpriteFilter::COUNT));

/*
 * The texture asset of a Texture value, or null when it no longer exists.
 */
SPtr<TextureAsset>
findTextureAsset(const ScriptTexture& texture)
{
  if (texture.texture.isNull() || !AssetManager::isStarted()) {
    return nullptr;
  }
  const SPtr<IAsset> asset = AssetManager::instance().getAsset(texture.texture);
  return asset ? asset->as<TextureAsset>() : nullptr;
}

/*
 * The texture at index, a Texture value or a path or name, loaded. Raises when there is
 * none. A path or name walks every asset, so scripts keep the value Texture.find gives.
 */
SPtr<TextureAsset>
checkTexture(lua_State* state, const int32 index, const ANSICHAR* function)
{
  if (!AssetManager::isStarted()) {
    luaL_error(state, "%s: there are no assets to search", function);
  }
  AssetManager& assetManager = AssetManager::instance();
  SPtr<TextureAsset> texture;
  if (const auto* value = static_cast<const ScriptTexture*>(
          luaL_testudata(state, index, ScriptValueType<ScriptTexture>::NAME))) {
    texture = findTextureAsset(*value);
    if (!texture) {
      luaL_error(state, "%s: the texture was deleted", function);
    }
  }
  else {
    const StringView text = ScriptTraits<StringView>::check(state, index);
    const SPtr<IAsset> asset =
        assetManager.findAsset(AssetTypeTraits<TextureAsset>::getTypeId(), text);
    if (!asset) {
      luaL_error(state, "%s: there is no texture %s", function, String(text).c_str());
    }
    texture = asset->as<TextureAsset>();
  }
  if (!texture->isLoaded() && !assetManager.syncLoadAsset(texture)) {
    luaL_error(state, "%s: texture %s could not be loaded", function, texture->getName());
  }
  return texture;
}

/*
 * The atlas asset of an Atlas value, or null when it no longer exists.
 */
SPtr<SpriteAtlasAsset>
findAtlasAsset(const ScriptSpriteAtlas& atlas)
{
  if (atlas.atlas.isNull() || !AssetManager::isStarted()) {
    return nullptr;
  }
  const SPtr<IAsset> asset = AssetManager::instance().getAsset(atlas.atlas);
  return asset ? asset->as<SpriteAtlasAsset>() : nullptr;
}

/*
 * The atlas at index, an Atlas value or a path or name, loaded (its texture too). Raises
 * when there is none.
 */
SPtr<SpriteAtlasAsset>
checkAtlas(lua_State* state, const int32 index, const ANSICHAR* function)
{
  if (!AssetManager::isStarted()) {
    luaL_error(state, "%s: there are no assets to search", function);
  }
  AssetManager& assetManager = AssetManager::instance();
  SPtr<SpriteAtlasAsset> atlas;
  if (const auto* value = static_cast<const ScriptSpriteAtlas*>(
          luaL_testudata(state, index, ScriptValueType<ScriptSpriteAtlas>::NAME))) {
    atlas = findAtlasAsset(*value);
    if (!atlas) {
      luaL_error(state, "%s: the atlas was deleted", function);
    }
  }
  else {
    const StringView text = ScriptTraits<StringView>::check(state, index);
    const SPtr<IAsset> asset =
        assetManager.findAsset(AssetTypeTraits<SpriteAtlasAsset>::getTypeId(), text);
    if (!asset) {
      luaL_error(state, "%s: there is no atlas %s", function, String(text).c_str());
    }
    atlas = asset->as<SpriteAtlasAsset>();
  }
  if (!atlas->isLoaded() && !assetManager.syncLoadAsset(atlas)) {
    luaL_error(state, "%s: atlas %s could not be loaded", function, atlas->getName());
  }
  return atlas;
}

/*
 * The frame at index of atlas: a name or a number from 1. Raises when there is none.
 */
uint32
checkFrame(lua_State* state,
           const int32 index,
           const SpriteAtlasAsset& atlas,
           const ANSICHAR* function)
{
  const SpriteAtlas& data = atlas.getAtlas();
  if (lua_type(state, index) == LUA_TNUMBER) {
    const lua_Integer number = luaL_checkinteger(state, index);
    if (number < 1 || number > static_cast<lua_Integer>(data.getFrames().size())) {
      luaL_error(state, "%s: atlas %s has no frame %d", function, atlas.getName(),
                 static_cast<int32>(number));
    }
    return static_cast<uint32>(number - 1);
  }
  const StringView name = ScriptTraits<StringView>::check(state, index);
  const uint32 frame = data.findFrame(name);
  if (frame == SpriteAtlas::NOT_FOUND) {
    luaL_error(state, "%s: atlas %s has no frame '%s'", function, atlas.getName(),
               String(name).c_str());
  }
  return frame;
}

/*
 * The style a texture alone gets: the whole image at the default pixels per unit.
 */
ScriptSpriteStyle
makeDefaultStyle(const TextureAsset* texture)
{
  ScriptSpriteStyle style{};
  style.texture = texture ? texture->getUUID() : UUID::null();
  SpriteDraw::makeUvRect(texture, Vector2::ZERO, Vector2::ZERO, false, false, style.uvRect);
  const bool bHasSize = texture && texture->getWidth() > 0 && texture->getHeight() > 0;
  style.size[0] = bHasSize ? static_cast<float>(texture->getWidth()) /
                                 SpriteComponent::DEFAULT_PIXELS_PER_UNIT
                           : 1.0f;
  style.size[1] = bHasSize ? static_cast<float>(texture->getHeight()) /
                                 SpriteComponent::DEFAULT_PIXELS_PER_UNIT
                           : 1.0f;
  style.pivot[0] = 0.5f;
  style.pivot[1] = 0.5f;
  style.color[0] = 1.0f;
  style.color[1] = 1.0f;
  style.color[2] = 1.0f;
  style.color[3] = 1.0f;
  style.textureAndFlags = SpriteDraw::makeTextureFlags(texture, SpriteFilter::Point);
  style.blendMode = static_cast<uint8>(SpriteBlendMode::Alpha);
  style.filter = static_cast<uint8>(SpriteFilter::Point);
  return style;
}

/*
 * The index of name in names, raising with the allowed names when it is none of them.
 */
template<SIZE_T Count>
uint8
checkName(lua_State* state,
          const int32 index,
          const ANSICHAR* option,
          const ANSICHAR* const (&names)[Count])
{
  const StringView text = ScriptTraits<StringView>::check(state, index);
  for (SIZE_T i = 0; i < Count; ++i) {
    if (StringUtils::equalsIgnoreCase(text, names[i])) {
      return static_cast<uint8>(i);
    }
  }
  luaL_error(state, "Sprites.makeStyle: %s must be %s or %s, not '%s'", option, names[0],
             names[1], String(text).c_str());
  return 0;
}

NODISCARD int16
checkInt16(lua_State* state, const int32 index, const ANSICHAR* option)
{
  const lua_Integer value = luaL_checkinteger(state, index);
  if (value < INT16_MIN || value > INT16_MAX) {
    luaL_error(state, "%s must be between %d and %d", option, INT16_MIN, INT16_MAX);
  }
  return static_cast<int16>(value);
}

/*
 * Sprites.makeStyle(options): a SpriteStyle. Options (all optional): texture (Texture, path
 * or name), sourceRect (Vector4: x, y, width, height in pixels from the top left), size
 * (Vector2 in world units; else the pixels over pixelsPerUnit), pixelsPerUnit (100), pivot
 * (Vector2, (0, 0) bottom left), color, layer, order, blend ("Alpha", "Additive"), filter
 * ("Point", "Linear"), flipX, flipY, depth (world X). A misspelt option raises.
 */
int32
makeStyle(lua_State* state)
{
  constexpr const ANSICHAR* kFunction = "Sprites.makeStyle";
  if (!lua_isnoneornil(state, 1)) {
    luaL_checktype(state, 1, LUA_TTABLE);
  }

  SPtr<TextureAsset> texture;
  SPtr<SpriteAtlasAsset> atlas;
  uint32 frame = SpriteAtlas::NOT_FOUND;
  if (lua_istable(state, 1)) {
    if (lua_getfield(state, 1, "texture") != LUA_TNIL) {
      texture = checkTexture(state, -1, kFunction);
    }
    if (lua_getfield(state, 1, "atlas") != LUA_TNIL) {
      if (texture) {
        return luaL_error(state, "%s: give a texture or an atlas, not both", kFunction);
      }
      atlas = checkAtlas(state, -1, kFunction);
      texture = atlas->getTexture();
      if (lua_getfield(state, 1, "frame") != LUA_TNIL) {
        frame = checkFrame(state, -1, *atlas, kFunction);
      }
    }
    else if (lua_getfield(state, 1, "frame") != LUA_TNIL) {
      return luaL_error(state, "%s: a frame needs an atlas", kFunction);
    }
  }
  lua_settop(state, 1);
  ScriptSpriteStyle style = makeDefaultStyle(texture.get());
  if (!lua_istable(state, 1)) {
    ScriptTraits<ScriptSpriteStyle>::push(state, style);
    return 1;
  }

  Vector2 sourcePosition = Vector2::ZERO;
  Vector2 sourceSize = Vector2::ZERO;
  if (frame != SpriteAtlas::NOT_FOUND) {
    const SpriteAtlasFrame& atlasFrame = atlas->getAtlas().getFrames()[frame];
    sourcePosition = atlasFrame.position;
    sourceSize = atlasFrame.size;
    style.pivot[0] = atlasFrame.pivot.x;
    style.pivot[1] = atlasFrame.pivot.y;
  }
  float pixelsPerUnit = SpriteComponent::DEFAULT_PIXELS_PER_UNIT;
  bool bHasSize = false;
  bool bFlipX = false;
  bool bFlipY = false;
  SpriteFilter filter = SpriteFilter::Point;

  lua_pushnil(state);
  while (lua_next(state, 1) != 0) {
    const int32 value = lua_gettop(state);
    if (lua_type(state, -2) != LUA_TSTRING) {
      return luaL_error(state, "%s: options are named fields", kFunction);
    }
    const StringView key = lua_tostring(state, -2);
    if (key == "texture" || key == "atlas" || key == "frame") {
      // Read above.
    }
    else if (key == "sourceRect") {
      const Vector4 rect = ScriptTraits<Vector4>::check(state, value);
      sourcePosition = Vector2(rect.x, rect.y);
      sourceSize = Vector2(Math::max(rect.z, 0.0f), Math::max(rect.w, 0.0f));
    }
    else if (key == "size") {
      const Vector2 size = ScriptTraits<Vector2>::check(state, value);
      style.size[0] = size.x;
      style.size[1] = size.y;
      bHasSize = true;
    }
    else if (key == "pixelsPerUnit") {
      pixelsPerUnit = Math::max(static_cast<float>(luaL_checknumber(state, value)),
                                SpriteComponent::MIN_PIXELS_PER_UNIT);
    }
    else if (key == "pivot") {
      const Vector2 pivot = ScriptTraits<Vector2>::check(state, value);
      style.pivot[0] = pivot.x;
      style.pivot[1] = pivot.y;
    }
    else if (key == "color") {
      const LinearColor color = ScriptTraits<LinearColor>::check(state, value);
      style.color[0] = color.r;
      style.color[1] = color.g;
      style.color[2] = color.b;
      style.color[3] = color.a;
    }
    else if (key == "layer") {
      style.sortingLayer = checkInt16(state, value, "layer");
    }
    else if (key == "order") {
      style.order = checkInt16(state, value, "order");
    }
    else if (key == "blend") {
      style.blendMode = checkName(state, value, "blend", kBlendNames);
    }
    else if (key == "filter") {
      filter = static_cast<SpriteFilter>(checkName(state, value, "filter", kFilterNames));
    }
    else if (key == "flipX") {
      bFlipX = lua_toboolean(state, value) != 0;
    }
    else if (key == "flipY") {
      bFlipY = lua_toboolean(state, value) != 0;
    }
    else if (key == "depth") {
      style.depth = static_cast<float>(luaL_checknumber(state, value));
    }
    else {
      return luaL_error(state, "%s: unknown option '%s'", kFunction, lua_tostring(state, -2));
    }
    lua_pop(state, 1);
  }

  SpriteDraw::makeUvRect(texture.get(), sourcePosition, sourceSize, bFlipX, bFlipY,
                         style.uvRect);
  style.filter = static_cast<uint8>(filter);
  style.textureAndFlags = SpriteDraw::makeTextureFlags(texture.get(), filter);
  if (!bHasSize) {
    Vector2 pixels = sourceSize;
    if ((pixels.x <= 0.0f || pixels.y <= 0.0f) && texture && texture->getWidth() > 0) {
      pixels = Vector2(static_cast<float>(texture->getWidth()),
                       static_cast<float>(texture->getHeight()));
    }
    if (pixels.x > 0.0f && pixels.y > 0.0f) {
      style.size[0] = pixels.x / pixelsPerUnit;
      style.size[1] = pixels.y / pixelsPerUnit;
    }
  }
  ScriptTraits<ScriptSpriteStyle>::push(state, style);
  return 1;
}

/*
 * The list of the running script's scene for the phase it runs in, so sprites drawn in
 * fixedUpdate stay until the next step and those drawn in update last one frame. Null
 * without a scene: there is nowhere to draw.
 */
SpriteDraw*
getSpriteDraw()
{
  const ScriptManager& scriptManager = ScriptManager::instance();
  Scene* const scene = scriptManager.getCurrentScene();
  if (!scene) {
    return nullptr;
  }
  return scriptManager.isInFixedStep() ? &scene->getFixedSpriteDraw()
                                       : &scene->getSpriteDraw();
}

/*
 * Sprites.draw(look, x, y, rotation?, scale?, color?): one sprite this frame at (x, y) in
 * the YZ plane, turned counterclockwise by rotation degrees, its size times scale, its color
 * replaced by color. look is a SpriteStyle, or a Texture drawn whole at the default pixels
 * per unit. One C call and no allocation.
 */
int32
drawSprite(lua_State* state)
{
  ScriptSpriteStyle style;
  if (const auto* given = static_cast<const ScriptSpriteStyle*>(
          luaL_testudata(state, 1, ScriptValueType<ScriptSpriteStyle>::NAME))) {
    style = *given;
  }
  else if (luaL_testudata(state, 1, ScriptValueType<ScriptTexture>::NAME) != nullptr) {
    style = makeDefaultStyle(checkTexture(state, 1, "Sprites.draw").get());
  }
  else {
    return luaL_typeerror(state, 1, "SpriteStyle or Texture");
  }
  const float x = static_cast<float>(luaL_checknumber(state, 2));
  const float y = static_cast<float>(luaL_checknumber(state, 3));
  const float rotation = static_cast<float>(luaL_optnumber(state, 4, 0.0));
  const float scale = static_cast<float>(luaL_optnumber(state, 5, 1.0));

  SpriteDraw* spriteDraw = getSpriteDraw();
  if (!spriteDraw) {
    return 0;
  }
  SpriteQuad quad{.position = Vector2(x, y),
                  .depth = style.depth,
                  .size = Vector2(style.size[0] * scale, style.size[1] * scale),
                  .rotation = Radian(Degree(rotation)),
                  .pivot = Vector2(style.pivot[0], style.pivot[1]),
                  .color = LinearColor(style.color[0], style.color[1], style.color[2],
                                       style.color[3]),
                  .textureAndFlags = style.textureAndFlags,
                  .uvRect = {style.uvRect[0], style.uvRect[1], style.uvRect[2],
                             style.uvRect[3]},
                  .sortingLayer = style.sortingLayer,
                  .order = style.order,
                  .blendMode = static_cast<SpriteBlendMode>(style.blendMode)};
  if (!lua_isnoneornil(state, 6)) {
    quad.color = ScriptTraits<LinearColor>::check(state, 6);
  }
  spriteDraw->quad(quad);
  return 0;
}

int32
setStyleColor(lua_State* state)
{
  auto& style = ScriptTraits<ScriptSpriteStyle>::check(state, 1);
  const LinearColor color = ScriptTraits<LinearColor>::check(state, 2);
  style.color[0] = color.r;
  style.color[1] = color.g;
  style.color[2] = color.b;
  style.color[3] = color.a;
  return 0;
}

int32
setStyleSize(lua_State* state)
{
  auto& style = ScriptTraits<ScriptSpriteStyle>::check(state, 1);
  const Vector2 size = ScriptTraits<Vector2>::check(state, 2);
  style.size[0] = size.x;
  style.size[1] = size.y;
  return 0;
}

int32
getStyleSize(lua_State* state)
{
  const auto& style = ScriptTraits<ScriptSpriteStyle>::check(state, 1);
  ScriptTraits<Vector2>::push(state, Vector2(style.size[0], style.size[1]));
  return 1;
}

int32
setStyleSorting(lua_State* state)
{
  auto& style = ScriptTraits<ScriptSpriteStyle>::check(state, 1);
  style.sortingLayer = checkInt16(state, 2, "layer");
  style.order = lua_isnoneornil(state, 3) ? style.order : checkInt16(state, 3, "order");
  return 0;
}

int32
copyStyle(lua_State* state)
{
  const ScriptSpriteStyle style = ScriptTraits<ScriptSpriteStyle>::check(state, 1);
  ScriptTraits<ScriptSpriteStyle>::push(state, style);
  return 1;
}

/*
 * Texture.find(pathOrName): a Texture value, nil when there is none. Walks every asset, so
 * call it once (in onStart).
 */
int32
findTexture(lua_State* state)
{
  const StringView text = ScriptTraits<StringView>::check(state, 1);
  if (!AssetManager::isStarted()) {
    return luaL_error(state, "Texture.find: there are no assets to search");
  }
  const SPtr<IAsset> asset =
      AssetManager::instance().findAsset(AssetTypeTraits<TextureAsset>::getTypeId(), text);
  if (!asset) {
    lua_pushnil(state);
    return 1;
  }
  ScriptTraits<ScriptTexture>::push(state, ScriptTexture{asset->getUUID()});
  return 1;
}

int32
isTextureValid(lua_State* state)
{
  const auto& texture = ScriptTraits<ScriptTexture>::check(state, 1);
  lua_pushboolean(state, findTextureAsset(texture) ? 1 : 0);
  return 1;
}

int32
getTextureName(lua_State* state)
{
  const auto& texture = ScriptTraits<ScriptTexture>::check(state, 1);
  const SPtr<TextureAsset> asset = findTextureAsset(texture);
  if (asset) {
    lua_pushstring(state, asset->getName());
  }
  else {
    lua_pushnil(state);
  }
  return 1;
}

/*
 * The virtual path, such as /Game/Sprites/Hero.
 */
int32
getTexturePath(lua_State* state)
{
  const auto& texture = ScriptTraits<ScriptTexture>::check(state, 1);
  const SPtr<TextureAsset> asset = findTextureAsset(texture);
  if (!asset) {
    lua_pushnil(state);
    return 1;
  }
  const String path = Path(asset->getAssetPath()).join(Path(asset->getName())).toString();
  lua_pushlstring(state, path.data(), path.size());
  return 1;
}

/*
 * Width and height in pixels; loads the texture.
 */
int32
getTextureSize(lua_State* state)
{
  const SPtr<TextureAsset> asset = checkTexture(state, 1, "Texture.getSize");
  lua_pushinteger(state, static_cast<lua_Integer>(asset->getWidth()));
  lua_pushinteger(state, static_cast<lua_Integer>(asset->getHeight()));
  return 2;
}

int32
textureToString(lua_State* state)
{
  const auto& texture = ScriptTraits<ScriptTexture>::check(state, 1);
  const SPtr<TextureAsset> asset = findTextureAsset(texture);
  lua_pushfstring(state, "Texture(%s)", asset ? asset->getName() : "missing");
  return 1;
}

/*
 * Atlas.find(pathOrName): an Atlas value, nil when there is none. Walks every asset, so call
 * it once (in onStart).
 */
int32
findAtlas(lua_State* state)
{
  const StringView text = ScriptTraits<StringView>::check(state, 1);
  if (!AssetManager::isStarted()) {
    return luaL_error(state, "Atlas.find: there are no assets to search");
  }
  const SPtr<IAsset> asset = AssetManager::instance().findAsset(
      AssetTypeTraits<SpriteAtlasAsset>::getTypeId(), text);
  if (!asset) {
    lua_pushnil(state);
    return 1;
  }
  ScriptTraits<ScriptSpriteAtlas>::push(state, ScriptSpriteAtlas{asset->getUUID()});
  return 1;
}

int32
isAtlasValid(lua_State* state)
{
  const auto& atlas = ScriptTraits<ScriptSpriteAtlas>::check(state, 1);
  lua_pushboolean(state, findAtlasAsset(atlas) ? 1 : 0);
  return 1;
}

int32
getAtlasName(lua_State* state)
{
  const auto& atlas = ScriptTraits<ScriptSpriteAtlas>::check(state, 1);
  const SPtr<SpriteAtlasAsset> asset = findAtlasAsset(atlas);
  if (asset) {
    lua_pushstring(state, asset->getName());
  }
  else {
    lua_pushnil(state);
  }
  return 1;
}

int32
getAtlasFrameCount(lua_State* state)
{
  const SPtr<SpriteAtlasAsset> atlas = checkAtlas(state, 1, "Atlas.getFrameCount");
  lua_pushinteger(state, static_cast<lua_Integer>(atlas->getAtlas().getFrames().size()));
  return 1;
}

/*
 * atlas:findFrame(name): the frame's number (from 1), nil when there is none.
 */
int32
findAtlasFrame(lua_State* state)
{
  const SPtr<SpriteAtlasAsset> atlas = checkAtlas(state, 1, "Atlas.findFrame");
  const uint32 frame = atlas->getAtlas().findFrame(ScriptTraits<StringView>::check(state, 2));
  if (frame == SpriteAtlas::NOT_FOUND) {
    lua_pushnil(state);
  }
  else {
    lua_pushinteger(state, static_cast<lua_Integer>(frame) + 1);
  }
  return 1;
}

int32
getAtlasFrameName(lua_State* state)
{
  const SPtr<SpriteAtlasAsset> atlas = checkAtlas(state, 1, "Atlas.getFrameName");
  const uint32 frame = checkFrame(state, 2, *atlas, "Atlas.getFrameName");
  const String& name = atlas->getAtlas().getFrames()[frame].name;
  lua_pushlstring(state, name.data(), name.size());
  return 1;
}

/*
 * A new table with the names of the atlas' animations, in their order.
 */
int32
getAtlasAnimationNames(lua_State* state)
{
  const SPtr<SpriteAtlasAsset> atlas = checkAtlas(state, 1, "Atlas.getAnimationNames");
  const Vector<SpriteAnimation>& animations = atlas->getAtlas().getAnimations();
  lua_createtable(state, static_cast<int32>(animations.size()), 0);
  for (SIZE_T i = 0; i < animations.size(); ++i) {
    lua_pushlstring(state, animations[i].name.data(), animations[i].name.size());
    lua_rawseti(state, -2, static_cast<lua_Integer>(i) + 1);
  }
  return 1;
}

int32
getAtlasTexture(lua_State* state)
{
  const SPtr<SpriteAtlasAsset> atlas = checkAtlas(state, 1, "Atlas.getTexture");
  if (atlas->getTexture()) {
    ScriptTraits<ScriptTexture>::push(state, ScriptTexture{atlas->getTexture()->getUUID()});
  }
  else {
    lua_pushnil(state);
  }
  return 1;
}

int32
atlasToString(lua_State* state)
{
  const auto& atlas = ScriptTraits<ScriptSpriteAtlas>::check(state, 1);
  const SPtr<SpriteAtlasAsset> asset = findAtlasAsset(atlas);
  lua_pushfstring(state, "Atlas(%s)", asset ? asset->getName() : "missing");
  return 1;
}

int32
getSpriteGameObject(lua_State* state)
{
  ScriptTraits<GameObject>::push(
      state,
      ScriptBindings::checkPartOwner(state, 1, ScriptHandleType<SpriteComponent>::NAME));
  return 1;
}

/*
 * sprite:setTexture(texture?): a Texture, path or name; nil draws the color alone.
 */
int32
setSpriteTexture(lua_State* state)
{
  SpriteComponent& sprite = ScriptTraits<SpriteComponent>::check(state, 1);
  sprite.setTexture(lua_isnoneornil(state, 2) ? nullptr
                                              : checkTexture(state, 2, "Sprite.setTexture"));
  return 0;
}

/*
 * sprite:setAtlas(atlas, frame?): shows a frame of the atlas (a name or a number from 1; the
 * first without one).
 */
int32
setSpriteAtlas(lua_State* state)
{
  SpriteComponent& sprite = ScriptTraits<SpriteComponent>::check(state, 1);
  const SPtr<SpriteAtlasAsset> atlas = checkAtlas(state, 2, "Sprite.setAtlas");
  const uint32 frame =
      lua_isnoneornil(state, 3) ? 0 : checkFrame(state, 3, *atlas, "Sprite.setAtlas");
  sprite.setAtlasFrame(atlas, frame);
  return 0;
}

/*
 * sprite:setFrame(frame): a name or a number from 1 of the sprite's atlas.
 */
int32
setSpriteFrame(lua_State* state)
{
  SpriteComponent& sprite = ScriptTraits<SpriteComponent>::check(state, 1);
  if (!sprite.getAtlas()) {
    return luaL_error(state, "Sprite.setFrame: the sprite shows no atlas");
  }
  sprite.setFrame(checkFrame(state, 2, *sprite.getAtlas(), "Sprite.setFrame"));
  return 0;
}

/*
 * The frame's number (from 1), nil without an atlas.
 */
int32
getSpriteFrame(lua_State* state)
{
  const SpriteComponent& sprite = ScriptTraits<SpriteComponent>::check(state, 1);
  if (!sprite.getAtlas() || sprite.getFrame() == SpriteComponent::NO_FRAME) {
    lua_pushnil(state);
  }
  else {
    lua_pushinteger(state, static_cast<lua_Integer>(sprite.getFrame()) + 1);
  }
  return 1;
}

int32
setSpriteColor(lua_State* state)
{
  SpriteComponent& sprite = ScriptTraits<SpriteComponent>::check(state, 1);
  sprite.setColor(ScriptTraits<LinearColor>::check(state, 2));
  return 0;
}

int32
getSpriteColor(lua_State* state)
{
  ScriptTraits<LinearColor>::push(state,
                                  ScriptTraits<SpriteComponent>::check(state, 1).getColor());
  return 1;
}

/*
 * sprite:setFlip(flipX, flipY?): flipY keeps its value when not given.
 */
int32
setSpriteFlip(lua_State* state)
{
  SpriteComponent& sprite = ScriptTraits<SpriteComponent>::check(state, 1);
  const bool bFlipX = lua_toboolean(state, 2) != 0;
  const bool bFlipY =
      lua_isnoneornil(state, 3) ? sprite.isFlippedY() : lua_toboolean(state, 3) != 0;
  sprite.setFlip(bFlipX, bFlipY);
  return 0;
}

int32
isSpriteFlippedX(lua_State* state)
{
  lua_pushboolean(state, ScriptTraits<SpriteComponent>::check(state, 1).isFlippedX() ? 1 : 0);
  return 1;
}

int32
isSpriteFlippedY(lua_State* state)
{
  lua_pushboolean(state, ScriptTraits<SpriteComponent>::check(state, 1).isFlippedY() ? 1 : 0);
  return 1;
}

int32
setSpriteSorting(lua_State* state)
{
  SpriteComponent& sprite = ScriptTraits<SpriteComponent>::check(state, 1);
  const int16 layer = checkInt16(state, 2, "layer");
  const int16 order =
      lua_isnoneornil(state, 3) ? sprite.getOrder() : checkInt16(state, 3, "order");
  sprite.setSorting(layer, order);
  return 0;
}

int32
getSpriteSorting(lua_State* state)
{
  const SpriteComponent& sprite = ScriptTraits<SpriteComponent>::check(state, 1);
  lua_pushinteger(state, sprite.getSortingLayer());
  lua_pushinteger(state, sprite.getOrder());
  return 2;
}

int32
setSpriteVisible(lua_State* state)
{
  ScriptTraits<SpriteComponent>::check(state, 1).setEnabled(lua_toboolean(state, 2) != 0);
  return 0;
}

int32
isSpriteVisible(lua_State* state)
{
  lua_pushboolean(state, ScriptTraits<SpriteComponent>::check(state, 1).isEnabled() ? 1 : 0);
  return 1;
}

int32
getSpriteAnimatorGameObject(lua_State* state)
{
  ScriptTraits<GameObject>::push(
      state, ScriptBindings::checkPartOwner(state, 1,
                                            ScriptHandleType<SpriteAnimatorComponent>::NAME));
  return 1;
}

/*
 * animator:play(animation, restart?): the animation playing only resumes unless restart, so
 * calling it every frame is fine. A name the atlas does not have raises.
 */
int32
playSpriteAnimation(lua_State* state)
{
  SpriteAnimatorComponent& animator = ScriptTraits<SpriteAnimatorComponent>::check(state, 1);
  const StringView name = ScriptTraits<StringView>::check(state, 2);
  if (!animator.play(name, lua_toboolean(state, 3) != 0)) {
    return luaL_error(state, "SpriteAnimator.play: %s has no animation '%s'",
                      animator.getAtlas() ? animator.getAtlas()->getName() : "(no atlas)",
                      String(name).c_str());
  }
  return 0;
}

int32
stopSpriteAnimation(lua_State* state)
{
  ScriptTraits<SpriteAnimatorComponent>::check(state, 1).stop();
  return 0;
}

int32
pauseSpriteAnimation(lua_State* state)
{
  ScriptTraits<SpriteAnimatorComponent>::check(state, 1).setPlaying(false);
  return 0;
}

/*
 * Resumes the animation where it was; one that finished stays at its end.
 */
int32
resumeSpriteAnimation(lua_State* state)
{
  SpriteAnimatorComponent& animator = ScriptTraits<SpriteAnimatorComponent>::check(state, 1);
  animator.setPlaying(!animator.isFinished() && !animator.getAnimationName().empty());
  return 0;
}

int32
isSpriteAnimationPlaying(lua_State* state)
{
  lua_pushboolean(state,
                  ScriptTraits<SpriteAnimatorComponent>::check(state, 1).isPlaying() ? 1 : 0);
  return 1;
}

int32
isSpriteAnimationFinished(lua_State* state)
{
  lua_pushboolean(state,
                  ScriptTraits<SpriteAnimatorComponent>::check(state, 1).isFinished() ? 1 : 0);
  return 1;
}

/*
 * The name of the animation playing or paused, nil without one.
 */
int32
getSpriteAnimationName(lua_State* state)
{
  const String& name =
      ScriptTraits<SpriteAnimatorComponent>::check(state, 1).getAnimationName();
  if (name.empty()) {
    lua_pushnil(state);
  }
  else {
    lua_pushlstring(state, name.data(), name.size());
  }
  return 1;
}

int32
setSpriteAnimationSpeed(lua_State* state)
{
  ScriptTraits<SpriteAnimatorComponent>::check(state, 1).setSpeed(
      static_cast<float>(luaL_checknumber(state, 2)));
  return 0;
}

int32
getSpriteAnimationSpeed(lua_State* state)
{
  lua_pushnumber(state, ScriptTraits<SpriteAnimatorComponent>::check(state, 1).getSpeed());
  return 1;
}

int32
setSpriteAnimationTime(lua_State* state)
{
  ScriptTraits<SpriteAnimatorComponent>::check(state, 1).setTime(
      static_cast<float>(luaL_checknumber(state, 2)));
  return 0;
}

int32
getSpriteAnimationTime(lua_State* state)
{
  lua_pushnumber(state, ScriptTraits<SpriteAnimatorComponent>::check(state, 1).getTime());
  return 1;
}

int32
setSpriteAnimatorAtlas(lua_State* state)
{
  SpriteAnimatorComponent& animator = ScriptTraits<SpriteAnimatorComponent>::check(state, 1);
  animator.setAtlas(checkAtlas(state, 2, "SpriteAnimator.setAtlas"));
  return 0;
}

void
setToString(lua_State* state, const ANSICHAR* metatableName, const lua_CFunction function)
{
  luaL_getmetatable(state, metatableName);
  lua_pushcfunction(state, function);
  lua_setfield(state, -2, "__tostring");
  lua_pop(state, 1);
}
} // namespace

/*
 */
int32
ScriptBindings::getSprite(lua_State* state)
{
  GameObject& gameObject = ScriptTraits<GameObject>::check(state, 1);
  if (!gameObject.getComponent<SpriteComponent>()) {
    lua_pushnil(state);
    return 1;
  }
  pushPartHandle(
      state, gameObject, SPRITE_USER_VALUE, ScriptHandleType<SpriteComponent>::NAME);
  return 1;
}

/*
 */
int32
ScriptBindings::getSpriteAnimator(lua_State* state)
{
  GameObject& gameObject = ScriptTraits<GameObject>::check(state, 1);
  if (!gameObject.getComponent<SpriteAnimatorComponent>()) {
    lua_pushnil(state);
    return 1;
  }
  pushPartHandle(state, gameObject, SPRITE_ANIMATOR_USER_VALUE,
                 ScriptHandleType<SpriteAnimatorComponent>::NAME);
  return 1;
}

/*
 */
void
ScriptBindings::registerSprites(lua_State* state)
{
  ScriptClass<ScriptTexture>(state)
      .rawStaticFunction("find", &findTexture, "fun(pathOrName: string): Texture?")
      .rawMethod("isValid", &isTextureValid, "fun(): boolean")
      .rawMethod("getName", &getTextureName, "fun(): string?")
      .rawMethod("getPath", &getTexturePath, "fun(): string?")
      .rawMethod("getSize", &getTextureSize, "fun(): integer, integer")
      .operators<ScriptOperator::Equal>();
  setToString(state, ScriptValueType<ScriptTexture>::NAME, &textureToString);

  ScriptClass<ScriptSpriteAtlas>(state)
      .rawStaticFunction("find", &findAtlas, "fun(pathOrName: string): Atlas?")
      .rawMethod("isValid", &isAtlasValid, "fun(): boolean")
      .rawMethod("getName", &getAtlasName, "fun(): string?")
      .rawMethod("getTexture", &getAtlasTexture, "fun(): Texture?")
      .rawMethod("getFrameCount", &getAtlasFrameCount, "fun(): integer")
      .rawMethod("findFrame", &findAtlasFrame, "fun(name: string): integer?")
      .rawMethod("getFrameName", &getAtlasFrameName, "fun(frame: integer|string): string")
      .rawMethod("getAnimationNames", &getAtlasAnimationNames, "fun(): string[]")
      .operators<ScriptOperator::Equal>();
  setToString(state, ScriptValueType<ScriptSpriteAtlas>::NAME, &atlasToString);

  ScriptClass<SpriteComponent>(state)
      .rawMethod("getGameObject", &getSpriteGameObject, "fun(): GameObject")
      .rawMethod("setTexture", &setSpriteTexture, "fun(texture?: Texture|string)")
      .rawMethod("setAtlas", &setSpriteAtlas,
                 "fun(atlas: Atlas|string, frame?: integer|string)")
      .rawMethod("setFrame", &setSpriteFrame, "fun(frame: integer|string)")
      .rawMethod("getFrame", &getSpriteFrame, "fun(): integer?")
      .rawMethod("setColor", &setSpriteColor, "fun(color: Color)")
      .rawMethod("getColor", &getSpriteColor, "fun(): Color")
      .rawMethod("setFlip", &setSpriteFlip, "fun(flipX: boolean, flipY?: boolean)")
      .rawMethod("isFlippedX", &isSpriteFlippedX, "fun(): boolean")
      .rawMethod("isFlippedY", &isSpriteFlippedY, "fun(): boolean")
      .rawMethod("setSorting", &setSpriteSorting, "fun(layer: integer, order?: integer)")
      .rawMethod("getSorting", &getSpriteSorting, "fun(): integer, integer")
      .rawMethod("setVisible", &setSpriteVisible, "fun(visible: boolean)")
      .rawMethod("isVisible", &isSpriteVisible, "fun(): boolean");

  ScriptClass<SpriteAnimatorComponent>(state)
      .rawMethod("getGameObject", &getSpriteAnimatorGameObject, "fun(): GameObject")
      .rawMethod("setAtlas", &setSpriteAnimatorAtlas, "fun(atlas: Atlas|string)")
      .rawMethod("play", &playSpriteAnimation, "fun(animation: string, restart?: boolean)")
      .rawMethod("stop", &stopSpriteAnimation, "fun()")
      .rawMethod("pause", &pauseSpriteAnimation, "fun()")
      .rawMethod("resume", &resumeSpriteAnimation, "fun()")
      .rawMethod("isPlaying", &isSpriteAnimationPlaying, "fun(): boolean")
      .rawMethod("isFinished", &isSpriteAnimationFinished, "fun(): boolean")
      .rawMethod("getAnimation", &getSpriteAnimationName, "fun(): string?")
      .rawMethod("setSpeed", &setSpriteAnimationSpeed, "fun(speed: number)")
      .rawMethod("getSpeed", &getSpriteAnimationSpeed, "fun(): number")
      .rawMethod("setTime", &setSpriteAnimationTime, "fun(seconds: number)")
      .rawMethod("getTime", &getSpriteAnimationTime, "fun(): number");

  ScriptClass<ScriptSpriteStyle>(state)
      .rawMethod("setColor", &setStyleColor, "fun(color: Color)")
      .rawMethod("setSize", &setStyleSize, "fun(size: Vector2)")
      .rawMethod("getSize", &getStyleSize, "fun(): Vector2")
      .rawMethod("setSorting", &setStyleSorting, "fun(layer: integer, order?: integer)")
      .rawMethod("copy", &copyStyle, "fun(): SpriteStyle");

  ScriptTable(state, "Sprites",
              "Sprites drawn this frame without GameObjects, in the YZ plane (x is right, y "
              "up). Drawn in update they last one frame; in fixedUpdate, until the next "
              "step. They sort with sprite components by layer and order.")
      .rawFunction("makeStyle", &makeStyle,
                   "fun(options?: {texture?: Texture|string, atlas?: Atlas|string, frame?: "
                   "integer|string, sourceRect?: Vector4, size?: "
                   "Vector2, pixelsPerUnit?: number, pivot?: Vector2, color?: Color, layer?: "
                   "integer, order?: integer, blend?: \"Alpha\"|\"Additive\", filter?: "
                   "\"Point\"|\"Linear\", flipX?: boolean, flipY?: boolean, depth?: number}): "
                   "SpriteStyle")
      .rawFunction("draw", &drawSprite,
                   "fun(look: SpriteStyle|Texture, x: number, y: number, rotation?: number, "
                   "scale?: number, color?: Color)");
}

} // namespace chEngineSDK
