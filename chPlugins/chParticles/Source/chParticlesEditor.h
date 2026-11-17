/************************************************************************/
/**
 * @file chParticlesEditor.h
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  What the Inspector shows of the particles plugin's components.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#if USING(CH_EDITOR)

namespace chEngineSDK {

/**
 * Exists so the plugin's components get an Inspector section from data, without the plugin
 * knowing ImGui.
 */
class ParticlesEditor
{
 public:
  static void
  registerDescriptions();
};

} // namespace chEngineSDK

#endif // USING(CH_EDITOR)
