/************************************************************************/
/**
 * @file chIdeDebugSettings.h
 * @author AccelMR
 * @date 2026/11/19
 * @brief
 *  Writes debugger launch settings for the open project into the IDEs.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

namespace chEngineSDK {

enum class IdeDebugSettingsResultKind : uint8
{
  Written = 0,
  // Something is missing that the user may not need (no solution, no build folder).
  Warning = 1,
  // A file could not be written or read as expected.
  Error = 2
};

struct IdeDebugSettingsResult
{
  IdeDebugSettingsResultKind kind = IdeDebugSettingsResultKind::Written;
  String text;
};

struct IdeDebugSettingsOptions
{
  bool bVSCode = true;
  bool bFollowCMakeTools = true;
#if USING(CH_PLATFORM_WIN32)
  bool bVisualStudioSolution = true;
  bool bVisualStudioFolder = true;
#else
  // MIMode of the VS Code entries: "gdb" or "lldb".
  String debugger = "gdb";
#endif // USING(CH_PLATFORM_WIN32)
};

/**
 * Exists so the editor and the game can be debugged on a project straight from an IDE: picking
 * a project in the project browser starts a new process, which the debugger does not follow.
 * It writes launch settings that pass -project to chEditor and chGame: entries in the engine's
 * .vscode/launch.json and .vs/launch.vs.json (each project between marker comments, the rest of
 * the file kept), and the .vcxproj.user of every Visual Studio solution under build/. All of
 * these files are ignored by git. Used by the editor's dialog and by
 * chEditor -GenerateIdeDebugSettings.
 */
class IdeDebugSettings
{
 public:
  /**
   * Writes the settings for EnginePaths' project, logs one line per result and appends them
   * to outResults.
   */
  static void
  generate(const IdeDebugSettingsOptions& options, Vector<IdeDebugSettingsResult>& outResults);
};

} // namespace chEngineSDK
