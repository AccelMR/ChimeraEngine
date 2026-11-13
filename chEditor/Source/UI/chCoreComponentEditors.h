/************************************************************************/
/**
 * @file chCoreComponentEditors.h
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Inspector descriptions of the engine components that have no section of their own.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

namespace chEngineSDK {

/**
 * Exists so engine components get their Inspector section and their place in Add Component
 * from a description, the way plugin components do, instead of hand-written ImGui.
 */
class CoreComponentEditors
{
 public:
  /**
   * Called once at start up, after the plugins.
   */
  static void
  registerDescriptions();
};

} // namespace chEngineSDK
