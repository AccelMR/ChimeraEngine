/************************************************************************/
/**
 * @file chCrashHandlerTests.cpp
 * @author AccelMR
 * @date 2026/11/27
 * @brief Tests of the crash handler: the report writer, and real crashes of a child program.
 */
/************************************************************************/
#include <cstdlib>

#include "chCrashHandler.h"
#include "chFileSystem.h"
#include "chStringUtils.h"

#include "catch.hpp"

using namespace chEngineSDK;

TEST_CASE("chUtilities - Crash report writer", "[Crash]")
{
  ANSICHAR buffer[32];
  CrashReportWriter writer(buffer, sizeof(buffer));
  writer.append("Value ");
  writer.appendNumber(-42);
  writer.append(" ");
  writer.appendHex(255, 4);
  REQUIRE(writer.getText() == "Value -42 0x00ff");
  REQUIRE_FALSE(writer.wasCut());

  writer.appendLine(" and a line that is far too long");
  REQUIRE(writer.getText().size() == sizeof(buffer));
  REQUIRE(writer.wasCut());

  ANSICHAR hex[32];
  CrashReportWriter hexWriter(hex, sizeof(hex));
  hexWriter.appendHex(0);
  hexWriter.append(" ");
  hexWriter.appendHex(0xFFFFFFFFFFFFFFFFull, 20);
  REQUIRE(hexWriter.getText() == "0x0 0xffffffffffffffff");
}

TEST_CASE("chUtilities - Crash test kinds", "[Crash]")
{
  for (uint32 i = 0; i < static_cast<uint32>(CrashTestKind::COUNT); ++i) {
    const CrashTestKind kind = static_cast<CrashTestKind>(i);
    REQUIRE(CrashHandler::findTestKind(CrashHandler::getTestKindName(kind)) == kind);
  }
  REQUIRE(CrashHandler::findTestKind("ACCESS") == CrashTestKind::AccessViolation);
  REQUIRE(CrashHandler::findTestKind("nothing") == CrashTestKind::COUNT);
}

#if USING(CH_PLATFORM_WIN32) || USING(CH_PLATFORM_LINUX)
namespace {
struct CrashRun
{
  int32 exitCode = 0;
  String report;
  String log;
  bool bHasDump = false;
};

String
readText(const Path& file)
{
  const Vector<uint8> bytes = FileSystem::fastRead(file);
  return String(bytes.begin(), bytes.end());
}

CrashRun
runCrash(StringView kindName)
{
  const Path directory = FileSystem::getExecutableDirectory();
  const Path root = directory.join(Path("CrashTestReports"));
  const Path reports = root.join(Path(String(kindName)));
  CH_PARAMETER_UNUSED(FileSystem::removeAll(reports));
  if (!FileSystem::isDirectory(root)) {
    REQUIRE(FileSystem::createDirectory(root));
  }
  REQUIRE(FileSystem::createDirectory(reports));

#if USING(CH_PLATFORM_WIN32)
  const String program = StringUtils::replaceAllChars(
      directory.join(Path("chCrashTest.exe")).toString(), '/', '\\');
  const String folder = StringUtils::replaceAllChars(reports.toString(), '/', '\\');
  // cmd drops the outer quotes, which keeps the quotes around each path.
  const String command = "\"\"" + program + "\" " + String(kindName) + " \"" + folder + "\"\"";
#else
  const String program = directory.join(Path("chCrashTest")).toString();
  const String command = "'" + program + "' " + String(kindName) + " '" + reports.toString() +
                         "' 2>/dev/null";
#endif

  CrashRun run;
  run.exitCode = std::system(command.c_str());

  Vector<Path> files;
  Vector<Path> directories;
  FileSystem::getChildren(reports, files, directories);
  for (const Path& reportFolder : directories) {
    if (!reportFolder.getFileName().starts_with("CrashTest-")) {
      continue;
    }
    run.report = readText(reportFolder.join(Path("CrashReport.txt")));
    run.log = readText(reportFolder.join(Path("CrashTest.log")));
    run.bHasDump = !FileSystem::fastRead(reportFolder.join(Path("CrashTest.dmp"))).empty();
  }
  return run;
}

bool
contains(const String& text, StringView part)
{
  return text.find(part) != String::npos;
}
} // namespace

TEST_CASE("chUtilities - Crash reports", "[Crash]")
{
  struct Expected
  {
    const ANSICHAR* kind;
    const ANSICHAR* reason;
    const ANSICHAR* thread;
  };
  const Expected expectedRuns[] = {
#if USING(CH_ARCHITECTURE_ARM64) && USING(CH_PLATFORM_LINUX)
      {"access", "Access violation at address 0x0000000000000000", "Thread: Main"},
#else
      {"access", "Access violation writing 0x0000000000000000", "Thread: Main"},
#endif
      {"abort", "abort() was called", "Thread: Main"},
      {"stackoverflow", "Stack overflow", "Thread: Main"},
      {"terminate", "std::terminate was called", "Thread: Main"},
      {"exception", "Exception thrown on purpose", "Thread: CrashTest"},
      {"divzero", "Integer divide by zero", "Thread: Main"},
      {"fatal", "Fatal error raised on purpose", "Thread: Main"},
      {"job", "Access violation", "Thread: JobWorker"},
      {"check", "Check failed: g_zero == 1: zero is 0 on purpose", "Thread: Main"},
  };

  for (const Expected& expected : expectedRuns) {
    DYNAMIC_SECTION(expected.kind)
    {
      const CrashRun run = runCrash(expected.kind);
      INFO(run.report);
      CHECK(run.exitCode != 0);
      CHECK(contains(run.report, "Chimera crash report"));
      CHECK(contains(run.report, expected.reason));
      CHECK(contains(run.report, expected.thread));
      CHECK(contains(run.report, "Call stack:\n  #0"));
      CHECK(contains(run.report, "Log: CrashTest.log\n"));
#if USING(CH_PLATFORM_WIN32)
      CHECK(contains(run.report, "Memory dump: CrashTest.dmp"));
      CHECK(run.bHasDump);
#endif
      CHECK(contains(run.log, "Last line before the crash"));
      CHECK(contains(run.log, "The program crashed"));
    }
  }
}
#endif // USING(CH_PLATFORM_WIN32) || USING(CH_PLATFORM_LINUX)
