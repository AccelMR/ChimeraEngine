/************************************************************************/
/**
 * @file chCrashHandlerWin32.cpp
 * @author AccelMR
 * @date 2026/11/27
 * @brief
 *  CrashHandler on Windows: an unhandled exception filter, a memory dump and the call stack
 *  through dbghelp.
 */
/************************************************************************/
#include "chCrashHandler.h"

#if USING(CH_PLATFORM_WIN32)

#include "chCrashHandlerPrivate.h"
#include "chLogger.h"
#include "chMath.h"
#include "chWindows.h"

#include <DbgHelp.h>
#include <intrin.h>

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <exception>

namespace chEngineSDK {
namespace {
// Names a fatal error in the report, and stops a debugger on it. The top bits mark an
// application code.
constexpr DWORD kFatalErrorCode = 0xE0434846;
constexpr DWORD kCppExceptionCode = 0xE06D7363;
constexpr DWORD kHeapCorruptionCode = 0xC0000374;
// Room left on a thread after a stack overflow, enough for the filter to wake the crash
// thread and wait.
constexpr ULONG kStackGuaranteeBytes = 64 * 1024;
// Past it the crashed thread ends the process anyway, so a report that hangs never leaves a
// process behind.
constexpr DWORD kCrashWaitMilliseconds = 120 * 1000;
constexpr uint32 kMaxFrames = 64;
constexpr SIZE_T kMaxPathChars = 1024;
constexpr SIZE_T kMaxSymbolChars = 512;

using SetThreadDescriptionFunction = HRESULT(WINAPI*)(HANDLE, PCWSTR);

struct WindowsState
{
  HANDLE requestEvent = nullptr;
  HANDLE doneEvent = nullptr;
  HANDLE thread = nullptr;
  DWORD crashThreadId = 0;
  volatile LONG crashing = 0;
  volatile bool bStopping = false;
  EXCEPTION_POINTERS* exception = nullptr;
  DWORD faultingThreadId = 0;
  const ANSICHAR* fatalMessage = nullptr;
  LPTOP_LEVEL_EXCEPTION_FILTER previousFilter = nullptr;
  std::terminate_handler previousTerminate = nullptr;
  _purecall_handler previousPurecall = nullptr;
  _invalid_parameter_handler previousInvalidParameter = nullptr;
  SetThreadDescriptionFunction setThreadDescription = nullptr;
  WIDECHAR reportDirectory[kMaxPathChars] = {};
  WIDECHAR folder[kMaxPathChars] = {};
  WIDECHAR filePath[kMaxPathChars] = {};
  ANSICHAR folderUtf8[kMaxPathChars] = {};
  ANSICHAR message[CrashDetail::MAX_MESSAGE_CHARS] = {};
  ANSICHAR report[CrashDetail::REPORT_BUFFER_SIZE] = {};
};

// Made at install and never freed, so nothing is allocated when a crash happens.
WindowsState&
getWindowsState()
{
  static WindowsState* state = new WindowsState();
  return *state;
}

SIZE_T
getWideLength(const WIDECHAR* text) noexcept
{
  SIZE_T length = 0;
  while (text[length] != L'\0') {
    ++length;
  }
  return length;
}

void
appendWide(WIDECHAR* buffer, const WIDECHAR* text) noexcept
{
  SIZE_T length = getWideLength(buffer);
  for (SIZE_T i = 0; text[i] != L'\0' && length + 1 < kMaxPathChars; ++i) {
    buffer[length++] = text[i];
  }
  buffer[length] = L'\0';
}

void
appendUtf8(WIDECHAR* buffer, StringView text) noexcept
{
  const SIZE_T length = getWideLength(buffer);
  const SIZE_T room = kMaxPathChars - length - 1;
  const int32 written = MultiByteToWideChar(CP_UTF8,
                                            0,
                                            text.data(),
                                            static_cast<int32>(text.size()),
                                            buffer + length,
                                            static_cast<int32>(room));
  buffer[length + static_cast<SIZE_T>(Math::max(written, 0))] = L'\0';
}

/**
 * Makes the folder and every missing parent; errors show when the files are written.
 */
void
createDirectories(WIDECHAR* path) noexcept
{
  for (SIZE_T i = 3; path[i] != L'\0'; ++i) {
    if (path[i] == L'\\') {
      path[i] = L'\0';
      CreateDirectoryW(path, nullptr);
      path[i] = L'\\';
    }
  }
  CreateDirectoryW(path, nullptr);
}

void
appendExceptionReason(CrashReportWriter& report, const EXCEPTION_RECORD& record) noexcept
{
  switch (record.ExceptionCode) {
  case EXCEPTION_ACCESS_VIOLATION:
  case EXCEPTION_IN_PAGE_ERROR: {
    report.append(record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION ? "Access violation"
                                                                     : "In page error");
    if (record.NumberParameters >= 2) {
      const ULONG_PTR operation = record.ExceptionInformation[0];
      report.append(operation == 0   ? " reading "
                    : operation == 8 ? " executing "
                                     : " writing ");
      report.appendHex(static_cast<uint64>(record.ExceptionInformation[1]), 16);
    }
    break;
  }
  case EXCEPTION_STACK_OVERFLOW: report.append("Stack overflow"); break;
  case EXCEPTION_INT_DIVIDE_BY_ZERO: report.append("Integer divide by zero"); break;
  case EXCEPTION_INT_OVERFLOW: report.append("Integer overflow"); break;
  case EXCEPTION_ILLEGAL_INSTRUCTION: report.append("Illegal instruction"); break;
  case EXCEPTION_PRIV_INSTRUCTION: report.append("Privileged instruction"); break;
  case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: report.append("Array bounds exceeded"); break;
  case EXCEPTION_DATATYPE_MISALIGNMENT: report.append("Misaligned data"); break;
  case EXCEPTION_FLT_DIVIDE_BY_ZERO: report.append("Float divide by zero"); break;
  case EXCEPTION_FLT_INVALID_OPERATION: report.append("Invalid float operation"); break;
  case EXCEPTION_FLT_OVERFLOW: report.append("Float overflow"); break;
  case EXCEPTION_FLT_UNDERFLOW: report.append("Float underflow"); break;
  case EXCEPTION_BREAKPOINT: report.append("Breakpoint"); break;
  case EXCEPTION_NONCONTINUABLE_EXCEPTION: report.append("Noncontinuable exception"); break;
  case kCppExceptionCode: report.append("C++ exception that nothing caught"); break;
  case kHeapCorruptionCode: report.append("Heap corruption"); break;
  case kFatalErrorCode: report.append("Fatal error"); break;
  default:
    report.append("Exception ");
    report.appendHex(record.ExceptionCode, 8);
    break;
  }
  report.append(" at ");
  report.appendHex(reinterpret_cast<uint64>(record.ExceptionAddress), 16);
}

void
appendThreadName(CrashReportWriter& report, DWORD threadId) noexcept
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
  report.appendNumber(threadId);
  report.append(")\n");
}

void
appendCommandLine(CrashReportWriter& report) noexcept
{
  ANSICHAR text[2048];
  const int32 size = WideCharToMultiByte(CP_UTF8,
                                         0,
                                         GetCommandLineW(),
                                         -1,
                                         text,
                                         static_cast<int32>(sizeof(text)),
                                         nullptr,
                                         nullptr);
  report.append("Command line: ");
  if (size > 1) {
    report.append(StringView(text, static_cast<SIZE_T>(size - 1)));
  }
  report.append("\n");
}

void
appendFrame(CrashReportWriter& report, HANDLE process, uint32 number, DWORD64 address) noexcept
{
  report.append("  #");
  report.appendNumber(number);
  report.append(" ");
  report.appendHex(address, 16);

  IMAGEHLP_MODULE64 module{};
  module.SizeOfStruct = sizeof(module);
  if (SymGetModuleInfo64(process, address, &module)) {
    report.append(" ");
    report.append(module.ModuleName);
  }

  alignas(SYMBOL_INFO) uint8 symbolBuffer[sizeof(SYMBOL_INFO) + kMaxSymbolChars];
  SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
  std::memset(symbolBuffer, 0, sizeof(symbolBuffer));
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
  symbol->MaxNameLen = static_cast<ULONG>(kMaxSymbolChars - 1);
  DWORD64 symbolOffset = 0;
  // Without a .pdb dbghelp only knows the exported functions and names the closest one,
  // which is often another function, so the report gives the offset in the module instead.
  const bool bRealSymbols = module.SymType == SymPdb || module.SymType == SymDia;
  if (bRealSymbols && SymFromAddr(process, address, &symbolOffset, symbol)) {
    report.append("!");
    report.append(StringView(symbol->Name, symbol->NameLen));
    report.append(" + ");
    report.appendHex(symbolOffset);
  }
  else if (module.BaseOfImage != 0) {
    report.append(" + ");
    report.appendHex(address - module.BaseOfImage);
  }

  IMAGEHLP_LINE64 line{};
  line.SizeOfStruct = sizeof(line);
  DWORD lineOffset = 0;
  if (SymGetLineFromAddr64(process, address, &lineOffset, &line)) {
    report.append("  (");
    report.append(line.FileName);
    report.append(":");
    report.appendNumber(line.LineNumber);
    report.append(")");
  }
  report.append("\n");
}

void
appendCallStack(CrashReportWriter& report, const WindowsState& state) noexcept
{
  const HANDLE process = GetCurrentProcess();
  SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES |
                SYMOPT_FAIL_CRITICAL_ERRORS);
  // The executable's folder first, where a package keeps its symbols; dbghelp also tries
  // the path each module was built with.
  WIDECHAR searchPath[kMaxPathChars] = {};
  const DWORD length =
      GetModuleFileNameW(nullptr, searchPath, static_cast<DWORD>(kMaxPathChars));
  for (DWORD i = length; i > 0; --i) {
    if (searchPath[i - 1] == L'\\') {
      searchPath[i - 1] = L'\0';
      break;
    }
  }
  const bool bSymbols = SymInitializeW(process, searchPath, TRUE) != FALSE;

  CONTEXT context = *state.exception->ContextRecord;
  STACKFRAME64 frame{};
#if USING(CH_ARCHITECTURE_ARM64)
  const DWORD machine = IMAGE_FILE_MACHINE_ARM64;
  frame.AddrPC.Offset = context.Pc;
  frame.AddrStack.Offset = context.Sp;
  frame.AddrFrame.Offset = context.Fp;
#else
  const DWORD machine = IMAGE_FILE_MACHINE_AMD64;
  frame.AddrPC.Offset = context.Rip;
  frame.AddrStack.Offset = context.Rsp;
  frame.AddrFrame.Offset = context.Rbp;
#endif
  frame.AddrPC.Mode = AddrModeFlat;
  frame.AddrStack.Mode = AddrModeFlat;
  frame.AddrFrame.Mode = AddrModeFlat;

  report.append("\nCall stack:\n");
  const HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION,
                                   FALSE,
                                   state.faultingThreadId);
  for (uint32 i = 0; i < kMaxFrames; ++i) {
    if (!StackWalk64(machine,
                     process,
                     thread ? thread : GetCurrentThread(),
                     &frame,
                     &context,
                     nullptr,
                     SymFunctionTableAccess64,
                     SymGetModuleBase64,
                     nullptr) ||
        frame.AddrPC.Offset == 0) {
      break;
    }
    // Past the first frame the address is where the call returns to, so one byte back is
    // still inside the line that called.
    appendFrame(report, process, i, i == 0 ? frame.AddrPC.Offset : frame.AddrPC.Offset - 1);
  }
  if (thread) {
    CloseHandle(thread);
  }
  if (!bSymbols) {
    report.append("  (no symbols: dbghelp could not start)\n");
  }
  else {
    SymCleanup(process);
  }
}

NODISCARD MINIDUMP_TYPE
getMiniDumpType() noexcept
{
  const int32 always = MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules;
  switch (CrashDetail::getDumpType()) {
  case CrashDetail::DumpType::Full:
    return static_cast<MINIDUMP_TYPE>(always | MiniDumpWithFullMemory |
                                      MiniDumpWithFullMemoryInfo | MiniDumpWithHandleData);
  case CrashDetail::DumpType::Heap:
    return static_cast<MINIDUMP_TYPE>(always | MiniDumpWithPrivateReadWriteMemory |
                                      MiniDumpWithDataSegs | MiniDumpWithFullMemoryInfo |
                                      MiniDumpWithHandleData);
  case CrashDetail::DumpType::Small: break;
  }
  return static_cast<MINIDUMP_TYPE>(always | MiniDumpNormal);
}

void
setFilePath(WindowsState& state, const WIDECHAR* extension) noexcept
{
  state.filePath[0] = L'\0';
  appendWide(state.filePath, state.folder);
  appendWide(state.filePath, L"\\");
  appendUtf8(state.filePath, CrashDetail::getApplicationName());
  appendWide(state.filePath, extension);
}

void
writeMiniDump(CrashReportWriter& report, WindowsState& state) noexcept
{
  setFilePath(state, L".dmp");
  const HANDLE file = CreateFileW(state.filePath,
                                  GENERIC_WRITE,
                                  0,
                                  nullptr,
                                  CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL,
                                  nullptr);
  report.append("Memory dump: ");
  if (file == INVALID_HANDLE_VALUE) {
    report.append("could not be created, error ");
    report.appendNumber(GetLastError());
    report.append("\n");
    return;
  }

  MINIDUMP_EXCEPTION_INFORMATION exceptionInfo{};
  exceptionInfo.ThreadId = state.faultingThreadId;
  exceptionInfo.ExceptionPointers = state.exception;
  exceptionInfo.ClientPointers = FALSE;
  const BOOL bWritten = MiniDumpWriteDump(GetCurrentProcess(),
                                          GetCurrentProcessId(),
                                          file,
                                          getMiniDumpType(),
                                          &exceptionInfo,
                                          nullptr,
                                          nullptr);
  const DWORD error = GetLastError();
  CloseHandle(file);
  if (bWritten) {
    report.append(CrashDetail::getApplicationName());
    report.append(".dmp\n");
  }
  else {
    report.append("failed, error ");
    report.appendHex(error, 8);
    report.append("\n");
  }
}

// Kept apart because a function with __try can hold no object that has a destructor.
bool
runCallbacksGuarded(CrashReportWriter* report) noexcept
{
  __try {
    CrashDetail::runCallbacks(*report);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
  return true;
}

void
copyLog(CrashReportWriter& report, WindowsState& state, StringView reason) noexcept
{
  ANSICHAR line[CrashDetail::MAX_MESSAGE_CHARS];
  CrashReportWriter lineWriter(line, sizeof(line));
  lineWriter.append("The program crashed: ");
  lineWriter.append(reason);
  lineWriter.append(". Report in ");
  lineWriter.append(state.folderUtf8);
  const bool bFlushed = Logger::flushForCrash(lineWriter.getText());

  report.append("Log: ");
  if (!Logger::isStarted() || Logger::instance().getLogFilePath().empty()) {
    report.append("none\n");
    return;
  }

  WIDECHAR source[kMaxPathChars] = {};
  appendUtf8(source, Logger::instance().getLogFilePath());
  setFilePath(state, L".log");
  if (!CopyFileW(source, state.filePath, FALSE)) {
    report.append("could not be copied, error ");
    report.appendNumber(GetLastError());
    report.append("\n");
    return;
  }
  report.append(CrashDetail::getApplicationName());
  report.append(bFlushed ? ".log\n"
                         : ".log (it may miss the last lines: the Logger was busy)\n");
}

void
writeTextFile(WindowsState& state, StringView text) noexcept
{
  state.filePath[0] = L'\0';
  appendWide(state.filePath, state.folder);
  appendWide(state.filePath, L"\\CrashReport.txt");
  const HANDLE file = CreateFileW(state.filePath,
                                  GENERIC_WRITE,
                                  0,
                                  nullptr,
                                  CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL,
                                  nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return;
  }
  DWORD written = 0;
  WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
  CloseHandle(file);
}

void
makeReportFolder(WindowsState& state, const ANSICHAR* time) noexcept
{
  for (int32 attempt = 1; attempt < 10; ++attempt) {
    state.folder[0] = L'\0';
    appendWide(state.folder, state.reportDirectory);
    appendWide(state.folder, L"\\");
    appendUtf8(state.folder, CrashDetail::getApplicationName());
    appendWide(state.folder, L"-");
    appendUtf8(state.folder, time);
    if (attempt > 1) {
      const WIDECHAR suffix[] = {L'_', static_cast<WIDECHAR>(L'0' + attempt), L'\0'};
      appendWide(state.folder, suffix);
    }
    // Another run that crashed in the same second already has the name.
    if (GetFileAttributesW(state.folder) == INVALID_FILE_ATTRIBUTES) {
      break;
    }
  }
  createDirectories(state.folder);

  const int32 size = WideCharToMultiByte(CP_UTF8,
                                         0,
                                         state.folder,
                                         -1,
                                         state.folderUtf8,
                                         static_cast<int32>(sizeof(state.folderUtf8)),
                                         nullptr,
                                         nullptr);
  if (size <= 0) {
    state.folderUtf8[0] = '\0';
  }
}

void
writeCrashReport(WindowsState& state) noexcept
{
  SYSTEMTIME now{};
  GetLocalTime(&now);
  ANSICHAR time[20];
  CrashDetail::formatTime(time, now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute,
                          now.wSecond);
  makeReportFolder(state, time);

  CrashReportWriter report(state.report, sizeof(state.report));
  report.appendLine("Chimera crash report");
  report.append("Application: ");
  report.appendLine(CrashDetail::getApplicationName());
  CrashDetail::appendBuildInfo(report);
  report.append("Time: ");
  report.appendLine(time);

  const EXCEPTION_RECORD& record = *state.exception->ExceptionRecord;
  ANSICHAR reason[CrashDetail::MAX_MESSAGE_CHARS];
  CrashReportWriter reasonWriter(reason, sizeof(reason));
  appendExceptionReason(reasonWriter, record);
  report.append("Reason: ");
  report.appendLine(reasonWriter.getText());
  if (record.ExceptionCode == kFatalErrorCode && state.fatalMessage) {
    report.append("Message: ");
    report.appendLine(state.fatalMessage);
    reasonWriter.append(": ");
    reasonWriter.append(state.fatalMessage);
  }
  appendThreadName(report, state.faultingThreadId);
  appendCommandLine(report);

  writeMiniDump(report, state);
  copyLog(report, state, reasonWriter.getText());
  appendCallStack(report, state);

  report.append("\nFrom the program:\n");
  if (!runCallbacksGuarded(&report)) {
    report.append("(a crash callback crashed too)\n");
  }
  if (report.wasCut()) {
    static constexpr StringView kCut = "\n(cut: the report was too long)\n";
    std::memcpy(state.report + sizeof(state.report) - kCut.size(), kCut.data(), kCut.size());
  }
  writeTextFile(state, report.getText());
}

DWORD WINAPI
crashThreadMain(void* parameter)
{
  WindowsState& state = *static_cast<WindowsState*>(parameter);
  WaitForSingleObject(state.requestEvent, INFINITE);
  if (state.bStopping) {
    return 0;
  }
  writeCrashReport(state);
  SetEvent(state.doneEvent);
  return 0;
}

LONG WINAPI
onUnhandledException(EXCEPTION_POINTERS* info)
{
  WindowsState& state = getWindowsState();
  const DWORD code = info->ExceptionRecord->ExceptionCode;
  // The report itself crashed; nothing more can be written.
  if (GetCurrentThreadId() == state.crashThreadId) {
    TerminateProcess(GetCurrentProcess(), code);
  }
  if (!state.requestEvent || IsDebuggerPresent() || !CrashDetail::isEnabled()) {
    return state.previousFilter ? state.previousFilter(info) : EXCEPTION_CONTINUE_SEARCH;
  }
  // Only the first crash is reported; other threads wait for the process to end.
  if (InterlockedCompareExchange(&state.crashing, 1, 0) != 0) {
    Sleep(INFINITE);
  }

  state.exception = info;
  state.faultingThreadId = GetCurrentThreadId();
  SetEvent(state.requestEvent);
  WaitForSingleObject(state.doneEvent, kCrashWaitMilliseconds);
  TerminateProcess(GetCurrentProcess(), code);
  return EXCEPTION_EXECUTE_HANDLER;
}

void
onTerminate()
{
  WindowsState& state = getWindowsState();
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

void __cdecl
onPureCall()
{
  CrashDetail::platformReportFatal("A pure virtual function was called");
}

void __cdecl
onInvalidParameter(const WIDECHAR* expression,
                   const WIDECHAR* function,
                   const WIDECHAR* file,
                   uint32 line,
                   SIZE_T reserved)
{
  CH_PARAMETER_UNUSED(expression);
  CH_PARAMETER_UNUSED(function);
  CH_PARAMETER_UNUSED(file);
  CH_PARAMETER_UNUSED(line);
  CH_PARAMETER_UNUSED(reserved);
  CrashDetail::platformReportFatal("A C runtime function got an invalid parameter");
}

void __cdecl
onAbortSignal(int32 signal)
{
  CH_PARAMETER_UNUSED(signal);
  CrashDetail::platformReportFatal("abort() was called");
}
} // namespace

namespace CrashDetail {

/*
 */
bool
platformInstall()
{
  WindowsState& state = getWindowsState();
  state.requestEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  state.doneEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!state.requestEvent || !state.doneEvent) {
    return false;
  }
  // Made now, so a crash needs no new thread: a crashed process may not be able to start one.
  state.thread = CreateThread(nullptr, 0, &crashThreadMain, &state, 0, &state.crashThreadId);
  if (!state.thread) {
    return false;
  }

  const HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
  const FARPROC setDescription = kernel ? GetProcAddress(kernel, "SetThreadDescription")
                                        : nullptr;
  if (setDescription) {
    std::memcpy(&state.setThreadDescription, &setDescription, sizeof(setDescription));
  }

  state.previousFilter = SetUnhandledExceptionFilter(&onUnhandledException);
  state.previousTerminate = std::set_terminate(&onTerminate);
  state.previousPurecall = _set_purecall_handler(&onPureCall);
  state.previousInvalidParameter = _set_invalid_parameter_handler(&onInvalidParameter);
  std::signal(SIGABRT, &onAbortSignal);
  // abort() would otherwise show its own message box and call Windows Error Reporting.
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  return true;
}

/*
 */
void
platformUninstall()
{
  WindowsState& state = getWindowsState();
  SetUnhandledExceptionFilter(state.previousFilter);
  std::set_terminate(state.previousTerminate);
  _set_purecall_handler(state.previousPurecall);
  _set_invalid_parameter_handler(state.previousInvalidParameter);
  std::signal(SIGABRT, SIG_DFL);

  state.bStopping = true;
  SetEvent(state.requestEvent);
  WaitForSingleObject(state.thread, INFINITE);
  CloseHandle(state.thread);
  CloseHandle(state.requestEvent);
  CloseHandle(state.doneEvent);
  state.thread = nullptr;
  state.requestEvent = nullptr;
  state.doneEvent = nullptr;
  state.bStopping = false;
}

/*
 */
void
platformSetReportDirectory(const Path& directory)
{
  WindowsState& state = getWindowsState();
  state.reportDirectory[0] = L'\0';
  appendUtf8(state.reportDirectory, directory.toString());
  for (SIZE_T i = 0; state.reportDirectory[i] != L'\0'; ++i) {
    if (state.reportDirectory[i] == L'/') {
      state.reportDirectory[i] = L'\\';
    }
  }
  const SIZE_T length = getWideLength(state.reportDirectory);
  if (length > 0 && state.reportDirectory[length - 1] == L'\\') {
    state.reportDirectory[length - 1] = L'\0';
  }
}

/*
 */
uint64
platformGetCurrentThreadId() noexcept
{
  return GetCurrentThreadId();
}

/*
 */
void
platformPrepareThread(const ANSICHAR* name, int32 index)
{
  ULONG guarantee = kStackGuaranteeBytes;
  SetThreadStackGuarantee(&guarantee);
  // Older versions of the C runtime keep a terminate handler per thread.
  std::set_terminate(&onTerminate);

  WindowsState& state = getWindowsState();
  if (!state.setThreadDescription) {
    return;
  }
  WIDECHAR text[CrashDetail::MAX_NAME_CHARS] = {};
  SIZE_T length = 0;
  for (; name[length] != '\0' && length + 4 < CrashDetail::MAX_NAME_CHARS; ++length) {
    text[length] = static_cast<WIDECHAR>(name[length]);
  }
  if (index >= 0 && index < 100) {
    text[length++] = L' ';
    if (index >= 10) {
      text[length++] = static_cast<WIDECHAR>(L'0' + index / 10);
    }
    text[length++] = static_cast<WIDECHAR>(L'0' + index % 10);
  }
  text[length] = L'\0';
  state.setThreadDescription(GetCurrentThread(), text);
}

/*
 */
void
platformReportFatal(const ANSICHAR* message) noexcept
{
  WindowsState& state = getWindowsState();
  state.fatalMessage = message;
  // An exception lets a debugger stop here.
  if (IsDebuggerPresent()) {
    RaiseException(kFatalErrorCode, EXCEPTION_NONCONTINUABLE, 0, nullptr);
  }
  // Without one the report starts from here, not through an exception: the C runtime calls
  // terminate handlers inside a __try that would swallow it.
  else if (state.requestEvent && CrashDetail::isEnabled()) {
    CONTEXT context{};
    RtlCaptureContext(&context);
    EXCEPTION_RECORD record{};
    record.ExceptionCode = kFatalErrorCode;
    record.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
#if USING(CH_ARCHITECTURE_ARM64)
    record.ExceptionAddress = reinterpret_cast<void*>(context.Pc);
#else
    record.ExceptionAddress = reinterpret_cast<void*>(context.Rip);
#endif
    EXCEPTION_POINTERS pointers{&record, &context};
    onUnhandledException(&pointers);
  }
  __fastfail(FAST_FAIL_FATAL_APP_EXIT);
}

} // namespace CrashDetail
} // namespace chEngineSDK

#endif // USING(CH_PLATFORM_WIN32)
