/************************************************************************/
/**
 * @file chCrashHandler.cpp
 * @author AccelMR
 * @date 2026/11/27
 * @brief
 *  The parts of the crash handler every platform shares.
 */
/************************************************************************/
#include "chCrashHandler.h"

#include "chAlgorithm.h"
#include "chConsole.h"
#include "chConsoleVariable.h"
#include "chCrashHandlerPrivate.h"
#include "chFileSystem.h"
#include "chJobSystem.h"
#include "chLogger.h"
#include "chMath.h"
#include "chSTDThreading.h"
#include "chStringUtils.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <thread>

namespace chEngineSDK {
CH_LOG_DECLARE_STATIC(CrashLog, All);

namespace {
ConsoleVariable<bool> g_cvarCrashEnabled("Crash.Enabled",
                                         true,
                                         "Writes a report (memory dump, call stack, log) when "
                                         "the program crashes. Never with a debugger "
                                         "attached.");
ConsoleVariable<String> g_cvarCrashDumpType("Crash.DumpType",
                                            "Small",
                                            "Memory dump of a crash: Small (stacks and "
                                            "threads, a few MB), Heap (also the memory the "
                                            "program allocated) or Full (all of it).");
ConsoleVariable<int32> g_cvarCrashMaxReports("Crash.MaxReports",
                                             10,
                                             "Crash report folders kept; the oldest go when "
                                             "the program starts. 0 keeps all.",
                                             "",
                                             ConsoleVariableBehavior::RequiresRestart);

struct ThreadEntry
{
  Atomic<uint64> threadId{0};
  const ANSICHAR* name = nullptr;
  int32 index = -1;
};

// Plain storage that lives for the whole program, so a crash during the shut down of statics
// still finds it.
struct SharedState
{
  bool bInstalled = false;
  ANSICHAR applicationName[CrashDetail::MAX_NAME_CHARS] = {};
  Path reportDirectory;
  CrashCallback callbacks[CrashHandler::MAX_CALLBACKS] = {};
  ThreadEntry threads[CrashHandler::MAX_THREADS];
  Atomic<uint32> threadCount{0};
  ANSICHAR fatalMessage[CrashDetail::MAX_MESSAGE_CHARS] = {};
};

SharedState&
getState()
{
  static SharedState* state = new SharedState();
  return *state;
}

constexpr const ANSICHAR* kTestKindNames[] = {
    "access", "abort", "stackoverflow", "terminate", "exception", "divzero", "fatal", "job",
};
static_assert(sizeof(kTestKindNames) / sizeof(kTestKindNames[0]) ==
              static_cast<SIZE_T>(CrashTestKind::COUNT));

// volatile, so the compiler cannot prove what the test crashes do and drop them.
volatile int32* volatile g_nullPointer = nullptr;
volatile int32 g_zero = 0;
volatile bool g_keepRecursing = true;

uint32
recurseForever(uint32 depth)
{
  volatile uint8 buffer[4096];
  buffer[0] = static_cast<uint8>(depth);
  if (g_keepRecursing) {
    return recurseForever(depth + 1) + buffer[0];
  }
  return buffer[0];
}

void
writeThroughNull()
{
  *g_nullPointer = 1;
}

#if USING(CH_CONSOLE)
void
runCrashCommand(const ConsoleArguments& arguments)
{
  const CrashTestKind kind = CrashHandler::findTestKind(arguments.get(0));
  if (kind == CrashTestKind::COUNT) {
    String names;
    for (const ANSICHAR* name : kTestKindNames) {
      if (!names.empty()) {
        names += ", ";
      }
      names += name;
    }
    CH_LOG_WARNING(ConsoleLog, "Name how to crash: {0}.", names);
    return;
  }
  CH_LOG_WARNING(ConsoleLog,
                 "Crashing on purpose ({0}).",
                 CrashHandler::getTestKindName(kind));
  CrashHandler::crashForTest(kind);
}

ConsoleCommand g_commandCrash("crash",
                              "<access|abort|stackoverflow|terminate|exception|divzero|fatal|"
                              "job>",
                              "Crashes the program on purpose, to check the crash report.",
                              &runCrashCommand,
                              true);
#endif // USING(CH_CONSOLE)
} // namespace

/*
 */
CrashReportWriter::CrashReportWriter(ANSICHAR* buffer, SIZE_T capacity) noexcept
 : m_buffer(buffer),
   m_capacity(capacity)
{}

/*
 */
void
CrashReportWriter::append(StringView text) noexcept
{
  const SIZE_T room = m_capacity - m_size;
  SIZE_T count = text.size();
  if (count > room) {
    count = room;
    m_bCut = true;
  }
  if (count > 0) {
    std::memcpy(m_buffer + m_size, text.data(), count);
    m_size += count;
  }
}

/*
 */
void
CrashReportWriter::appendLine(StringView text) noexcept
{
  append(text);
  append("\n");
}

/*
 */
void
CrashReportWriter::appendNumber(int64 value) noexcept
{
  ANSICHAR digits[StringUtils::MAX_INTEGER_CHARS];
  append(StringUtils::toChars(digits, value));
}

/*
 */
void
CrashReportWriter::appendHex(uint64 value, uint32 minDigits) noexcept
{
  static constexpr ANSICHAR kDigits[] = "0123456789abcdef";
  ANSICHAR text[16];
  uint32 count = 0;
  do {
    text[15 - count] = kDigits[value & 0xF];
    value >>= 4;
    ++count;
  } while (value != 0 && count < 16);
  while (count < minDigits && count < 16) {
    text[15 - count] = '0';
    ++count;
  }
  append("0x");
  append(StringView(text + 16 - count, count));
}

namespace CrashDetail {

/*
 */
bool
isEnabled() noexcept
{
  return g_cvarCrashEnabled.get();
}

/*
 */
DumpType
getDumpType() noexcept
{
  const String& type = g_cvarCrashDumpType.get();
  if (StringUtils::equalsIgnoreCase(type, "Full")) {
    return DumpType::Full;
  }
  if (StringUtils::equalsIgnoreCase(type, "Heap")) {
    return DumpType::Heap;
  }
  return DumpType::Small;
}

/*
 */
void
runCallbacks(CrashReportWriter& report) noexcept
{
  for (CrashCallback callback : getState().callbacks) {
    if (callback) {
      callback(report);
    }
  }
}

/*
 */
void
appendBuildInfo(CrashReportWriter& report) noexcept
{
  report.appendLine("Engine version: " CH_ENGINE_VERSION_STRING);
#if USING(CH_DEBUG_MODE)
  report.append("Build: Debug");
#else
  report.append("Build: Release");
#endif
#if USING(CH_COMPILER_CLANG)
  report.appendLine(", clang");
#elif USING(CH_COMPILER_MSVC)
  report.appendLine(", MSVC");
#else
  report.appendLine(", GCC");
#endif
}

/*
 */
void
formatTime(ANSICHAR* buffer,
           int32 year,
           int32 month,
           int32 day,
           int32 hour,
           int32 minute,
           int32 second) noexcept
{
  const int32 values[] = {year, month, day, hour, minute, second};
  const ANSICHAR separators[] = {'.', '.', '-', '.', '.', '\0'};
  SIZE_T position = 0;
  for (SIZE_T i = 0; i < 6; ++i) {
    const int32 width = i == 0 ? 4 : 2;
    int32 value = values[i];
    for (int32 digit = width - 1; digit >= 0; --digit) {
      buffer[position + static_cast<SIZE_T>(digit)] = static_cast<ANSICHAR>('0' + value % 10);
      value /= 10;
    }
    position += static_cast<SIZE_T>(width);
    buffer[position++] = separators[i];
  }
}

/*
 */
const ANSICHAR*
getApplicationName() noexcept
{
  return getState().applicationName;
}

/*
 */
bool
findThread(uint64 threadId, const ANSICHAR*& outName, int32& outIndex) noexcept
{
  SharedState& state = getState();
  const uint32 count = Math::min(state.threadCount.load(), CrashHandler::MAX_THREADS);
  for (uint32 i = 0; i < count; ++i) {
    const ThreadEntry& entry = state.threads[i];
    if (entry.threadId.load() == threadId) {
      outName = entry.name;
      outIndex = entry.index;
      return outName != nullptr;
    }
  }
  return false;
}

} // namespace CrashDetail

/*
 */
void
CrashHandler::install(StringView applicationName)
{
  SharedState& state = getState();
  if (state.bInstalled) {
    return;
  }
  CH_PARAMETER_UNUSED(StringUtils::copyToBuffer(state.applicationName, applicationName));
  if (state.reportDirectory.empty()) {
    state.reportDirectory = FileSystem::getExecutableDirectory().join(Path("Crashes"));
  }
  CrashDetail::platformSetReportDirectory(state.reportDirectory);
  state.bInstalled = CrashDetail::platformInstall();
  if (state.bInstalled) {
    registerCurrentThread("Main");
  }
}

/*
 */
void
CrashHandler::uninstall()
{
  SharedState& state = getState();
  if (!state.bInstalled) {
    return;
  }
  CrashDetail::platformUninstall();
  state.bInstalled = false;
}

/*
 */
bool
CrashHandler::isInstalled() noexcept
{
  return getState().bInstalled;
}

/*
 */
void
CrashHandler::setReportDirectory(const Path& directory)
{
  SharedState& state = getState();
  state.reportDirectory = directory;
  CrashDetail::platformSetReportDirectory(directory);
}

/*
 */
void
CrashHandler::removeOldReports()
{
  SharedState& state = getState();
  const int32 maxReports = g_cvarCrashMaxReports.get();
  if (maxReports <= 0 || !FileSystem::isDirectory(state.reportDirectory)) {
    return;
  }

  const String prefix = String(state.applicationName) + "-";
  Vector<Path> files;
  Vector<Path> directories;
  FileSystem::getChildren(state.reportDirectory, files, directories);
  Vector<String> reports;
  for (const Path& directory : directories) {
    String name = directory.getFileName();
    if (name.starts_with(prefix)) {
      reports.push_back(std::move(name));
    }
  }
  if (reports.size() <= static_cast<SIZE_T>(maxReports)) {
    return;
  }

  // The names end with the time, so they sort from the oldest.
  Algorithm::sort(reports);
  const SIZE_T removeCount = reports.size() - static_cast<SIZE_T>(maxReports);
  for (SIZE_T i = 0; i < removeCount; ++i) {
    CH_PARAMETER_UNUSED(FileSystem::removeAll(state.reportDirectory.join(Path(reports[i]))));
  }
}

/*
 */
void
CrashHandler::registerCurrentThread(const ANSICHAR* name, int32 index)
{
  SharedState& state = getState();
  const uint32 slot = state.threadCount.fetch_add(1);
  if (slot >= MAX_THREADS) {
    return;
  }
  ThreadEntry& entry = state.threads[slot];
  entry.name = name;
  entry.index = index;
  entry.threadId.store(CrashDetail::platformGetCurrentThreadId());
  CrashDetail::platformPrepareThread(name, index);
}

/*
 */
bool
CrashHandler::addCallback(CrashCallback callback)
{
  for (CrashCallback& slot : getState().callbacks) {
    if (!slot) {
      slot = callback;
      return true;
    }
  }
  return false;
}

/*
 */
void
CrashHandler::removeCallback(CrashCallback callback)
{
  for (CrashCallback& slot : getState().callbacks) {
    if (slot == callback) {
      slot = nullptr;
    }
  }
}

/*
 */
void
CrashHandler::reportFatal(const ANSICHAR* message, const ANSICHAR* file, int32 line) noexcept
{
  SharedState& state = getState();
  CrashReportWriter writer(state.fatalMessage, sizeof(state.fatalMessage) - 1);
  writer.append(message ? message : "Fatal error");
  if (file) {
    writer.append(" (");
    writer.append(file);
    writer.append(":");
    writer.appendNumber(line);
    writer.append(")");
  }
  state.fatalMessage[writer.getText().size()] = '\0';

  if (!state.bInstalled) {
    CH_LOG_FATAL(CrashLog, "{0}", StringView(state.fatalMessage));
    std::abort();
  }
  CrashDetail::platformReportFatal(state.fatalMessage);
}

/*
 */
void
CrashHandler::crashForTest(CrashTestKind kind)
{
  switch (kind) {
  case CrashTestKind::AccessViolation:
    writeThroughNull();
    break;
  case CrashTestKind::Abort:
    std::abort();
  case CrashTestKind::StackOverflow:
    CH_PARAMETER_UNUSED(recurseForever(0));
    break;
  case CrashTestKind::Terminate:
    std::terminate();
  case CrashTestKind::UncaughtException: {
    // An exception that leaves a thread's function ends the program through std::terminate.
    // The thread registers like the engine's own, which gives it the terminate handler.
    Thread thread([] {
      registerCurrentThread("CrashTest");
      throw std::runtime_error("Exception thrown on purpose");
    });
    thread.join();
    break;
  }
  case CrashTestKind::DivideByZero: {
    const volatile int32 result = 1 / g_zero;
    CH_PARAMETER_UNUSED(result);
    break;
  }
  case CrashTestKind::Fatal:
    reportFatal("Fatal error raised on purpose", __FILE__, __LINE__);
  case CrashTestKind::Job: {
    // Not JobSystem::wait, which would run the job on this thread.
    const JobHandle job = JobSystem::submit("CrashTest", [] { writeThroughNull(); });
    while (!JobSystem::isDone(job)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    break;
  }
  case CrashTestKind::COUNT:
    break;
  }
  // Divide by zero does not trap on every processor.
  reportFatal("The test crash did not crash", __FILE__, __LINE__);
}

/*
 */
const ANSICHAR*
CrashHandler::getTestKindName(CrashTestKind kind) noexcept
{
  const SIZE_T index = static_cast<SIZE_T>(kind);
  return index < static_cast<SIZE_T>(CrashTestKind::COUNT) ? kTestKindNames[index] : "";
}

/*
 */
CrashTestKind
CrashHandler::findTestKind(StringView name) noexcept
{
  for (SIZE_T i = 0; i < static_cast<SIZE_T>(CrashTestKind::COUNT); ++i) {
    if (StringUtils::equalsIgnoreCase(name, kTestKindNames[i])) {
      return static_cast<CrashTestKind>(i);
    }
  }
  return CrashTestKind::COUNT;
}

} // namespace chEngineSDK
