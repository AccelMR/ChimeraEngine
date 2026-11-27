/************************************************************************/
/**
 * @file chCrashTestMain.cpp
 * @author AccelMR
 * @date 2026/11/27
 * @brief
 *  A program that crashes on purpose, so the tests can check the crash report it leaves.
 *  Usage: chCrashTest <kind> <report folder>
 */
/************************************************************************/
#include "chCrashHandler.h"
#include "chJobSystem.h"
#include "chLogger.h"
#include "chPath.h"

using namespace chEngineSDK;

CH_LOG_DECLARE_STATIC(CrashTestLog, All);

int32
main(int32 argc, ANSICHAR* argv[])
{
  if (argc < 3) {
    return 2;
  }
  const CrashTestKind kind = CrashHandler::findTestKind(argv[1]);
  if (kind == CrashTestKind::COUNT) {
    return 2;
  }

  CrashHandler::install("CrashTest");
  const Path folder(argv[2]);
  CrashHandler::setReportDirectory(folder);

  Logger::startUp();
  Logger::instance().setFileOutput(true, folder.join(Path("CrashTest.log")).toString());
  // Info lines are not flushed when written, so finding it in the copied log shows the crash
  // flushed the Logger.
  CH_LOG_INFO(CrashTestLog, "Last line before the crash");

  if (kind == CrashTestKind::Job) {
    JobSystem::startUp();
  }
  CrashHandler::crashForTest(kind);
}
