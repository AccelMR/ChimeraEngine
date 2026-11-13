/************************************************************************/
/**
 * @file chFloatingNumbersEditor.h
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  What the Inspector shows of the floating numbers plugin's component.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#if USING(CH_EDITOR)

namespace chEngineSDK {

/**
 * Exists so the plugin's component gets an Inspector section from data, without the plugin
 * knowing ImGui.
 */
class FloatingNumbersEditor
{
 public:
  static void
  registerDescriptions();
};

} // namespace chEngineSDK

#endif // USING(CH_EDITOR)
