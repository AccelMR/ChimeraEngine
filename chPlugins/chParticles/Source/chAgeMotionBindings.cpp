/************************************************************************/
/**
 * @file chAgeMotionBindings.cpp
 * @author AccelMR
 * @date 2026/11/16
 * @brief
 *  The AgeMotion keys every configure{} of the plugin takes.
 */
/************************************************************************/
#include "chAgeMotionBindings.h"

#include "chScriptApi.h"
#include "chScriptBinder.h"
#include "chScriptMathTypes.h"

#include "lauxlib.h"
#include "lua.h"

namespace chEngineSDK {

namespace {
struct FloatKey
{
  const ANSICHAR* name;
  float AgeMotion::*member;
};

constexpr FloatKey kFloatKeys[] = {{"angle", &AgeMotion::angle},
                                   {"spread", &AgeMotion::spread},
                                   {"speed", &AgeMotion::speed},
                                   {"speedVariance", &AgeMotion::speedVariance},
                                   {"drag", &AgeMotion::drag},
                                   {"lifetime", &AgeMotion::lifetime},
                                   {"lifetimeVariance", &AgeMotion::lifetimeVariance},
                                   {"fadeIn", &AgeMotion::fadeIn},
                                   {"fadeOut", &AgeMotion::fadeOut}};

constexpr const ANSICHAR* kGravityKey = "gravity";
} // namespace

/*
 */
bool
AgeMotionBindings::readOption(lua_State* state,
                              StringView key,
                              int32 valueIndex,
                              AgeMotion& motion)
{
  for (const FloatKey& floatKey : kFloatKeys) {
    if (key == floatKey.name) {
      motion.*floatKey.member = static_cast<float>(luaL_checknumber(state, valueIndex));
      return true;
    }
  }
  if (key == kGravityKey) {
    motion.gravity = ScriptTraits<Vector2>::check(state, valueIndex);
    return true;
  }
  return false;
}

/*
 */
void
AgeMotionBindings::addApiFields(Vector<ScriptApiField>& fields)
{
  for (const FloatKey& floatKey : kFloatKeys) {
    fields.push_back({String(floatKey.name) + "?", "number"});
  }
  fields.push_back({String(kGravityKey) + "?", "Vector2"});
}

} // namespace chEngineSDK
