/************************************************************************/
/**
 * @file chCrashHandlerPrivate.h
 * @author AccelMR
 * @date 2026/11/27
 * @brief
 *  What the platform crash handlers share: settings, callbacks and the report header.
 */
/************************************************************************/
#pragma once

#include "chCrashHandler.h"
#include "chPath.h"

namespace chEngineSDK {
namespace CrashDetail {

enum class DumpType : uint32
{
  Small,
  Heap,
  Full
};

// Read when a crash happens, so they must not allocate.
NODISCARD bool
isEnabled() noexcept;

NODISCARD DumpType
getDumpType() noexcept;

void
runCallbacks(CrashReportWriter& report) noexcept;

/**
 * Engine version, build kind and compiler, one line each.
 */
void
appendBuildInfo(CrashReportWriter& report) noexcept;

/**
 * "YYYY.MM.DD-HH.MM.SS" into a buffer of at least 20 characters.
 */
void
formatTime(ANSICHAR* buffer,
           int32 year,
           int32 month,
           int32 day,
           int32 hour,
           int32 minute,
           int32 second) noexcept;

NODISCARD const ANSICHAR*
getApplicationName() noexcept;

/**
 * True when a dialog will show after the report (a function is set and Crash.ShowDialog is
 * on), so the crashed thread waits for the user.
 */
NODISCARD bool
hasDialog() noexcept;

void
showDialog(const ANSICHAR* folder) noexcept;

/**
 * Name and index a thread registered with, false when it never did.
 */
NODISCARD bool
findThread(uint64 threadId, const ANSICHAR*& outName, int32& outIndex) noexcept;

// Each platform file implements these.
NODISCARD bool
platformInstall();

void
platformUninstall();

void
platformSetReportDirectory(const Path& directory);

NODISCARD uint64
platformGetCurrentThreadId() noexcept;

// Called once per thread when it registers.
void
platformPrepareThread(const ANSICHAR* name, int32 index);

[[noreturn]] void
platformReportFatal(const ANSICHAR* message) noexcept;

static constexpr SIZE_T MAX_NAME_CHARS = 64;
static constexpr SIZE_T MAX_MESSAGE_CHARS = 2048;
static constexpr SIZE_T REPORT_BUFFER_SIZE = 64 * 1024;

} // namespace CrashDetail
} // namespace chEngineSDK
