/************************************************************************/
/**
 * @file chProjectileBindings.h
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  The projectile types scripts see.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

struct lua_State;

namespace chEngineSDK {

/**
 * Exists so the plugin adds its script API to every script state the ScriptManager makes:
 * ProjectileSystem and ProjectileTarget handles, and GameObject methods that give and add
 * them.
 */
class ProjectileBindings
{
 public:
  static void
  registerBindings(lua_State* state);

  static constexpr const ANSICHAR* SYSTEM_TYPE_NAME = "ProjectileSystem";
  static constexpr const ANSICHAR* TARGET_TYPE_NAME = "ProjectileTarget";
};

} // namespace chEngineSDK
