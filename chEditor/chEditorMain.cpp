#include "chCommandLine.h"
#include "chEditorApplication.h"
#include "chEnginePaths.h"
#include "chIdeDebugSettingsApplication.h"
#include "chProjectBrowserApplication.h"
#include "chStringUtils.h"

using namespace chEngineSDK;

namespace {
/*
 * Read before the command line is parsed, so the settings run keeps its own log and never
 * turns the editor's log of a running editor into a backup.
 */
bool
isIdeDebugSettingsRun(int32 argc, ANSICHAR* argv[])
{
  for (int32 i = 1; i < argc; ++i) {
    StringView argument(argv[i]);
    while (!argument.empty() && argument.front() == '-') {
      argument.remove_prefix(1);
    }
    if (StringUtils::compareIgnoreCase(argument,
                                       IdeDebugSettingsApplication::COMMAND_LINE_FLAG) == 0) {
      return true;
    }
  }
  return false;
}
} // namespace

int32
main(int32 argc, ANSICHAR* argv[])
{
  const bool bIdeDebugSettings = isIdeDebugSettingsRun(argc, argv);
  const String logFileName = bIdeDebugSettings ? "ChimeraIdeSettings.log" : "ChimeraEditor.log";
  return BaseApplication::launch(
      argc, argv, {.logFileName = logFileName, .bProjectOptional = true}, [] {
        if (CommandLine::hasFlag(IdeDebugSettingsApplication::COMMAND_LINE_FLAG)) {
          BaseApplication::startUp<IdeDebugSettingsApplication>();
        }
        // Without -project the editor first asks for one.
        else if (EnginePaths::hasProject()) {
          BaseApplication::startUp<EditorApplication>();
        }
        else {
          BaseApplication::startUp<ProjectBrowserApplication>();
        }
      });
}
