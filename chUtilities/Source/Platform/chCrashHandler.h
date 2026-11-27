/************************************************************************/
/**
 * @file chCrashHandler.h
 * @author AccelMR
 * @date 2026/11/27
 * @brief
 *  Catches crashes and writes a report (memory dump, call stack, log) before the process
 *  ends.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesUtilities.h"

namespace chEngineSDK {

/**
 * Exists so callbacks can add text to a crash report without allocating: the crash may have
 * happened inside the allocator, so the report is written into a fixed buffer.
 */
class CH_UTILITY_EXPORT CrashReportWriter
{
 public:
  CrashReportWriter(ANSICHAR* buffer, SIZE_T capacity) noexcept;

  /**
   * Text past the end of the buffer is cut.
   */
  void
  append(StringView text) noexcept;

  void
  appendLine(StringView text) noexcept;

  void
  appendNumber(int64 value) noexcept;

  void
  appendHex(uint64 value, uint32 minDigits = 1) noexcept;

  NODISCARD FORCEINLINE StringView
  getText() const noexcept
  {
    return StringView(m_buffer, m_size);
  }

  NODISCARD FORCEINLINE bool
  wasCut() const noexcept
  {
    return m_bCut;
  }

 private:
  ANSICHAR* m_buffer = nullptr;
  SIZE_T m_capacity = 0;
  SIZE_T m_size = 0;
  bool m_bCut = false;
};

/**
 * Runs while a crash report is written, on the thread that writes it. It must not allocate,
 * lock or wait: the crashed thread may hold the allocator or any lock.
 */
using CrashCallback = void (*)(CrashReportWriter& report);

/**
 * Tells the user once the report is written; folder is where it went. It runs on a crashed
 * process, so it is a best effort.
 */
using CrashDialogFunction = void (*)(const ANSICHAR* applicationName, const ANSICHAR* folder);

/**
 * Ways to crash on purpose, to check the crash handler (console command "crash", tests).
 */
enum class CrashTestKind : uint32
{
  AccessViolation = 0,
  Abort = 1,
  StackOverflow = 2,
  Terminate = 3,
  UncaughtException = 4,
  DivideByZero = 5,
  Fatal = 6,
  Job = 7,
  Check = 8,
  COUNT
};

/**
 * Exists so a crash in the editor or a shipped game leaves a report instead of a process
 * that just disappears: a memory dump, the call stack of the crashed thread and the log,
 * in a folder of their own. Each platform implements it in its own file.
 */
class CH_UTILITY_EXPORT CrashHandler
{
 public:
  /**
   * Starts catching crashes; call it first thing in the program. applicationName names the
   * report folders and files.
   */
  static void
  install(StringView applicationName);

  static void
  uninstall();

  NODISCARD static bool
  isInstalled() noexcept;

  /**
   * Real folder that gets one folder per crash. Until it is set reports go to a "Crashes"
   * folder next to the executable.
   */
  static void
  setReportDirectory(const Path& directory);

  /**
   * Deletes the oldest report folders past Crash.MaxReports.
   */
  static void
  removeOldReports();

  /**
   * Every thread the engine starts calls it once, so a crash names the thread and a stack
   * overflow there still has room to write its report. The name must be a literal.
   */
  static void
  registerCurrentThread(const ANSICHAR* name, int32 index = -1);

  /**
   * False when the list of callbacks is full.
   */
  static bool
  addCallback(CrashCallback callback);

  static void
  removeCallback(CrashCallback callback);

  /**
   * Ends the program with a report that holds the message. Without the handler it only logs
   * and aborts.
   */
  [[noreturn]] static void
  reportFatal(const ANSICHAR* message, const ANSICHAR* file, int32 line) noexcept;

  /**
   * What CH_CHECK and CH_CHECK_MSG call when their condition is false.
   */
  [[noreturn]] static void
  reportCheckFailed(const ANSICHAR* condition,
                    const ANSICHAR* file,
                    int32 line,
                    StringView message = StringView()) noexcept;

  /**
   * Shown after the report is written while Crash.ShowDialog is on; null shows nothing.
   * chUtilities has no window code, so the application gives it.
   */
  static void
  setDialogFunction(CrashDialogFunction function) noexcept;

  [[noreturn]] static void
  crashForTest(CrashTestKind kind);

  NODISCARD static const ANSICHAR*
  getTestKindName(CrashTestKind kind) noexcept;

  /**
   * COUNT when the name matches no kind (ignores case).
   */
  NODISCARD static CrashTestKind
  findTestKind(StringView name) noexcept;

  static constexpr uint32 MAX_CALLBACKS = 16;
  static constexpr uint32 MAX_THREADS = 128;
};

} // namespace chEngineSDK

/**
 * Checks that stay in every build, unlike CH_ASSERT: a false condition ends the program with a
 * crash report. For cheap checks of things that must never happen. CH_CHECK_MSG formats its
 * message with StringUtils::format, so the file must include chStringUtils.h; the message is
 * only built when the check fails.
 */
#define CH_CHECK(condition)                                                                   \
  do {                                                                                        \
    if (!(condition)) [[unlikely]] {                                                          \
      ::chEngineSDK::CrashHandler::reportCheckFailed(#condition, __FILE__, __LINE__);         \
    }                                                                                         \
  } while (false)

#define CH_CHECK_MSG(condition, ...)                                                          \
  do {                                                                                        \
    if (!(condition)) [[unlikely]] {                                                          \
      ::chEngineSDK::CrashHandler::reportCheckFailed(                                         \
          #condition, __FILE__, __LINE__, ::chEngineSDK::StringUtils::format(__VA_ARGS__));   \
    }                                                                                         \
  } while (false)
