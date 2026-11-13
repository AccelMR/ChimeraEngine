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
 * Exists so the editor and the game can be debugged on a project straight from an IDE: picking
 * a project in the project browser starts a new process, which the debugger does not follow.
 * It writes launch settings that pass -project to chEditor and chGame: entries in the engine's
 * .vscode/launch.json and .vs/launch.vs.json (each project between marker comments, the rest of
 * the file kept), and the .vcxproj.user of every Visual Studio solution under build/. All of
 * these files are ignored by git.
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
