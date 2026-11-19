/************************************************************************/
/**
 * @file chIdeDebugSettingsUI.h
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Writes debugger launch settings for the open project into the IDEs.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

namespace chEngineSDK {

/**
 * Exists so the IDE debug settings can be written from the editor: File > Generate IDE Debug
 * Settings... shows the options of IdeDebugSettings and what each file got.
 */
class IdeDebugSettingsUI
{
 public:
  static void
  open();

  /**
   * Call once per frame, after the panels.
   */
  static void
  render();
};

} // namespace chEngineSDK
