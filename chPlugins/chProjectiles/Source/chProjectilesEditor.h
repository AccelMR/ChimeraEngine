/************************************************************************/
/**
 * @file chProjectilesEditor.h
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  What the editor's Inspector shows of the projectile components.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#if USING(CH_EDITOR)

namespace chEngineSDK {

/**
 * Exists so the projectile components can be added and edited in the Object Inspector,
 * which draws them from the descriptions this registers.
 */
class ProjectilesEditor
{
 public:
  static void
  registerDescriptions();
};

} // namespace chEngineSDK

#endif // USING(CH_EDITOR)
