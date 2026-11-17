/************************************************************************/
/**
 * @file chParticleBindings.cpp
 * @author AccelMR
 * @date 2026/11/16
 * @brief
 *  The particle types scripts see.
 */
/************************************************************************/
#include "chParticleBindings.h"

#include "chAgeMotionBindings.h"
#include "chAssetManager.h"
#include "chGameObject.h"
#include "chParticleSystemComponent.h"
#include "chScriptApi.h"
#include "chScriptBinder.h"
#include "chScriptMathTypes.h"
#include "chScriptObjectTypes.h"
#include "chSpriteAtlasAsset.h"
#include "chStringUtils.h"
#include "chTextureAsset.h"

#include "lauxlib.h"
#include "lua.h"

namespace chEngineSDK {

CH_SCRIPT_HANDLE_TYPE(ParticleSystemComponent, "Particles");

namespace {
constexpr const ANSICHAR* kSystemName = ParticleBindings::SYSTEM_TYPE_NAME;
constexpr const ANSICHAR* kBlendNames[] = {"Alpha", "Additive"};
constexpr const ANSICHAR* kFilterNames[] = {"Point", "Linear"};
constexpr const ANSICHAR* kShapeNames[] = {"Point", "Circle", "CircleEdge", "Rectangle", "Line"};
constexpr uint32 kMaxKeys = GpuParticleLook::MAX_KEYS;

ParticleSystemComponent&
checkSystem(lua_State* state, const int32 index)
{
  GameObject& owner = ScriptPartHandle::checkOwner(state, index, kSystemName);
  ParticleSystemComponent* const system = owner.getComponent<ParticleSystemComponent>();
  if (!system) {
    luaL_error(state, "GameObject '%s' has no Particles", owner.getName().c_str());
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
  luaL_error(state, "Particles: %s cannot be '%s'", option, String(text).c_str());
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

NODISCARD uint32
checkCount(lua_State* state, const int32 index, const ANSICHAR* option)
{
  const lua_Integer value = luaL_checkinteger(state, index);
  if (value < 0 || value > ParticleSystemComponent::MAX_CAPACITY) {
    luaL_error(state, "%s must be between 0 and %d", option,
               static_cast<int32>(ParticleSystemComponent::MAX_CAPACITY));
  }
  return static_cast<uint32>(value);
}

/*
 * A number gives both sides; a Vector2 each.
 */
NODISCARD Vector2
checkSize(lua_State* state, const int32 index)
{
  if (lua_type(state, index) == LUA_TNUMBER) {
    const float size = checkFloat(state, index);
    return Vector2(size, size);
  }
  return ScriptTraits<Vector2>::check(state, index);
}

/*
 * An asset of type Asset: its script value (ValueType holding the UUID in Member), a path
 * or a name; nil gives none.
 */
template <typename Asset, typename ValueType, UUID ValueType::*Member>
NODISCARD UUID
checkAssetId(lua_State* state, const int32 index, const ANSICHAR* what)
{
  if (lua_isnoneornil(state, index)) {
    return UUID::null();
  }
  if (const auto* value = static_cast<const ValueType*>(
          luaL_testudata(state, index, ScriptValueType<ValueType>::NAME))) {
    return value->*Member;
  }
  if (!AssetManager::isStarted()) {
    luaL_error(state, "Particles: there are no assets to search");
  }
  const SPtr<IAsset> asset = AssetManager::instance().findAsset(
      AssetTypeTraits<Asset>::getTypeId(), ScriptTraits<StringView>::check(state, index));
  if (!asset) {
    luaL_error(state, "Particles: there is no such %s", what);
  }
  return asset->getUUID();
}

// ---------- GameObject ----------

int32
getParticles(lua_State* state)
{
  GameObject& gameObject = ScriptTraits<GameObject>::check(state, 1);
  if (!gameObject.getComponent<ParticleSystemComponent>()) {
    lua_pushnil(state);
    return 1;
  }
  ScriptPartHandle::push(state, gameObject, kSystemName);
  return 1;
}

/*
 * gameObject:addParticles(): adds a system and gives it; an object that has one gets that
 * one.
 */
int32
addParticles(lua_State* state)
{
  GameObject& gameObject = ScriptTraits<GameObject>::check(state, 1);
  if (!gameObject.getComponent<ParticleSystemComponent>()) {
    gameObject.addComponent<ParticleSystemComponent>();
  }
  ScriptPartHandle::push(state, gameObject, kSystemName);
  return 1;
}

// ---------- Particles ----------

/*
 * configure{...}: the emission, the look and the shared motion keys. A misspelt key
 * raises.
 */
int32
configure(lua_State* state)
{
  ParticleSystemComponent& system = checkSystem(state, 1);
  luaL_checktype(state, 2, LUA_TTABLE);
  ParticleEmission emission = system.getEmission();
  ParticleLook look = system.getLook();
  AgeMotion motion = system.getMotion();
  uint32 capacity = 0;
  lua_pushnil(state);
  while (lua_next(state, 2) != 0) {
    const int32 value = lua_gettop(state);
    if (lua_type(state, -2) != LUA_TSTRING) {
      return luaL_error(state, "Particles.configure: options are named fields");
    }
    const StringView key = lua_tostring(state, -2);
    if (key == "capacity") {
      capacity = checkCount(state, value, "capacity");
      if (capacity == 0) {
        return luaL_error(state, "capacity must be at least 1");
      }
    }
    else if (key == "rate") {
      emission.rate = checkFloat(state, value);
    }
    else if (key == "duration") {
      emission.duration = checkFloat(state, value);
    }
    else if (key == "loop") {
      emission.bLoop = lua_toboolean(state, value) != 0;
    }
    else if (key == "playOnStart") {
      emission.bPlayOnStart = lua_toboolean(state, value) != 0;
    }
    else if (key == "localSpace") {
      emission.bLocalSpace = lua_toboolean(state, value) != 0;
    }
    else if (key == "shape") {
      emission.shape = static_cast<ParticleShape>(checkName(state, value, "shape", kShapeNames));
    }
    else if (key == "shapeSize") {
      emission.shapeSize = checkSize(state, value);
    }
    else if (key == "texture") {
      look.texture = checkAssetId<TextureAsset, ScriptTexture, &ScriptTexture::texture>(
          state, value, "texture");
    }
    else if (key == "size") {
      look.size = checkSize(state, value);
    }
    else if (key == "minScale") {
      look.minScale = checkFloat(state, value);
    }
    else if (key == "maxScale") {
      look.maxScale = checkFloat(state, value);
    }
    else if (key == "color") {
      look.color = ScriptTraits<LinearColor>::check(state, value);
    }
    else if (key == "color2") {
      look.color2 = ScriptTraits<LinearColor>::check(state, value);
    }
    else if (key == "faceVelocity") {
      look.rotationMode = lua_toboolean(state, value) != 0 ? ParticleRotation::FaceVelocity
                                                           : ParticleRotation::Random;
    }
    else if (key == "rotation") {
      look.rotation = checkFloat(state, value);
    }
    else if (key == "rotationVariance") {
      look.rotationVariance = checkFloat(state, value);
    }
    else if (key == "spin") {
      look.spin = checkFloat(state, value);
    }
    else if (key == "spinVariance") {
      look.spinVariance = checkFloat(state, value);
    }
    else if (key == "depth") {
      look.depth = checkFloat(state, value);
    }
    else if (key == "layer") {
      look.sortingLayer = checkInt16(state, value, "layer");
    }
    else if (key == "order") {
      look.order = checkInt16(state, value, "order");
    }
    else if (key == "blend") {
      look.blendMode =
          static_cast<SpriteBlendMode>(checkName(state, value, "blend", kBlendNames));
    }
    else if (key == "filter") {
      look.filter = static_cast<SpriteFilter>(checkName(state, value, "filter", kFilterNames));
    }
    else if (!AgeMotionBindings::readOption(state, key, value, motion)) {
      return luaL_error(state, "Particles.configure: unknown option '%s'",
                        lua_tostring(state, -2));
    }
    lua_pop(state, 1);
  }
  system.setEmission(emission);
  system.setLook(look);
  system.setMotion(motion);
  if (capacity > 0) {
    system.setCapacity(capacity);
  }
  return 0;
}

/*
 * setLook(style): the texture part, size, color, depth, sorting, blend and filter of a
 * SpriteStyle; the other look values stay.
 */
int32
setLook(lua_State* state)
{
  ParticleSystemComponent& system = checkSystem(state, 1);
  const ScriptSpriteStyle& style = ScriptTraits<ScriptSpriteStyle>::check(state, 2);
  ParticleLook look = system.getLook();
  look.texture = style.texture;
  for (uint32 i = 0; i < 4; ++i) {
    look.uvRect[i] = style.uvRect[i];
  }
  look.size = Vector2(style.size[0], style.size[1]);
  look.color = LinearColor(style.color[0], style.color[1], style.color[2], style.color[3]);
  look.color2 = look.color;
  look.depth = style.depth;
  look.sortingLayer = style.sortingLayer;
  look.order = style.order;
  look.blendMode = static_cast<SpriteBlendMode>(style.blendMode);
  look.filter = static_cast<SpriteFilter>(style.filter);
  system.setLook(look);
  return 0;
}

/*
 * setFlipbook(atlas?, firstFrame?, frameCount?, framesPerSecond?): frames from 1; nil turns
 * it off.
 */
int32
setFlipbook(lua_State* state)
{
  ParticleSystemComponent& system = checkSystem(state, 1);
  ParticleFlipbook flipbook;
  flipbook.atlas =
      checkAssetId<SpriteAtlasAsset, ScriptSpriteAtlas, &ScriptSpriteAtlas::atlas>(
          state, 2, "atlas");
  const lua_Integer firstFrame = luaL_optinteger(state, 3, 1);
  if (firstFrame < 1) {
    return luaL_error(state, "Particles.setFlipbook: frames count from 1");
  }
  flipbook.firstFrame = static_cast<uint32>(firstFrame - 1);
  flipbook.frameCount = lua_isnoneornil(state, 4) ? 0u : checkCount(state, 4, "frameCount");
  flipbook.framesPerSecond = static_cast<float>(luaL_optnumber(state, 5, 0.0));
  system.setFlipbook(flipbook);
  return 0;
}

/*
 * The time of the key at index of the list at listIndex, then its value left on the stack.
 */
float
readKeyTime(lua_State* state,
            const int32 listIndex,
            const lua_Integer index,
            const ANSICHAR* name)
{
  lua_rawgeti(state, listIndex, index);
  if (!lua_istable(state, -1)) {
    luaL_error(state, "Particles.%s: key %d must be { time, value }", name,
               static_cast<int32>(index));
  }
  lua_rawgeti(state, -1, 1);
  const float time = checkFloat(state, -1);
  lua_pop(state, 1);
  lua_rawgeti(state, -1, 2);
  return time;
}

NODISCARD lua_Integer
checkKeyCount(lua_State* state, const ANSICHAR* name)
{
  luaL_checktype(state, 2, LUA_TTABLE);
  const lua_Integer count = static_cast<lua_Integer>(lua_rawlen(state, 2));
  if (count > kMaxKeys) {
    luaL_error(state, "Particles.%s: at most %d keys", name, static_cast<int32>(kMaxKeys));
  }
  return count;
}

/*
 * setColorGradient{ {0, Color(...)}, {1, Color(...)} }: times from 0 (birth) to 1.
 */
int32
setColorGradient(lua_State* state)
{
  ParticleSystemComponent& system = checkSystem(state, 1);
  const lua_Integer count = checkKeyCount(state, "setColorGradient");
  ParticleColorKey keys[kMaxKeys];
  for (lua_Integer i = 1; i <= count; ++i) {
    keys[i - 1].time = readKeyTime(state, 2, i, "setColorGradient");
    keys[i - 1].color = ScriptTraits<LinearColor>::check(state, -1);
    lua_pop(state, 2);
  }
  system.setColorGradient(Span<const ParticleColorKey>(keys, static_cast<SIZE_T>(count)));
  return 0;
}

/*
 * setSizeCurve{ {0, 1}, {1, 0} }: times from 0 (birth) to 1, sizes times the look's.
 */
int32
setSizeCurve(lua_State* state)
{
  ParticleSystemComponent& system = checkSystem(state, 1);
  const lua_Integer count = checkKeyCount(state, "setSizeCurve");
  ParticleSizeKey keys[kMaxKeys];
  for (lua_Integer i = 1; i <= count; ++i) {
    keys[i - 1].time = readKeyTime(state, 2, i, "setSizeCurve");
    keys[i - 1].size = checkFloat(state, -1);
    lua_pop(state, 2);
  }
  system.setSizeCurve(Span<const ParticleSizeKey>(keys, static_cast<SIZE_T>(count)));
  return 0;
}

/*
 * setBursts{ {time = 0, count = 30, cycles = 1, interval = 0.5}, ... }; a misspelt key
 * raises.
 */
int32
setBursts(lua_State* state)
{
  ParticleSystemComponent& system = checkSystem(state, 1);
  luaL_checktype(state, 2, LUA_TTABLE);
  const lua_Integer count = static_cast<lua_Integer>(lua_rawlen(state, 2));
  if (count > ParticleSystemComponent::MAX_BURSTS) {
    return luaL_error(state, "Particles.setBursts: at most %d bursts",
                      static_cast<int32>(ParticleSystemComponent::MAX_BURSTS));
  }
  ParticleBurst bursts[ParticleSystemComponent::MAX_BURSTS];
  for (lua_Integer i = 1; i <= count; ++i) {
    lua_rawgeti(state, 2, i);
    const int32 table = lua_gettop(state);
    luaL_checktype(state, table, LUA_TTABLE);
    ParticleBurst& burst = bursts[i - 1];
    lua_pushnil(state);
    while (lua_next(state, table) != 0) {
      const int32 value = lua_gettop(state);
      const StringView key =
          lua_type(state, -2) == LUA_TSTRING ? lua_tostring(state, -2) : StringView();
      if (key == "time") {
        burst.time = checkFloat(state, value);
      }
      else if (key == "count") {
        burst.count = checkCount(state, value, "count");
      }
      else if (key == "cycles") {
        burst.cycles = checkCount(state, value, "cycles");
      }
      else if (key == "interval") {
        burst.interval = checkFloat(state, value);
      }
      else {
        return luaL_error(state, "Particles.setBursts: unknown burst option '%s'",
                          luaL_tolstring(state, -2, nullptr));
      }
      lua_pop(state, 1);
    }
    lua_pop(state, 1);
  }
  system.setBursts(Span<const ParticleBurst>(bursts, static_cast<SIZE_T>(count)));
  return 0;
}

/*
 * emit(count, x?, y?, color?): on the shape, or at (x, y) in the YZ plane with its colors
 * times color. Gives how many were made.
 */
int32
emit(lua_State* state)
{
  ParticleSystemComponent& system = checkSystem(state, 1);
  const uint32 count = checkCount(state, 2, "count");
  uint32 made = 0;
  if (lua_isnoneornil(state, 3)) {
    made = system.emit(count);
  }
  else {
    const Vector2 position(checkFloat(state, 3), checkFloat(state, 4));
    const LinearColor tint = lua_isnoneornil(state, 5)
                                 ? LinearColor::WHITE
                                 : ScriptTraits<LinearColor>::check(state, 5);
    made = system.emitAt(count, position, tint);
  }
  lua_pushinteger(state, static_cast<lua_Integer>(made));
  return 1;
}

int32
play(lua_State* state)
{
  checkSystem(state, 1).play();
  return 0;
}

/*
 * stop(clear?): the particles alive keep flying unless clear.
 */
int32
stop(lua_State* state)
{
  checkSystem(state, 1).stop(lua_toboolean(state, 2) != 0);
  return 0;
}

int32
isPlaying(lua_State* state)
{
  lua_pushboolean(state, checkSystem(state, 1).isPlaying() ? 1 : 0);
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
    lua_pushfstring(state, "Particles(%s)", owner->getName().c_str());
  }
  else {
    lua_pushliteral(state, "Particles(destroyed)");
  }
  return 1;
}

/*
 * The shapes of the tables configure and setBursts take, for code editors.
 */
void
describeShapes(lua_State* state)
{
  ScriptApiDescription* description = ScriptApiDescription::find(state);
  if (!description) {
    return;
  }
  ScriptApiType& options = description->addType("ParticleOptions", ScriptApiTypeKind::Shape);
  options.description = "What Particles:configure sets. Angles are degrees counterclockwise "
                        "from +Y (90 is up); world units and seconds. Each particle picks its "
                        "scale, rotation, spin and color between the ranges.";
  options.fields = {
      {"capacity?", "integer"},
      {"rate?", "number"},
      {"duration?", "number"},
      {"loop?", "boolean"},
      {"playOnStart?", "boolean"},
      {"localSpace?", "boolean"},
      {"shape?", "\"Point\"|\"Circle\"|\"CircleEdge\"|\"Rectangle\"|\"Line\""},
      {"shapeSize?", "number|Vector2"},
      {"texture?", "Texture|string"},
      {"size?", "number|Vector2"},
      {"minScale?", "number"},
      {"maxScale?", "number"},
      {"color?", "Color"},
      {"color2?", "Color"},
      {"faceVelocity?", "boolean"},
      {"rotation?", "number"},
      {"rotationVariance?", "number"},
      {"spin?", "number"},
      {"spinVariance?", "number"},
      {"depth?", "number"},
      {"layer?", "integer"},
      {"order?", "integer"},
      {"blend?", "\"Alpha\"|\"Additive\""},
      {"filter?", "\"Point\"|\"Linear\""}};
  AgeMotionBindings::addApiFields(options.fields);

  ScriptApiType& burst = description->addType("ParticleBurst", ScriptApiTypeKind::Shape);
  burst.description = "count particles at time seconds into each cycle, cycles times, "
                      "interval seconds apart.";
  burst.fields = {{"time?", "number"},
                  {"count?", "integer"},
                  {"cycles?", "integer"},
                  {"interval?", "number"}};
}
} // namespace

/*
 */
void
ParticleBindings::registerBindings(lua_State* state)
{
  ScriptClass<GameObject>(state)
      .rawMethod("getParticles", &getParticles, "fun(): Particles?")
      .rawMethod("addParticles", &addParticles, "fun(): Particles");

  ScriptClass<ParticleSystemComponent>(state)
      .rawMethod("configure", &configure, "fun(options: ParticleOptions)")
      .rawMethod("setLook", &setLook, "fun(style: SpriteStyle)")
      .rawMethod("setFlipbook", &setFlipbook,
                 "fun(atlas?: Atlas|string, firstFrame?: integer, frameCount?: integer, "
                 "framesPerSecond?: number)")
      .rawMethod("setColorGradient", &setColorGradient, "fun(keys: [number, Color][])")
      .rawMethod("setSizeCurve", &setSizeCurve, "fun(keys: [number, number][])")
      .rawMethod("setBursts", &setBursts, "fun(bursts: ParticleBurst[])")
      .rawMethod("emit", &emit,
                 "fun(count: integer, x?: number, y?: number, color?: Color): integer")
      .rawMethod("play", &play, "fun()")
      .rawMethod("stop", &stop, "fun(clear?: boolean)")
      .rawMethod("isPlaying", &isPlaying, "fun(): boolean")
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
