/************************************************************************/
/**
 * @file chCrashHandlerLinux.cpp
 * @author AccelMR
 * @date 2026/11/27
 * @brief
 *  CrashHandler on Linux. Signals are not caught yet, so crashes are left to the system.
 */
/************************************************************************/
#include "chCrashHandler.h"

#if USING(CH_PLATFORM_LINUX)

#include "chCrashHandlerPrivate.h"

#include <cstdio>
#include <cstdlib>

#include <unistd.h>
#include <sys/syscall.h>

namespace chEngineSDK {
namespace CrashDetail {

/*
 */
bool
platformInstall()
{
  return true;
}

/*
 */
void
platformUninstall()
{}

/*
 */
void
platformSetReportDirectory(const Path& directory)
{
  CH_PARAMETER_UNUSED(directory);
}

/*
 */
uint64
platformGetCurrentThreadId() noexcept
{
  return static_cast<uint64>(syscall(SYS_gettid));
}

/*
 */
void
platformPrepareThread(const ANSICHAR* name, int32 index)
{
  CH_PARAMETER_UNUSED(name);
  CH_PARAMETER_UNUSED(index);
}

/*
 */
void
platformReportFatal(const ANSICHAR* message) noexcept
{
  std::fputs(message, stderr);
  std::fputs("\n", stderr);
  std::abort();
}

} // namespace CrashDetail
} // namespace chEngineSDK

#endif // USING(CH_PLATFORM_LINUX)
