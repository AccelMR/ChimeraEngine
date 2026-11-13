/************************************************************************/
/**
 * @file chFloatingNumberBindings.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  The floating number types scripts see.
 */
/************************************************************************/
#include "chFloatingNumberBindings.h"

#include "chAssetManager.h"
#include "chFloatingNumberSystemComponent.h"
#include "chFontAsset.h"
#include "chGameObject.h"
#include "chMath.h"
#include "chScriptApi.h"
#include "chScriptBinder.h"
#include "chScriptManager.h"
#include "chScriptMathTypes.h"
#include "chScriptObjectTypes.h"
#include "chStringUtils.h"

#include "lauxlib.h"
#include "lua.h"

namespace chEngineSDK {

CH_SCRIPT_HANDLE_TYPE(FloatingNumberSystemComponent, "FloatingNumbers");

namespace {
constexpr const ANSICHAR* kSystemName = FloatingNumberBindings::SYSTEM_TYPE_NAME;
constexpr const ANSICHAR* kBlendNames[] = {"Alpha", "Additive"};
constexpr const ANSICHAR* kNotationNames[] = {"Plain", "Suffix", "Scientific", "Engineering"};

FloatingNumberSystemComponent&
checkSystem(lua_State* state, const int32 index)
{
  GameObject& owner = ScriptPartHandle::checkOwner(state, index, kSystemName);
  FloatingNumberSystemComponent* const system =
      owner.getComponent<FloatingNumberSystemComponent>();
  if (!system) {
    luaL_error(state, "GameObject '%s' has no FloatingNumbers", owner.getName().c_str());
  }
  return *system;
}

NODISCARD float
checkFloat(lua_State* state, const int32 index)
{
  return static_cast<float>(luaL_checknumber(state, index));
}

template <SIZE_T Count>
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
  luaL_error(state, "FloatingNumbers: %s cannot be '%s'", option, String(text).c_str());
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

NODISCARD int32
checkDecimals(lua_State* state, const int32 index)
{
  const lua_Integer value = luaL_checkinteger(state, index);
  if (value < 0 || value > NumberFormat::MAX_DECIMALS) {
    luaL_error(state, "decimals must be between 0 and %d", NumberFormat::MAX_DECIMALS);
  }
  return static_cast<int32>(value);
}

/*
 * A Font value (nil for the default font) or a path or name, loaded.
 */
SPtr<FontAsset>
checkFont(lua_State* state, const int32 index)
{
  if (const auto* value = static_cast<const ScriptFont*>(
          luaL_testudata(state, index, ScriptValueType<ScriptFont>::NAME))) {
    if (value->font.isNull()) {
      return nullptr;
    }
  }
  if (!AssetManager::isStarted()) {
    luaL_error(state, "FloatingNumbers: there are no assets to search");
  }
  AssetManager& assetManager = AssetManager::instance();
  SPtr<IAsset> asset;
  if (const auto* value = static_cast<const ScriptFont*>(
          luaL_testudata(state, index, ScriptValueType<ScriptFont>::NAME))) {
    asset = assetManager.getAsset(value->font);
  }
  else {
    asset = assetManager.findAsset(AssetTypeTraits<FontAsset>::getTypeId(),
                                   ScriptTraits<StringView>::check(state, index));
  }
  SPtr<FontAsset> font = asset ? asset->as<FontAsset>() : nullptr;
  if (!font) {
    luaL_error(state, "FloatingNumbers: there is no such font");
  }
  if (!font->isLoaded() && !assetManager.syncLoadAsset(font)) {
    luaL_error(state, "FloatingNumbers: font %s could not be loaded", font->getName());
  }
  return font;
}

// ---------- GameObject ----------

int32
getFloatingNumbers(lua_State* state)
{
  GameObject& gameObject = ScriptTraits<GameObject>::check(state, 1);
  if (!gameObject.getComponent<FloatingNumberSystemComponent>()) {
    lua_pushnil(state);
    return 1;
  }
  ScriptPartHandle::push(state, gameObject, kSystemName);
  return 1;
}

/*
 * gameObject:addFloatingNumbers(): adds a system and gives it; an object that has one gets
 * that one.
 */
int32
addFloatingNumbers(lua_State* state)
{
  GameObject& gameObject = ScriptTraits<GameObject>::check(state, 1);
  if (!gameObject.getComponent<FloatingNumberSystemComponent>()) {
    gameObject.addComponent<FloatingNumberSystemComponent>();
  }
  ScriptPartHandle::push(state, gameObject, kSystemName);
  return 1;
}

// ---------- FloatingNumbers ----------

/*
 * configure{...}: font, size, capacity, layer, order, blend, monospaceDigits, the motion
 * (angle, spread, speed, speedVariance, gravity, drag, lifetime, lifetimeVariance, fadeIn,
 * fadeOut) and the number format (notation, decimals, trimZeros, plainBelow, plainDecimals).
 * A misspelt key raises.
 */
int32
configure(lua_State* state)
{
  FloatingNumberSystemComponent& system = checkSystem(state, 1);
  luaL_checktype(state, 2, LUA_TTABLE);
  FloatingNumberMotion motion = system.getMotion();
  NumberFormatDesc format = system.getNumberFormat();
  uint32 capacity = 0;
  lua_pushnil(state);
  while (lua_next(state, 2) != 0) {
    const int32 value = lua_gettop(state);
    if (lua_type(state, -2) != LUA_TSTRING) {
      return luaL_error(state, "FloatingNumbers.configure: options are named fields");
    }
    const StringView key = lua_tostring(state, -2);
    if (key == "font") {
      system.setFont(checkFont(state, value));
    }
    else if (key == "size") {
      system.setSize(checkFloat(state, value));
    }
    else if (key == "capacity") {
      const lua_Integer count = luaL_checkinteger(state, value);
      if (count < 1 || count > FloatingNumberSystemComponent::MAX_CAPACITY) {
        return luaL_error(state, "capacity must be between 1 and %d",
                          static_cast<int32>(FloatingNumberSystemComponent::MAX_CAPACITY));
      }
      capacity = static_cast<uint32>(count);
    }
    else if (key == "layer") {
      system.setSortingLayer(checkInt16(state, value, "layer"));
    }
    else if (key == "order") {
      system.setOrder(checkInt16(state, value, "order"));
    }
    else if (key == "blend") {
      system.setBlendMode(static_cast<SpriteBlendMode>(checkName(state, value, "blend", kBlendNames)));
    }
    else if (key == "monospaceDigits") {
      system.setMonospaceDigits(lua_toboolean(state, value) != 0);
    }
    else if (key == "angle") {
      motion.angle = checkFloat(state, value);
    }
    else if (key == "spread") {
      motion.spread = checkFloat(state, value);
    }
    else if (key == "speed") {
      motion.speed = checkFloat(state, value);
    }
    else if (key == "speedVariance") {
      motion.speedVariance = checkFloat(state, value);
    }
    else if (key == "gravity") {
      motion.gravity = ScriptTraits<Vector2>::check(state, value);
    }
    else if (key == "drag") {
      motion.drag = checkFloat(state, value);
    }
    else if (key == "lifetime") {
      motion.lifetime = checkFloat(state, value);
    }
    else if (key == "lifetimeVariance") {
      motion.lifetimeVariance = checkFloat(state, value);
    }
    else if (key == "fadeIn") {
      motion.fadeIn = checkFloat(state, value);
    }
    else if (key == "fadeOut") {
      motion.fadeOut = checkFloat(state, value);
    }
    else if (key == "notation") {
      format.notation = static_cast<NumberNotation>(checkName(state, value, "notation", kNotationNames));
    }
    else if (key == "decimals") {
      format.decimals = checkDecimals(state, value);
    }
    else if (key == "trimZeros") {
      format.bTrimZeros = lua_toboolean(state, value) != 0;
    }
    else if (key == "plainBelow") {
      format.plainBelow = luaL_checknumber(state, value);
    }
    else if (key == "plainDecimals") {
      format.plainDecimals = checkDecimals(state, value);
    }
    else {
      return luaL_error(state, "FloatingNumbers.configure: unknown option '%s'",
                        lua_tostring(state, -2));
    }
    lua_pop(state, 1);
  }
  system.setMotion(motion);
  system.setNumberFormat(format);
  if (capacity > 0) {
    system.setCapacity(capacity);
  }
  return 0;
}

/*
 * One tier from the table at index; a misspelt key raises.
 */
FloatingNumberTier
readTier(lua_State* state, const int32 index)
{
  luaL_checktype(state, index, LUA_TTABLE);
  FloatingNumberTier tier;
  lua_pushnil(state);
  while (lua_next(state, index) != 0) {
    const int32 value = lua_gettop(state);
    if (lua_type(state, -2) != LUA_TSTRING) {
      luaL_error(state, "FloatingNumbers.setTiers: tier options are named fields");
    }
    const StringView key = lua_tostring(state, -2);
    if (key == "min") {
      tier.minValue = luaL_checknumber(state, value);
    }
    else if (key == "color") {
      tier.color = ScriptTraits<LinearColor>::check(state, value);
      tier.flashColor = tier.color;
    }
    else if (key == "flashColor") {
      tier.flashColor = ScriptTraits<LinearColor>::check(state, value);
    }
    else if (key == "outlineColor") {
      tier.outlineColor = ScriptTraits<LinearColor>::check(state, value);
    }
    else if (key == "outline") {
      tier.outlineWidth = checkFloat(state, value);
    }
    else if (key == "scale") {
      tier.scale = checkFloat(state, value);
    }
    else if (key == "pop") {
      tier.popScale = checkFloat(state, value);
    }
    else if (key == "popDuration") {
      tier.popDuration = checkFloat(state, value);
    }
    else if (key == "shake") {
      tier.shakeAmplitude = checkFloat(state, value);
    }
    else if (key == "shakeFrequency") {
      tier.shakeFrequency = checkFloat(state, value);
    }
    else if (key == "shakeDuration") {
      tier.shakeDuration = checkFloat(state, value);
    }
    else if (key == "flash") {
      tier.flashDuration = checkFloat(state, value);
    }
    else if (key == "flashRate") {
      tier.flashRate = checkFloat(state, value);
    }
    else {
      luaL_error(state, "FloatingNumbers.setTiers: unknown tier option '%s'",
                 lua_tostring(state, -2));
    }
    lua_pop(state, 1);
  }
  return tier;
}

/*
 * setTiers({ {min = 0, ...}, {min = 1000, ...} }): up to 8, in any order.
 */
int32
setTiers(lua_State* state)
{
  FloatingNumberSystemComponent& system = checkSystem(state, 1);
  luaL_checktype(state, 2, LUA_TTABLE);
  const lua_Integer count = luaL_len(state, 2);
  if (count > GpuGlyphRunMotion::MAX_TIERS) {
    return luaL_error(state, "FloatingNumbers.setTiers: at most %d tiers",
                      static_cast<int32>(GpuGlyphRunMotion::MAX_TIERS));
  }
  Array<FloatingNumberTier, GpuGlyphRunMotion::MAX_TIERS> tiers;
  for (lua_Integer i = 1; i <= count; ++i) {
    lua_rawgeti(state, 2, i);
    tiers[static_cast<SIZE_T>(i - 1)] = readTier(state, lua_gettop(state));
    lua_pop(state, 1);
  }
  system.setTiers(Span<const FloatingNumberTier>(tiers.data(), static_cast<SIZE_T>(count)));
  return 0;
}

/*
 * The optional table of spawn and spawnText: tier (from 1), tint, scale.
 */
FloatingNumberSpawn
readSpawnOptions(lua_State* state, const int32 index)
{
  FloatingNumberSpawn options;
  if (lua_isnoneornil(state, index)) {
    return options;
  }
  luaL_checktype(state, index, LUA_TTABLE);
  lua_pushnil(state);
  while (lua_next(state, index) != 0) {
    const int32 value = lua_gettop(state);
    if (lua_type(state, -2) != LUA_TSTRING) {
      luaL_error(state, "FloatingNumbers.spawn: options are named fields");
    }
    const StringView key = lua_tostring(state, -2);
    if (key == "tier") {
      const lua_Integer tier = luaL_checkinteger(state, value);
      if (tier < 1 || tier > GpuGlyphRunMotion::MAX_TIERS) {
        luaL_error(state, "tier must be between 1 and %d",
                   static_cast<int32>(GpuGlyphRunMotion::MAX_TIERS));
      }
      options.tier = static_cast<int32>(tier - 1);
    }
    else if (key == "tint") {
      options.tint = ScriptTraits<LinearColor>::check(state, value);
    }
    else if (key == "scale") {
      options.scale = checkFloat(state, value);
    }
    else {
      luaL_error(state, "FloatingNumbers.spawn: unknown option '%s'", lua_tostring(state, -2));
    }
    lua_pop(state, 1);
  }
  return options;
}

/*
 * spawn(value, x, y, options?): the number, at (x, y) in the YZ plane. No allocation.
 */
int32
spawn(lua_State* state)
{
  FloatingNumberSystemComponent& system = checkSystem(state, 1);
  const double value = luaL_checknumber(state, 2);
  const Vector2 position(checkFloat(state, 3), checkFloat(state, 4));
  lua_pushboolean(state, system.spawn(value, position, readSpawnOptions(state, 5)) ? 1 : 0);
  return 1;
}

int32
spawnText(lua_State* state)
{
  FloatingNumberSystemComponent& system = checkSystem(state, 1);
  const StringView text = ScriptTraits<StringView>::check(state, 2);
  const Vector2 position(checkFloat(state, 3), checkFloat(state, 4));
  lua_pushboolean(state, system.spawnText(text, position, readSpawnOptions(state, 5)) ? 1 : 0);
  return 1;
}

int32
clear(lua_State* state)
{
  checkSystem(state, 1).clear();
  return 0;
}

int32
getCount(lua_State* state)
{
  lua_pushinteger(state, static_cast<lua_Integer>(checkSystem(state, 1).getLiveCount()));
  return 1;
}

int32
getCapacity(lua_State* state)
{
  lua_pushinteger(state, static_cast<lua_Integer>(checkSystem(state, 1).getCapacity()));
  return 1;
}

int32
getSystemObject(lua_State* state)
{
  ScriptTraits<GameObject>::push(state, ScriptPartHandle::checkOwner(state, 1, kSystemName));
  return 1;
}

int32
systemToString(lua_State* state)
{
  if (const GameObject* owner = ScriptPartHandle::testOwner(state, 1, kSystemName)) {
    lua_pushfstring(state, "FloatingNumbers(%s)", owner->getName().c_str());
  }
  else {
    lua_pushliteral(state, "FloatingNumbers(destroyed)");
  }
  return 1;
}

/*
 * The shapes of the tables configure, setTiers and spawn take, for code editors.
 */
void
describeShapes(lua_State* state)
{
  ScriptApiDescription* description = ScriptApiDescription::find(state);
  if (!description) {
    return;
  }
  ScriptApiType& options =
      description->addType("FloatingNumberOptions", ScriptApiTypeKind::Shape);
  options.description = "What FloatingNumbers:configure sets. Angles are degrees "
                        "counterclockwise from +Y (90 is up); world units and seconds.";
  options.fields = {{"font?", "Font|string"},      {"size?", "number"},
                    {"capacity?", "integer"},      {"layer?", "integer"},
                    {"order?", "integer"},         {"blend?", "\"Alpha\"|\"Additive\""},
                    {"monospaceDigits?", "boolean"}, {"angle?", "number"},
                    {"spread?", "number"},         {"speed?", "number"},
                    {"speedVariance?", "number"},  {"gravity?", "Vector2"},
                    {"drag?", "number"},           {"lifetime?", "number"},
                    {"lifetimeVariance?", "number"}, {"fadeIn?", "number"},
                    {"fadeOut?", "number"},        {"notation?", "NumberNotation"},
                    {"decimals?", "integer"},      {"trimZeros?", "boolean"},
                    {"plainBelow?", "number"},     {"plainDecimals?", "integer"}};

  ScriptApiType& tier = description->addType("FloatingNumberTier", ScriptApiTypeKind::Shape);
  tier.description = "How numbers from min up look: color, an outline, a size times the "
                     "system's, a pop (born pop times bigger, settling over popDuration), a "
                     "shake (world units over shakeDuration) and a flash (seconds of "
                     "blinking to flashColor).";
  tier.fields = {{"min?", "number"},          {"color?", "Color"},
                 {"flashColor?", "Color"},    {"outlineColor?", "Color"},
                 {"outline?", "number"},      {"scale?", "number"},
                 {"pop?", "number"},          {"popDuration?", "number"},
                 {"shake?", "number"},        {"shakeFrequency?", "number"},
                 {"shakeDuration?", "number"}, {"flash?", "number"},
                 {"flashRate?", "number"}};

  ScriptApiType& spawnOptions =
      description->addType("FloatingNumberSpawn", ScriptApiTypeKind::Shape);
  spawnOptions.description = "Changes of one spawn: a tier from 1 instead of the one the "
                             "value picks, a tint times the tier's color, a scale.";
  spawnOptions.fields = {{"tier?", "integer"}, {"tint?", "Color"}, {"scale?", "number"}};
}
} // namespace

/*
 */
void
FloatingNumberBindings::registerBindings(lua_State* state)
{
  ScriptClass<GameObject>(state)
      .rawMethod("getFloatingNumbers", &getFloatingNumbers, "fun(): FloatingNumbers?")
      .rawMethod("addFloatingNumbers", &addFloatingNumbers, "fun(): FloatingNumbers");

  ScriptClass<FloatingNumberSystemComponent>(state)
      .rawMethod("configure", &configure, "fun(options: FloatingNumberOptions)")
      .rawMethod("setTiers", &setTiers, "fun(tiers: FloatingNumberTier[])")
      .rawMethod("spawn", &spawn,
                 "fun(value: number, x: number, y: number, options?: FloatingNumberSpawn): "
                 "boolean")
      .rawMethod("spawnText", &spawnText,
                 "fun(text: string, x: number, y: number, options?: FloatingNumberSpawn): "
                 "boolean")
      .rawMethod("clear", &clear, "fun()")
      .rawMethod("getCount", &getCount, "fun(): integer")
      .rawMethod("getCapacity", &getCapacity, "fun(): integer")
      .rawMethod("getGameObject", &getSystemObject, "fun(): GameObject");

  luaL_getmetatable(state, kSystemName);
  lua_pushcfunction(state, &systemToString);
  lua_setfield(state, -2, "__tostring");
  lua_pop(state, 1);
  describeShapes(state);
}

} // namespace chEngineSDK
