/************************************************************************/
/**
 * @file chProjectileBindings.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  The projectile types scripts see.
 */
/************************************************************************/
#include "chProjectileBindings.h"

#include "chGameObject.h"
#include "chMath.h"
#include "chProjectileSystemComponent.h"
#include "chProjectileTargetComponent.h"
#include "chScriptApi.h"
#include "chScriptBinder.h"
#include "chScriptManager.h"
#include "chScriptMathTypes.h"
#include "chScriptObjectTypes.h"

#include "lauxlib.h"
#include "lua.h"

namespace chEngineSDK {

CH_SCRIPT_HANDLE_TYPE(ProjectileSystemComponent, "ProjectileSystem");
CH_SCRIPT_HANDLE_TYPE(ProjectileTargetComponent, "ProjectileTarget");

namespace {
constexpr const ANSICHAR* kSystemName = ProjectileBindings::SYSTEM_TYPE_NAME;
constexpr const ANSICHAR* kTargetName = ProjectileBindings::TARGET_TYPE_NAME;

/*
 * The component is found again at every call, so one removed in between is an error.
 */
template <typename T>
T&
checkComponent(lua_State* state, const int32 index, const ANSICHAR* typeName)
{
  GameObject& owner = ScriptPartHandle::checkOwner(state, index, typeName);
  T* const component = owner.getComponent<T>();
  if (!component) {
    luaL_error(state, "GameObject '%s' has no %s", owner.getName().c_str(), typeName);
  }
  return *component;
}

ProjectileSystemComponent&
checkSystem(lua_State* state, const int32 index)
{
  return checkComponent<ProjectileSystemComponent>(state, index, kSystemName);
}

ProjectileTargetComponent&
checkTarget(lua_State* state, const int32 index)
{
  return checkComponent<ProjectileTargetComponent>(state, index, kTargetName);
}

NODISCARD float
checkFloat(lua_State* state, const int32 index)
{
  return static_cast<float>(luaL_checknumber(state, index));
}

NODISCARD float
optFloat(lua_State* state, const int32 index, const float fallback)
{
  return static_cast<float>(luaL_optnumber(state, index, static_cast<lua_Number>(fallback)));
}

NODISCARD uint32
checkCount(lua_State* state, const int32 index)
{
  const lua_Integer count = luaL_checkinteger(state, index);
  if (count < 0 || count > static_cast<lua_Integer>(ProjectileSystemComponent::MAX_CAPACITY)) {
    luaL_error(state, "count must be between 0 and %d",
               static_cast<int32>(ProjectileSystemComponent::MAX_CAPACITY));
  }
  return static_cast<uint32>(count);
}

NODISCARD uint32
checkTeam(lua_State* state, const int32 index)
{
  const lua_Integer team = luaL_checkinteger(state, index);
  if (team < 1 || team > static_cast<lua_Integer>(ProjectileTargetComponent::MAX_TEAMS)) {
    luaL_error(state, "a team is a number from 1 to %d",
               static_cast<int32>(ProjectileTargetComponent::MAX_TEAMS));
  }
  return static_cast<uint32>(team);
}

NODISCARD uint16
checkPierce(lua_State* state, const int32 index)
{
  const lua_Integer pierce = luaL_checkinteger(state, index);
  if (pierce < 0 || pierce > 0xFFFF) {
    luaL_error(state, "pierce must be between 0 and 65535");
  }
  return static_cast<uint16>(pierce);
}

/*
 * Calls apply(key, valueIndex) for each entry of the options table at index; a key that is
 * not a string raises.
 */
template <typename Apply>
void
forEachOption(lua_State* state, const int32 index, const ANSICHAR* function, Apply apply)
{
  luaL_checktype(state, index, LUA_TTABLE);
  lua_pushnil(state);
  while (lua_next(state, index) != 0) {
    if (lua_type(state, -2) != LUA_TSTRING) {
      luaL_error(state, "%s: option names are strings", function);
    }
    apply(StringView(lua_tostring(state, -2)), lua_gettop(state));
    lua_pop(state, 1);
  }
}

void
raiseUnknownOption(lua_State* state, const ANSICHAR* function, const StringView key)
{
  luaL_error(state, "%s: there is no option '%s'", function, String(key).c_str());
}

/*
 * Options of configure and spawn that both understand; false for another key.
 */
bool
readProjectileOption(lua_State* state, const StringView key, const int32 value,
                     ProjectileDefaults& defaults)
{
  if (key == "speed") {
    defaults.speed = checkFloat(state, value);
  }
  else if (key == "lifetime") {
    defaults.lifetime = checkFloat(state, value);
  }
  else if (key == "radius") {
    defaults.radius = checkFloat(state, value);
  }
  else if (key == "damage") {
    defaults.damage = checkFloat(state, value);
  }
  else if (key == "turnRate") {
    defaults.turnRate = checkFloat(state, value);
  }
  else if (key == "scale") {
    defaults.scale = checkFloat(state, value);
  }
  else if (key == "acceleration") {
    defaults.acceleration = ScriptTraits<Vector2>::check(state, value);
  }
  else if (key == "pierce") {
    defaults.pierce = checkPierce(state, value);
  }
  else {
    return false;
  }
  return true;
}

// ---------- GameObject ----------

int32
getProjectiles(lua_State* state)
{
  GameObject& gameObject = ScriptTraits<GameObject>::check(state, 1);
  if (!gameObject.getComponent<ProjectileSystemComponent>()) {
    lua_pushnil(state);
    return 1;
  }
  ScriptPartHandle::push(state, gameObject, kSystemName);
  return 1;
}

int32
addProjectiles(lua_State* state)
{
  GameObject& gameObject = ScriptTraits<GameObject>::check(state, 1);
  if (!gameObject.getComponent<ProjectileSystemComponent>()) {
    gameObject.addComponent<ProjectileSystemComponent>();
  }
  ScriptPartHandle::push(state, gameObject, kSystemName);
  return 1;
}

int32
getProjectileTarget(lua_State* state)
{
  GameObject& gameObject = ScriptTraits<GameObject>::check(state, 1);
  if (!gameObject.getComponent<ProjectileTargetComponent>()) {
    lua_pushnil(state);
    return 1;
  }
  ScriptPartHandle::push(state, gameObject, kTargetName);
  return 1;
}

int32
addProjectileTarget(lua_State* state)
{
  GameObject& gameObject = ScriptTraits<GameObject>::check(state, 1);
  ProjectileTargetComponent* target = gameObject.getComponent<ProjectileTargetComponent>();
  if (!target) {
    target = &gameObject.addComponent<ProjectileTargetComponent>();
  }
  if (!lua_isnoneornil(state, 2)) {
    target->setRadius(checkFloat(state, 2));
  }
  if (!lua_isnoneornil(state, 3)) {
    target->setTeam(checkTeam(state, 3));
  }
  ScriptPartHandle::push(state, gameObject, kTargetName);
  return 1;
}

// ---------- ProjectileSystem ----------

/*
 * setLook(style, faceVelocity?): every projectile drawn as style, its size times the
 * projectile's scale; faceVelocity (true) turns the image's right side along the velocity.
 */
int32
setLook(lua_State* state)
{
  ProjectileSystemComponent& system = checkSystem(state, 1);
  const ScriptSpriteStyle& style = ScriptTraits<ScriptSpriteStyle>::check(state, 2);
  ProjectileLook look;
  look.texture = style.texture;
  for (uint32 i = 0; i < 4; ++i) {
    look.uvRect[i] = style.uvRect[i];
  }
  look.size = Vector2(style.size[0], style.size[1]);
  look.pivot = Vector2(style.pivot[0], style.pivot[1]);
  look.color = LinearColor(style.color[0], style.color[1], style.color[2], style.color[3]);
  look.depth = style.depth;
  look.sortingLayer = style.sortingLayer;
  look.order = style.order;
  look.blendMode = static_cast<SpriteBlendMode>(style.blendMode);
  look.filter = static_cast<SpriteFilter>(style.filter);
  look.material = style.material;
  look.bFaceVelocity = lua_isnoneornil(state, 3) || lua_toboolean(state, 3) != 0;
  system.setLook(look);
  return 0;
}

/*
 * configure{...}: speed, lifetime, radius, damage, turnRate, scale, acceleration, pierce
 * (what fire, fireRing and fireSpread give), capacity and cellSize.
 */
int32
configure(lua_State* state)
{
  ProjectileSystemComponent& system = checkSystem(state, 1);
  ProjectileDefaults defaults = system.getDefaults();
  forEachOption(state, 2, "ProjectileSystem:configure", [&](StringView key, int32 value) {
    if (readProjectileOption(state, key, value, defaults)) {
      return;
    }
    if (key == "capacity") {
      system.setCapacity(checkCount(state, value));
    }
    else if (key == "cellSize") {
      system.setCellSize(checkFloat(state, value));
    }
    else {
      raiseUnknownOption(state, "ProjectileSystem:configure", key);
    }
  });
  system.setDefaults(defaults);
  return 0;
}

int32
fire(lua_State* state)
{
  ProjectileSystemComponent& system = checkSystem(state, 1);
  const Vector2 position(checkFloat(state, 2), checkFloat(state, 3));
  const float angle = checkFloat(state, 4);
  const float speed = optFloat(state, 5, system.getDefaults().speed);
  lua_pushboolean(state, system.fire(position, angle, speed) ? 1 : 0);
  return 1;
}

int32
fireRing(lua_State* state)
{
  ProjectileSystemComponent& system = checkSystem(state, 1);
  const Vector2 position(checkFloat(state, 2), checkFloat(state, 3));
  const uint32 count = checkCount(state, 4);
  const float startAngle = optFloat(state, 5, 0.0f);
  const float speed = optFloat(state, 6, system.getDefaults().speed);
  lua_pushinteger(state, system.fireRing(position, count, startAngle, speed));
  return 1;
}

int32
fireSpread(lua_State* state)
{
  ProjectileSystemComponent& system = checkSystem(state, 1);
  const Vector2 position(checkFloat(state, 2), checkFloat(state, 3));
  const uint32 count = checkCount(state, 4);
  const float angle = checkFloat(state, 5);
  const float arc = checkFloat(state, 6);
  const float speed = optFloat(state, 7, system.getDefaults().speed);
  lua_pushinteger(state, system.fireSpread(position, count, angle, arc, speed));
  return 1;
}

/*
 * spawn{...}: x and y (required), then angle and speed or velocity, plus any option of
 * configure for this projectile only. Reads a table, so fire is cheaper for plain shots.
 */
int32
spawn(lua_State* state)
{
  ProjectileSystemComponent& system = checkSystem(state, 1);
  ProjectileDefaults options = system.getDefaults();
  Vector2 position = Vector2::ZERO;
  bool bHasX = false;
  bool bHasY = false;
  float angle = 0.0f;
  bool bHasVelocity = false;
  Vector2 velocity = Vector2::ZERO;
  forEachOption(state, 2, "ProjectileSystem:spawn", [&](StringView key, int32 value) {
    if (readProjectileOption(state, key, value, options)) {
      return;
    }
    if (key == "x") {
      position.x = checkFloat(state, value);
      bHasX = true;
    }
    else if (key == "y") {
      position.y = checkFloat(state, value);
      bHasY = true;
    }
    else if (key == "angle") {
      angle = checkFloat(state, value);
    }
    else if (key == "velocity") {
      velocity = ScriptTraits<Vector2>::check(state, value);
      bHasVelocity = true;
    }
    else {
      raiseUnknownOption(state, "ProjectileSystem:spawn", key);
    }
  });
  if (!bHasX || !bHasY) {
    return luaL_error(state, "ProjectileSystem:spawn needs x and y");
  }
  if (!bHasVelocity) {
    float sine = 0.0f;
    float cosine = 1.0f;
    Math::sinCos(Degree(angle).valueRadian(), sine, cosine);
    velocity = Vector2(cosine, sine) * options.speed;
  }

  const bool bSpawned = system.spawn({.position = position,
                                      .velocity = velocity,
                                      .acceleration = options.acceleration,
                                      .lifetime = options.lifetime,
                                      .radius = options.radius,
                                      .damage = options.damage,
                                      .turnRate = options.turnRate,
                                      .scale = options.scale,
                                      .pierce = options.pierce});
  lua_pushboolean(state, bSpawned ? 1 : 0);
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
  lua_pushinteger(state, checkSystem(state, 1).getCount());
  return 1;
}

int32
getCapacity(lua_State* state)
{
  lua_pushinteger(state, checkSystem(state, 1).getCapacity());
  return 1;
}

int32
setHomingTarget(lua_State* state)
{
  ProjectileSystemComponent& system = checkSystem(state, 1);
  system.setHomingTarget(
      lua_isnoneornil(state, 2) ? nullptr : &ScriptTraits<GameObject>::check(state, 2));
  return 0;
}

int32
getHomingTarget(lua_State* state)
{
  if (GameObject* target = checkSystem(state, 1).getHomingTarget()) {
    ScriptTraits<GameObject>::push(state, *target);
  }
  else {
    lua_pushnil(state);
  }
  return 1;
}

/*
 * setHitTeams({2, 3}) hits only those teams, setHitTeams({}) none, setHitTeams() every one.
 */
int32
setHitTeams(lua_State* state)
{
  ProjectileSystemComponent& system = checkSystem(state, 1);
  if (lua_isnoneornil(state, 2)) {
    system.setHitTeams(ProjectileSystemComponent::ALL_TEAMS);
    return 0;
  }
  luaL_checktype(state, 2, LUA_TTABLE);
  uint32 mask = 0;
  const lua_Integer count = static_cast<lua_Integer>(lua_rawlen(state, 2));
  for (lua_Integer i = 1; i <= count; ++i) {
    lua_rawgeti(state, 2, i);
    mask |= 1u << (checkTeam(state, -1) - 1);
    lua_pop(state, 1);
  }
  system.setHitTeams(mask);
  return 0;
}

int32
getHitTeams(lua_State* state)
{
  const uint32 mask = checkSystem(state, 1).getHitTeams();
  lua_createtable(state, 0, 0);
  lua_Integer next = 1;
  for (uint32 team = 1; team <= ProjectileTargetComponent::MAX_TEAMS; ++team) {
    if ((mask & (1u << (team - 1))) != 0) {
      lua_pushinteger(state, static_cast<lua_Integer>(team));
      lua_rawseti(state, -2, next++);
    }
  }
  return 1;
}

/*
 * onHit(function(target, damage, x, y) end) or onHit(nil). Called after each fixed step for
 * every projectile that reached a target, before the destroys of that step apply.
 */
int32
onHit(lua_State* state)
{
  ProjectileSystemComponent& system = checkSystem(state, 1);
  if (lua_isnoneornil(state, 2)) {
    system.setHitCallback(ScriptClassInfo::NO_REFERENCE);
    return 0;
  }
  luaL_checktype(state, 2, LUA_TFUNCTION);
  lua_pushvalue(state, 2);
  system.setHitCallback(luaL_ref(state, LUA_REGISTRYINDEX));
  return 0;
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
    lua_pushfstring(state, "ProjectileSystem(%s)", owner->getName().c_str());
  }
  else {
    lua_pushliteral(state, "ProjectileSystem(destroyed)");
  }
  return 1;
}

// ---------- ProjectileTarget ----------

int32
setTargetRadius(lua_State* state)
{
  checkTarget(state, 1).setRadius(checkFloat(state, 2));
  return 0;
}

int32
getTargetRadius(lua_State* state)
{
  lua_pushnumber(state, static_cast<lua_Number>(checkTarget(state, 1).getRadius()));
  return 1;
}

int32
setTargetTeam(lua_State* state)
{
  checkTarget(state, 1).setTeam(checkTeam(state, 2));
  return 0;
}

int32
getTargetTeam(lua_State* state)
{
  lua_pushinteger(state, static_cast<lua_Integer>(checkTarget(state, 1).getTeam()));
  return 1;
}

int32
getTargetObject(lua_State* state)
{
  ScriptTraits<GameObject>::push(state, ScriptPartHandle::checkOwner(state, 1, kTargetName));
  return 1;
}

int32
targetToString(lua_State* state)
{
  if (const GameObject* owner = ScriptPartHandle::testOwner(state, 1, kTargetName)) {
    lua_pushfstring(state, "ProjectileTarget(%s)", owner->getName().c_str());
  }
  else {
    lua_pushliteral(state, "ProjectileTarget(destroyed)");
  }
  return 1;
}

void
setToString(lua_State* state, const ANSICHAR* typeName, const lua_CFunction function)
{
  luaL_getmetatable(state, typeName);
  lua_pushcfunction(state, function);
  lua_setfield(state, -2, "__tostring");
  lua_pop(state, 1);
}

/*
 * The shape of the tables configure and spawn take, for code editors.
 */
void
describeOptions(lua_State* state)
{
  ScriptApiDescription* description = ScriptApiDescription::find(state);
  if (!description) {
    return;
  }
  ScriptApiType& options = description->addType("ProjectileOptions", ScriptApiTypeKind::Shape);
  options.description = "What ProjectileSystem:configure sets for every shot, and "
                        "ProjectileSystem:spawn for one. Angles are degrees counterclockwise "
                        "from +Y; x and y are world Y and Z.";
  options.fields = {
      {"speed?", "number"},         {"lifetime?", "number"}, {"radius?", "number"},
      {"damage?", "number"},        {"turnRate?", "number"}, {"scale?", "number"},
      {"acceleration?", "Vector2"}, {"pierce?", "integer"},  {"capacity?", "integer"},
      {"cellSize?", "number"},      {"x?", "number"},        {"y?", "number"},
      {"angle?", "number"},         {"velocity?", "Vector2"}};
}
} // namespace

/*
 */
void
ProjectileBindings::registerBindings(lua_State* state)
{
  ScriptClass<GameObject>(state)
      .rawMethod("getProjectiles", &getProjectiles, "fun(): ProjectileSystem?")
      .rawMethod("addProjectiles", &addProjectiles, "fun(): ProjectileSystem")
      .rawMethod("getProjectileTarget", &getProjectileTarget, "fun(): ProjectileTarget?")
      .rawMethod("addProjectileTarget", &addProjectileTarget,
                 "fun(radius?: number, team?: integer): ProjectileTarget");

  ScriptClass<ProjectileSystemComponent>(state)
      .rawMethod("setLook", &setLook, "fun(style: SpriteStyle, faceVelocity?: boolean)")
      .rawMethod("configure", &configure, "fun(options: ProjectileOptions)")
      .rawMethod("fire", &fire,
                 "fun(x: number, y: number, angle: number, speed?: number): boolean")
      .rawMethod("fireRing", &fireRing,
                 "fun(x: number, y: number, count: integer, startAngle?: number, "
                 "speed?: number): integer")
      .rawMethod("fireSpread", &fireSpread,
                 "fun(x: number, y: number, count: integer, angle: number, arc: number, "
                 "speed?: number): integer")
      .rawMethod("spawn", &spawn, "fun(options: ProjectileOptions): boolean")
      .rawMethod("clear", &clear, "fun()")
      .rawMethod("getCount", &getCount, "fun(): integer")
      .rawMethod("getCapacity", &getCapacity, "fun(): integer")
      .rawMethod("setHomingTarget", &setHomingTarget, "fun(target?: GameObject)")
      .rawMethod("getHomingTarget", &getHomingTarget, "fun(): GameObject?")
      .rawMethod("setHitTeams", &setHitTeams, "fun(teams?: integer[])")
      .rawMethod("getHitTeams", &getHitTeams, "fun(): integer[]")
      .rawMethod("onHit", &onHit,
                 "fun(callback?: fun(target: GameObject, damage: number, x: number, "
                 "y: number))")
      .rawMethod("getGameObject", &getSystemObject, "fun(): GameObject");

  ScriptClass<ProjectileTargetComponent>(state)
      .rawMethod("setRadius", &setTargetRadius, "fun(radius: number)")
      .rawMethod("getRadius", &getTargetRadius, "fun(): number")
      .rawMethod("setTeam", &setTargetTeam, "fun(team: integer)")
      .rawMethod("getTeam", &getTargetTeam, "fun(): integer")
      .rawMethod("getGameObject", &getTargetObject, "fun(): GameObject");

  setToString(state, kSystemName, &systemToString);
  setToString(state, kTargetName, &targetToString);
  describeOptions(state);
}

} // namespace chEngineSDK
