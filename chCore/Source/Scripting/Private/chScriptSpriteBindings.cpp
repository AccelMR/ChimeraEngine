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
#include "chLinearColor.h"
#include "chPath.h"
#include "chScene.h"
#include "chScriptManager.h"
#include "chScriptMathTypes.h"
#include "chScriptObjectTypes.h"
#include "chSpriteComponent.h"
#include "chSpriteDraw.h"
#include "chStringUtils.h"
#include "chTextureAsset.h"

namespace chEngineSDK {
namespace {
/*
 * What a sprite looks like, resolved once by Sprites.makeStyle so each Sprites.draw only
 * places it. The texture's bindless index is read when the style is made.
 */
struct ScriptSpriteStyle
{
  UUID texture;
  float uvRect[4];
  float size[2];
  float pivot[2];
  float color[4];
  float depth;
  uint32 textureAndFlags;
  int16 sortingLayer;
  int16 order;
  uint8 blendMode;
  uint8 filter;
  uint8 padding[2];
};
} // namespace

CH_SCRIPT_VALUE_TYPE(ScriptSpriteStyle, "SpriteStyle");

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
    luaL_error(state, "Sprites.makeStyle: %s must be between %d and %d", option, INT16_MIN,
               INT16_MAX);
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
  if (lua_istable(state, 1) && lua_getfield(state, 1, "texture") != LUA_TNIL) {
    texture = checkTexture(state, -1, kFunction);
  }
  lua_settop(state, 1);
  ScriptSpriteStyle style = makeDefaultStyle(texture.get());
  if (!lua_istable(state, 1)) {
    ScriptTraits<ScriptSpriteStyle>::push(state, style);
    return 1;
  }

  Vector2 sourcePosition = Vector2::ZERO;
  Vector2 sourceSize = Vector2::ZERO;
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
    if (key == "texture") {
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
                   "fun(options?: {texture?: Texture|string, sourceRect?: Vector4, size?: "
                   "Vector2, pixelsPerUnit?: number, pivot?: Vector2, color?: Color, layer?: "
                   "integer, order?: integer, blend?: \"Alpha\"|\"Additive\", filter?: "
                   "\"Point\"|\"Linear\", flipX?: boolean, flipY?: boolean, depth?: number}): "
                   "SpriteStyle")
      .rawFunction("draw", &drawSprite,
                   "fun(look: SpriteStyle|Texture, x: number, y: number, rotation?: number, "
                   "scale?: number, color?: Color)");
}

} // namespace chEngineSDK
