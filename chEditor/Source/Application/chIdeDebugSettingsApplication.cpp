/************************************************************************/
/**
 * @file chIdeDebugSettingsApplication.cpp
 * @author AccelMR
 * @date 2026/11/19
 * @brief
 *  The editor's run that only writes the IDE debug settings.
 */
/************************************************************************/
#include "chIdeDebugSettingsApplication.h"

#include "chCommandLine.h"
#include "chEnginePaths.h"
#include "chException.h"
#include "chIdeDebugSettings.h"
#include "chStringUtils.h"

namespace chEngineSDK {

/*
 */
void
IdeDebugSettingsApplication::run()
{
  if (!EnginePaths::hasProject()) {
    CH_EXCEPT(InvalidArgumentException,
              "-GenerateIdeDebugSettings needs -project=<folder or .chproject file>.");
  }

  IdeDebugSettingsOptions options;
  options.bVSCode = !CommandLine::hasFlag("NoVSCode");
  options.bFollowCMakeTools = !CommandLine::hasFlag("NoFollowCMakeTools");
#if USING(CH_PLATFORM_WIN32)
  options.bVisualStudioSolution = !CommandLine::hasFlag("NoVSSolution");
  options.bVisualStudioFolder = !CommandLine::hasFlag("NoVSFolder");
#else
  const String debugger = CommandLine::getValue("Debugger", "gdb");
  if (StringUtils::compareIgnoreCase(debugger, "gdb") == 0) {
    options.debugger = "gdb";
  }
  else if (StringUtils::compareIgnoreCase(debugger, "lldb") == 0) {
    options.debugger = "lldb";
  }
  else {
    CH_EXCEPT(InvalidArgumentException, "-Debugger must be gdb or lldb, not " + debugger + ".");
  }
#endif // USING(CH_PLATFORM_WIN32)

  Vector<IdeDebugSettingsResult> results;
  IdeDebugSettings::generate(options, results);

  uint32 errorCount = 0;
  for (const IdeDebugSettingsResult& result : results) {
    errorCount += result.kind == IdeDebugSettingsResultKind::Error ? 1 : 0;
  }
  if (errorCount > 0) {
    CH_EXCEPT(RunTimeException, StringUtils::format("{0} IDE settings file(s) failed.",
                                                    errorCount));
  }
}

} // namespace chEngineSDK
