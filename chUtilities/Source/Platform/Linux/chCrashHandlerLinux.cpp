/************************************************************************/
/**
 * @file chCrashHandlerLinux.cpp
 * @author AccelMR
 * @date 2026/11/27
 * @brief
 *  CrashHandler on Linux: signal handlers on an alternate stack write the report, then the
 *  signal is raised again so the system keeps its core dump.
 */
/************************************************************************/
#include "chCrashHandler.h"

#if USING(CH_PLATFORM_LINUX)

#include "chCrashHandlerPrivate.h"
#include "chLogger.h"
#include "chSTDThreading.h"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>

#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <ucontext.h>
#include <unistd.h>

namespace chEngineSDK {
namespace {
constexpr int32 kCaughtSignals[] = {SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT, SIGTRAP};
constexpr SIZE_T kSignalCount = sizeof(kCaughtSignals) / sizeof(kCaughtSignals[0]);
// Room for the handler when the thread's own stack is gone (stack overflow).
constexpr SIZE_T kAlternateStackBytes = 128 * 1024;
// A fault this close below a thread's stack is taken for a stack overflow.
constexpr SIZE_T kStackOverflowMargin = 256 * 1024;
constexpr int32 kMaxFrames = 64;
constexpr SIZE_T kMaxPathChars = 1024;

struct LinuxThread
{
  Atomic<uint64> threadId{0};
  SIZE_T stackLow = 0;
};

struct LinuxState
{
  bool bInstalled = false;
  Atomic<uint64> crashingThread{0};
  struct sigaction previousActions[kSignalCount] = {};
  std::terminate_handler previousTerminate = nullptr;
  int64 utcOffsetSeconds = 0;
  LinuxThread threads[CrashHandler::MAX_THREADS];
  Atomic<uint32> threadCount{0};
  ANSICHAR reportDirectory[kMaxPathChars] = {};
  ANSICHAR folder[kMaxPathChars] = {};
  ANSICHAR filePath[kMaxPathChars] = {};
  ANSICHAR message[CrashDetail::MAX_MESSAGE_CHARS] = {};
  ANSICHAR report[CrashDetail::REPORT_BUFFER_SIZE] = {};
};

// Made at install and never freed, so nothing is allocated when a crash happens.
LinuxState&
getLinuxState()
{
  static LinuxState* state = new LinuxState();
  return *state;
}

uint64
getThreadId() noexcept
{
  return static_cast<uint64>(syscall(SYS_gettid));
}

void
writeAll(int32 file, const ANSICHAR* data, SIZE_T size) noexcept
{
  while (size > 0) {
    const auto written = write(file, data, size);
    if (written < 0 && errno == EINTR) {
      continue;
    }
    if (written <= 0) {
      return;
    }
    data += written;
    size -= static_cast<SIZE_T>(written);
  }
}

void
setPath(ANSICHAR* buffer, StringView first, StringView second, StringView third) noexcept
{
  CrashReportWriter writer(buffer, kMaxPathChars - 1);
  writer.append(first);
  writer.append(second);
  writer.append(third);
  buffer[writer.getText().size()] = '\0';
}

/**
 * Makes the folder and every missing parent; errors show when the files are written.
 */
void
createDirectories(ANSICHAR* path) noexcept
{
  for (SIZE_T i = 1; path[i] != '\0'; ++i) {
    if (path[i] == '/') {
      path[i] = '\0';
      mkdir(path, 0755);
      path[i] = '/';
    }
  }
  mkdir(path, 0755);
}

/**
 * Local time without localtime_r, which may take a lock: the offset is read at install.
 */
void
formatNow(ANSICHAR* buffer, int64 utcOffsetSeconds) noexcept
{
  const int64 seconds = static_cast<int64>(time(nullptr)) + utcOffsetSeconds;
  int64 days = seconds / 86400;
  int64 secondOfDay = seconds % 86400;
  if (secondOfDay < 0) {
    secondOfDay += 86400;
    --days;
  }
  // Civil date from days since 1970 (Howard Hinnant's algorithm).
  days += 719468;
  const int64 era = (days >= 0 ? days : days - 146096) / 146097;
  const int64 dayOfEra = days - era * 146097;
  const int64 yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) /
                          365;
  const int64 dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
  const int64 monthIndex = (5 * dayOfYear + 2) / 153;
  const int64 day = dayOfYear - (153 * monthIndex + 2) / 5 + 1;
  const int64 month = monthIndex < 10 ? monthIndex + 3 : monthIndex - 9;
  const int64 year = yearOfEra + era * 400 + (month <= 2 ? 1 : 0);
  CrashDetail::formatTime(buffer,
                          static_cast<int32>(year),
                          static_cast<int32>(month),
                          static_cast<int32>(day),
                          static_cast<int32>(secondOfDay / 3600),
                          static_cast<int32>(secondOfDay / 60 % 60),
                          static_cast<int32>(secondOfDay % 60));
}

NODISCARD bool
isDebuggerAttached() noexcept
{
  const int32 file = open("/proc/self/status", O_RDONLY | O_CLOEXEC);
  if (file < 0) {
    return false;
  }
  ANSICHAR text[4096];
  const auto size = read(file, text, sizeof(text) - 1);
  close(file);
  if (size <= 0) {
    return false;
  }
  text[size] = '\0';
  const ANSICHAR* tracer = std::strstr(text, "TracerPid:");
  if (!tracer) {
    return false;
  }
  tracer += 10;
  while (*tracer == ' ' || *tracer == '\t') {
    ++tracer;
  }
  return *tracer != '0';
}

NODISCARD bool
isStackOverflow(uint64 threadId, SIZE_T faultAddress) noexcept
{
  const LinuxState& state = getLinuxState();
  const uint32 count = state.threadCount.load() < CrashHandler::MAX_THREADS
                           ? state.threadCount.load()
                           : CrashHandler::MAX_THREADS;
  for (uint32 i = 0; i < count; ++i) {
    const LinuxThread& thread = state.threads[i];
    if (thread.threadId.load() == threadId && thread.stackLow != 0) {
      return faultAddress + kStackOverflowMargin >= thread.stackLow &&
             faultAddress < thread.stackLow + kStackOverflowMargin;
    }
  }
  return false;
}

NODISCARD SIZE_T
getProgramCounter(const void* context) noexcept
{
  if (!context) {
    return 0;
  }
  const ucontext_t* userContext = static_cast<const ucontext_t*>(context);
#if USING(CH_ARCHITECTURE_ARM64)
  return static_cast<SIZE_T>(userContext->uc_mcontext.pc);
#else
  return static_cast<SIZE_T>(userContext->uc_mcontext.gregs[REG_RIP]);
#endif
}

void
appendSignalReason(CrashReportWriter& report,
                   int32 signalNumber,
                   const siginfo_t* info,
                   const void* context,
                   uint64 threadId) noexcept
{
  const SIZE_T faultAddress = info ? reinterpret_cast<SIZE_T>(info->si_addr) : 0;
  switch (signalNumber) {
  case SIGSEGV:
  case SIGBUS:
    if (isStackOverflow(threadId, faultAddress)) {
      report.append("Stack overflow");
      break;
    }
    report.append(signalNumber == SIGSEGV ? "Access violation" : "Bus error");
#if !USING(CH_ARCHITECTURE_ARM64)
    if (context) {
      // Bit 1 of the page fault error code is set for writes.
      const ucontext_t* userContext = static_cast<const ucontext_t*>(context);
      report.append((userContext->uc_mcontext.gregs[REG_ERR] & 2) != 0 ? " writing "
                                                                        : " reading ");
    }
    else {
      report.append(" at address ");
    }
#else
    report.append(" at address ");
#endif
    report.appendHex(faultAddress, 16);
    break;
  case SIGFPE:
    report.append(info && info->si_code == FPE_INTDIV ? "Integer divide by zero"
                                                     : "Floating point error");
    break;
  case SIGILL: report.append("Illegal instruction"); break;
  case SIGTRAP: report.append("Breakpoint"); break;
  case SIGABRT: report.append("abort() was called"); break;
  default:
    report.append("Signal ");
    report.appendNumber(signalNumber);
    break;
  }
  report.append(" at ");
  report.appendHex(getProgramCounter(context), 16);
}

void
appendThreadName(CrashReportWriter& report, uint64 threadId) noexcept
{
  const ANSICHAR* name = nullptr;
  int32 index = -1;
  report.append("Thread: ");
  if (CrashDetail::findThread(threadId, name, index)) {
    report.append(name);
    if (index >= 0) {
      report.append(" ");
      report.appendNumber(index);
    }
  }
  else {
    report.append("not started by the engine");
  }
  report.append(" (id ");
  report.appendNumber(static_cast<int64>(threadId));
  report.append(")\n");
}

void
appendCommandLine(CrashReportWriter& report) noexcept
{
  report.append("Command line:");
  const int32 file = open("/proc/self/cmdline", O_RDONLY | O_CLOEXEC);
  if (file >= 0) {
    ANSICHAR text[2048];
    const auto size = read(file, text, sizeof(text));
    close(file);
    // The arguments are separated by '\0'.
    SIZE_T start = 0;
    for (SIZE_T i = 0; size > 0 && i < static_cast<SIZE_T>(size); ++i) {
      if (text[i] == '\0') {
        report.append(" ");
        report.append(StringView(text + start, i - start));
        start = i + 1;
      }
    }
  }
  report.append("\n");
}

void
appendCallStack(CrashReportWriter& report, const void* context) noexcept
{
  void* frames[kMaxFrames];
  const int32 count = backtrace(frames, kMaxFrames);

  // The first frames are this handler and the signal trampoline; the stack of the program
  // starts at the address that faulted.
  const SIZE_T programCounter = getProgramCounter(context);
  int32 first = 0;
  if (programCounter != 0) {
    for (int32 i = 0; i < count; ++i) {
      if (reinterpret_cast<SIZE_T>(frames[i]) == programCounter) {
        first = i;
        break;
      }
    }
  }

  report.append("\nCall stack:\n");
  for (int32 i = first; i < count; ++i) {
    const SIZE_T address = reinterpret_cast<SIZE_T>(frames[i]);
    report.append("  #");
    report.appendNumber(i - first);
    report.append(" ");
    report.appendHex(address, 16);

    Dl_info info{};
    // Past the first frame the address is where the call returns to, so one byte back is
    // still inside the function that called.
    const SIZE_T lookup = i == first ? address : address - 1;
    if (dladdr(reinterpret_cast<void*>(lookup), &info) != 0 && info.dli_fname) {
      const ANSICHAR* module = std::strrchr(info.dli_fname, '/');
      report.append(" ");
      report.append(module ? module + 1 : info.dli_fname);
      if (info.dli_sname) {
        report.append("!");
        report.append(info.dli_sname);
        report.append(" + ");
        report.appendHex(address - reinterpret_cast<SIZE_T>(info.dli_saddr));
      }
      else {
        report.append(" + ");
        report.appendHex(address - reinterpret_cast<SIZE_T>(info.dli_fbase));
      }
    }
    report.append("\n");
  }
}

void
copyLog(CrashReportWriter& report, LinuxState& state, StringView reason) noexcept
{
  ANSICHAR line[CrashDetail::MAX_MESSAGE_CHARS];
  CrashReportWriter lineWriter(line, sizeof(line));
  lineWriter.append("The program crashed: ");
  lineWriter.append(reason);
  lineWriter.append(". Report in ");
  lineWriter.append(state.folder);
  const bool bFlushed = Logger::flushForCrash(lineWriter.getText());

  report.append("Log: ");
  if (!Logger::isStarted() || Logger::instance().getLogFilePath().empty()) {
    report.append("none\n");
    return;
  }

  ANSICHAR source[kMaxPathChars];
  setPath(source, Logger::instance().getLogFilePath(), "", "");
  setPath(state.filePath, state.folder, "/", CrashDetail::getApplicationName());
  setPath(state.filePath, state.filePath, ".log", "");
  const int32 input = open(source, O_RDONLY | O_CLOEXEC);
  const int32 output = open(state.filePath, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
  if (input < 0 || output < 0) {
    report.append("could not be copied, error ");
    report.appendNumber(errno);
    report.append("\n");
  }
  else {
    ANSICHAR buffer[4096];
    while (true) {
      const auto size = read(input, buffer, sizeof(buffer));
      if (size < 0 && errno == EINTR) {
        continue;
      }
      if (size <= 0) {
        break;
      }
      writeAll(output, buffer, static_cast<SIZE_T>(size));
    }
    report.append(CrashDetail::getApplicationName());
    report.append(bFlushed ? ".log\n"
                           : ".log (it may miss the last lines: the Logger was busy)\n");
  }
  if (input >= 0) {
    close(input);
  }
  if (output >= 0) {
    close(output);
  }
}

void
makeReportFolder(LinuxState& state, const ANSICHAR* time) noexcept
{
  for (int32 attempt = 1; attempt < 10; ++attempt) {
    setPath(state.folder, state.reportDirectory, "/", CrashDetail::getApplicationName());
    setPath(state.folder, state.folder, "-", time);
    if (attempt > 1) {
      const ANSICHAR suffix[] = {'_', static_cast<ANSICHAR>('0' + attempt), '\0'};
      setPath(state.folder, state.folder, suffix, "");
    }
    // Another run that crashed in the same second already has the name.
    struct stat status{};
    if (stat(state.folder, &status) != 0) {
      break;
    }
  }
  createDirectories(state.folder);
}

/**
 * Writes the whole report. info and context are null for a fatal error, which has a message
 * instead.
 */
void
writeCrashReport(int32 signalNumber,
                 const siginfo_t* info,
                 const void* context,
                 const ANSICHAR* fatalMessage) noexcept
{
  LinuxState& state = getLinuxState();
  const uint64 threadId = getThreadId();
  ANSICHAR time[20];
  formatNow(time, state.utcOffsetSeconds);
  makeReportFolder(state, time);

  CrashReportWriter report(state.report, sizeof(state.report));
  report.appendLine("Chimera crash report");
  report.append("Application: ");
  report.appendLine(CrashDetail::getApplicationName());
  CrashDetail::appendBuildInfo(report);
  report.append("Time: ");
  report.appendLine(time);

  ANSICHAR reason[CrashDetail::MAX_MESSAGE_CHARS];
  CrashReportWriter reasonWriter(reason, sizeof(reason));
  if (fatalMessage) {
    reasonWriter.append("Fatal error: ");
    reasonWriter.append(fatalMessage);
  }
  else {
    appendSignalReason(reasonWriter, signalNumber, info, context, threadId);
  }
  report.append("Reason: ");
  report.appendLine(reasonWriter.getText());
  if (fatalMessage) {
    report.append("Message: ");
    report.appendLine(fatalMessage);
  }
  appendThreadName(report, threadId);
  appendCommandLine(report);
  report.appendLine("Memory dump: none; the system keeps a core dump when it is enabled "
                    "(coredumpctl, ulimit -c)");

  copyLog(report, state, reasonWriter.getText());
  appendCallStack(report, context);

  report.append("\nFrom the program:\n");
  CrashDetail::runCallbacks(report);
  if (report.wasCut()) {
    static constexpr StringView kCut = "\n(cut: the report was too long)\n";
    std::memcpy(state.report + sizeof(state.report) - kCut.size(), kCut.data(), kCut.size());
  }

  setPath(state.filePath, state.folder, "/CrashReport.txt", "");
  const int32 file = open(state.filePath, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
  if (file >= 0) {
    writeAll(file, report.getText().data(), report.getText().size());
    close(file);
  }
  CrashDetail::showDialog(state.folder);
}

/**
 * Only the first crash is reported: true for the thread that writes it. Another thread that
 * crashes meanwhile waits for the process to end; the same thread crashing again (its own
 * abort after a fatal report) gets false.
 */
NODISCARD bool
startCrash() noexcept
{
  LinuxState& state = getLinuxState();
  const uint64 threadId = getThreadId();
  uint64 expected = 0;
  if (state.crashingThread.compare_exchange_strong(expected, threadId)) {
    return true;
  }
  if (expected != threadId) {
    while (true) {
      pause();
    }
  }
  return false;
}

void
raiseWithDefaultAction(int32 signalNumber) noexcept
{
  struct sigaction action{};
  action.sa_handler = SIG_DFL;
  sigemptyset(&action.sa_mask);
  sigaction(signalNumber, &action, nullptr);
  raise(signalNumber);
}

void
onSignal(int32 signalNumber, siginfo_t* info, void* context)
{
  if (CrashDetail::isEnabled() && !isDebuggerAttached() && startCrash()) {
    writeCrashReport(signalNumber, info, context, nullptr);
  }
  // A fault returns to the instruction that faulted, which faults again with the default
  // action; signals sent by raise or kill need to be raised again.
  raiseWithDefaultAction(signalNumber);
}

void
onTerminate()
{
  LinuxState& state = getLinuxState();
  CrashReportWriter writer(state.message, sizeof(state.message) - 1);
  const std::exception_ptr exception = std::current_exception();
  if (!exception) {
    writer.append("std::terminate was called");
  }
  else {
    try {
      std::rethrow_exception(exception);
    } catch (const std::exception& e) {
      writer.append("C++ exception that nothing caught: ");
      writer.append(e.what());
    } catch (...) {
      writer.append("C++ exception of an unknown type that nothing caught");
    }
  }
  state.message[writer.getText().size()] = '\0';
  CrashDetail::platformReportFatal(state.message);
}
} // namespace

namespace CrashDetail {

/*
 */
bool
platformInstall()
{
  LinuxState& state = getLinuxState();

  // The first call loads the unwinder, which allocates; never inside a signal handler.
  void* frames[2];
  CH_PARAMETER_UNUSED(backtrace(frames, 2));

  const time_t now = time(nullptr);
  struct tm localTime{};
  if (localtime_r(&now, &localTime)) {
    state.utcOffsetSeconds = static_cast<int64>(localTime.tm_gmtoff);
  }

  struct sigaction action{};
  action.sa_sigaction = &onSignal;
  action.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigemptyset(&action.sa_mask);
  for (SIZE_T i = 0; i < kSignalCount; ++i) {
    sigaction(kCaughtSignals[i], &action, &state.previousActions[i]);
  }
  state.previousTerminate = std::set_terminate(&onTerminate);
  state.bInstalled = true;
  return true;
}

/*
 */
void
platformUninstall()
{
  LinuxState& state = getLinuxState();
  for (SIZE_T i = 0; i < kSignalCount; ++i) {
    sigaction(kCaughtSignals[i], &state.previousActions[i], nullptr);
  }
  std::set_terminate(state.previousTerminate);
  state.bInstalled = false;
}

/*
 */
void
platformSetReportDirectory(const Path& directory)
{
  LinuxState& state = getLinuxState();
  setPath(state.reportDirectory, directory.toString(), "", "");
  const SIZE_T length = std::strlen(state.reportDirectory);
  if (length > 1 && state.reportDirectory[length - 1] == '/') {
    state.reportDirectory[length - 1] = '\0';
  }
}

/*
 */
uint64
platformGetCurrentThreadId() noexcept
{
  return getThreadId();
}

/*
 */
void
platformPrepareThread(const ANSICHAR* name, int32 index)
{
  LinuxState& state = getLinuxState();

  // Kept for the life of the program: the thread may crash at any time.
  stack_t alternateStack{};
  alternateStack.ss_sp = std::malloc(kAlternateStackBytes);
  alternateStack.ss_size = kAlternateStackBytes;
  if (alternateStack.ss_sp) {
    sigaltstack(&alternateStack, nullptr);
  }

  const uint32 slot = state.threadCount.fetch_add(1);
  if (slot < CrashHandler::MAX_THREADS) {
    pthread_attr_t attributes;
    if (pthread_getattr_np(pthread_self(), &attributes) == 0) {
      void* stackAddress = nullptr;
      SIZE_T stackSize = 0;
      if (pthread_attr_getstack(&attributes, &stackAddress, &stackSize) == 0) {
        state.threads[slot].stackLow = reinterpret_cast<SIZE_T>(stackAddress);
      }
      pthread_attr_destroy(&attributes);
    }
    state.threads[slot].threadId.store(getThreadId());
  }

  // Thread names hold at most 15 characters.
  ANSICHAR threadName[16] = {};
  CrashReportWriter writer(threadName, sizeof(threadName) - 1);
  writer.append(name);
  if (index >= 0) {
    writer.append(" ");
    writer.appendNumber(index);
  }
  threadName[writer.getText().size()] = '\0';
  pthread_setname_np(pthread_self(), threadName);
}

/*
 */
void
platformReportFatal(const ANSICHAR* message) noexcept
{
  if (getLinuxState().bInstalled && isEnabled() && !isDebuggerAttached() && startCrash()) {
    writeCrashReport(SIGABRT, nullptr, nullptr, message);
  }
  raiseWithDefaultAction(SIGABRT);
  std::_Exit(EXIT_FAILURE);
}

} // namespace CrashDetail
} // namespace chEngineSDK

#endif // USING(CH_PLATFORM_LINUX)
