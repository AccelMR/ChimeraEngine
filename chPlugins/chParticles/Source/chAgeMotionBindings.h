/************************************************************************/
/**
 * @file chAgeMotionBindings.h
 * @author AccelMR
 * @date 2026/11/16
 * @brief
 *  The AgeMotion keys every configure{} of the plugin takes.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chAgeMotion.h"

struct lua_State;

namespace chEngineSDK {
struct ScriptApiField;

/**
 * Exists so floating numbers and particles read their motion from scripts with the same
 * keys: angle, spread, speed, speedVariance, gravity, drag, lifetime, lifetimeVariance,
 * fadeIn and fadeOut.
 */
class AgeMotionBindings
{
 public:
  /**
   * Sets the member named key from the value at valueIndex (raises when it has the wrong
   * type). False when key is not a motion key.
   */
  static bool
  readOption(lua_State* state, StringView key, int32 valueIndex, AgeMotion& motion);

  /**
   * The motion keys, optional, for a LuaLS shape.
   */
  static void
  addApiFields(Vector<ScriptApiField>& fields);
};

} // namespace chEngineSDK
