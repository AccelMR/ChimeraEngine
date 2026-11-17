/************************************************************************/
/**
 * @file chFloatingNumberBindings.h
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  The floating number types scripts see.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

struct lua_State;

namespace chEngineSDK {

/**
 * Exists so the plugin adds its script API to every script state: the FloatingNumbers
 * handle and GameObject methods that give and add it.
 */
class FloatingNumberBindings
{
 public:
  static void
  registerBindings(lua_State* state);

  static constexpr const ANSICHAR* SYSTEM_TYPE_NAME = "FloatingNumbers";
};

} // namespace chEngineSDK
