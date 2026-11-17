/************************************************************************/
/**
 * @file chParticleBindings.h
 * @author AccelMR
 * @date 2026/11/16
 * @brief
 *  The particle types scripts see.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

struct lua_State;

namespace chEngineSDK {

/**
 * Exists so the plugin adds its particles to every script state: the Particles handle and
 * GameObject methods that give and add it.
 */
class ParticleBindings
{
 public:
  static void
  registerBindings(lua_State* state);

  static constexpr const ANSICHAR* SYSTEM_TYPE_NAME = "Particles";
};

} // namespace chEngineSDK
