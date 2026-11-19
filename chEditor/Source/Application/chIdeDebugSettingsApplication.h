/************************************************************************/
/**
 * @file chIdeDebugSettingsApplication.h
 * @author AccelMR
 * @date 2026/11/19
 * @brief
 *  The editor's run that only writes the IDE debug settings.
 */
/************************************************************************/
#pragma once

#include "chBaseApplication.h"

namespace chEngineSDK {

/**
 * Exists so the IDE debug settings can be written without opening the editor (scripts, a
 * fresh clone): chEditor -project=<project> -GenerateIdeDebugSettings writes them and exits,
 * with no window or graphics API. A file that could not be written makes the exit code 1.
 * Options: -NoVSCode, -NoFollowCMakeTools, -NoVSSolution, -NoVSFolder (Windows),
 * -Debugger=gdb|lldb (Linux).
 */
class IdeDebugSettingsApplication : public BaseApplication
{
 public:
  static constexpr const ANSICHAR* COMMAND_LINE_FLAG = "GenerateIdeDebugSettings";

  void
  run() override;
};

} // namespace chEngineSDK
