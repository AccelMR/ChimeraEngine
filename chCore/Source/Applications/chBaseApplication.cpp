/************************************************************************/
/**
 * @file chBaseApplication.cpp
 * @author AccelMR
 * @date 2025/12/09
 *   BaseApplication class that handles the main loop, and the initialization of the engine.
 */
/************************************************************************/

/************************************************************************/
/*
 * Includes
 */
#include "chBaseApplication.h"

#include <chrono>
#include <ctime>

#include "chAlgorithm.h"
#include "chCommandLine.h"
#include "chConsoleVariable.h"
#include "chEngineConfig.h"
#include "chEnginePaths.h"
#include "chException.h"
#include "chFileSystem.h"
#include "chLogger.h"
#include "chPath.h"
#include "chPlatformSystem.h"
#include "chStringUtils.h"

namespace chEngineSDK {
using namespace std::chrono;
using std::stoi;

#if USING(CH_DEBUG_MODE)
#define CH_BASE_APPLICATION_LOG_LEVEL All
#else
#define CH_BASE_APPLICATION_LOG_LEVEL Info
#endif //USING(CH_DEBUG_MODE)


CH_LOG_DECLARE_STATIC(BaseApp, CH_BASE_APPLICATION_LOG_LEVEL);

namespace {
struct RelaunchRequest
{
  bool bRequested = false;
  Vector<String> arguments;
};

RelaunchRequest&
getRelaunchRequest()
{
  static RelaunchRequest request;
  return request;
}

ConsoleVariable<int32> g_cvarLogMaxBackups("Log.MaxBackups",
                                           20,
                                           "Logs of earlier runs kept per program in "
                                           "Saved/Logs; the oldest go first. 0 keeps all.");

constexpr const ANSICHAR* kBackupTag = "-backup-";
// "[YYYY-MM-DD HH:MM:SS", the start of every line the Logger writes.
constexpr SIZE_T kLineTimeLength = 20;

/*
 * Every log line starts with its time, so the first one tells when that run started, which
 * is what a backup is named after (the time of the copy would only say when the next run
 * began). Gives YYYY.MM.DD-HH.MM.SS, or the time now when the file does not start that way.
 */
String
getRunStartTime(const Path& logFile)
{
  ANSICHAR line[kLineTimeLength] = {};
  const SPtr<DataStream> stream = FileSystem::openFile(logFile);
  if (stream && stream->read(line, kLineTimeLength) == kLineTimeLength && line[0] == '[' &&
      line[5] == '-' && line[8] == '-' && line[11] == ' ' && line[14] == ':' &&
      line[17] == ':') {
    String result(line + 1, kLineTimeLength - 1);
    result[4] = '.';
    result[7] = '.';
    result[10] = '-';
    result[13] = '.';
    result[16] = '.';
    return result;
  }

  const std::time_t now = system_clock::to_time_t(system_clock::now());
  std::tm localTime{};
#if USING(CH_PLATFORM_WIN32)
  localtime_s(&localTime, &now);
#else
  localtime_r(&now, &localTime);
#endif // USING(CH_PLATFORM_WIN32)
  ANSICHAR buffer[32];
  const SIZE_T length = std::strftime(buffer, sizeof(buffer), "%Y.%m.%d-%H.%M.%S", &localTime);
  return String(buffer, length);
}

/*
 * Picks the file this run logs into and keeps the last run's log. The name stays fixed, so
 * the current log is always easy to find; a log already there is renamed to
 * <Name>-backup-<time it started>.log. A log another running instance still writes cannot be
 * renamed (Windows), so this run takes <Name>_2.log, _3... instead of emptying it.
 */
Path
prepareLogFile(const Path& directory, const String& fileName)
{
  const Path baseName(fileName);
  const String stem = baseName.getFileName(false);
  const String extension = baseName.getExtension();
  for (int32 instance = 1;; ++instance) {
    const String name =
        instance == 1 ? fileName : StringUtils::format("{0}_{1}{2}", stem, instance, extension);
    const Path logFile = directory.join(Path(name));
    if (!FileSystem::exists(logFile)) {
      return logFile;
    }

    const String backupStart = stem + kBackupTag + getRunStartTime(logFile);
    Path backup = directory.join(Path(backupStart + extension));
    // Two runs started within one second.
    for (int32 copy = 2; FileSystem::exists(backup); ++copy) {
      backup = directory.join(Path(StringUtils::format("{0}_{1}{2}", backupStart, copy,
                                                       extension)));
    }
    if (FileSystem::renameFile(logFile, backup)) {
      return logFile;
    }
    CH_LOG_INFO(BaseApp, "{0} is in use by another run; trying the next name.", name);
  }
}

/*
 * Deletes the oldest backups of this program past Log.MaxBackups. Their names hold the time
 * as text that sorts in time order, so a sort by name is a sort by age.
 */
void
removeOldLogBackups(const Path& directory, const String& fileName)
{
  const int32 maxBackups = g_cvarLogMaxBackups.get();
  if (maxBackups <= 0) {
    return;
  }

  const String prefix = Path(fileName).getFileName(false) + kBackupTag;
  Vector<Path> files;
  Vector<Path> directories;
  FileSystem::getChildren(directory, files, directories);
  Vector<String> backups;
  for (const Path& file : files) {
    String name = file.getFileName();
    if (name.starts_with(prefix)) {
      backups.push_back(std::move(name));
    }
  }
  if (backups.size() <= static_cast<SIZE_T>(maxBackups)) {
    return;
  }

  Algorithm::sort(backups);
  const SIZE_T removeCount = backups.size() - static_cast<SIZE_T>(maxBackups);
  for (SIZE_T i = 0; i < removeCount; ++i) {
    CH_PARAMETER_UNUSED(FileSystem::removeFile(directory.join(Path(backups[i]))));
  }
}
} // namespace

/*
*/
BaseApplication::BaseApplication() {
}

/*
*/
BaseApplication::~BaseApplication() {
  CH_LOG_INFO(BaseApp, "Destroying BaseApplication");
}

/*
*/
int32
BaseApplication::launch(int32 argc,
                        ANSICHAR* argv[],
                        const LaunchInfo& info,
                        void (*startUpApplication)())
{
  CommandLine::initialize(argc, argv);
  // Windowed programs start without a console; -log asks for one, before the first message.
  if (CommandLine::hasFlag("log")) {
    Logger::openConsoleWindow();
  }

  Logger::startUp();
  Logger& logger = Logger::instance();
  logger.setBufferingEnabled(info.logBufferSize > 0, info.logBufferSize);

  int32 exitCode = 0;
  try {
    // The log file lives in the project's Saved folder, so the project (-project) must
    // be known before the file is opened.
    EnginePaths::initialize(info.defaultProject, info.bProjectOptional);
    const Path& logDirectory = EnginePaths::getLogDirectory();
    const Path logFile = prepareLogFile(logDirectory, info.logFileName);
    logger.setFileOutput(true, logFile.toString());

    // After the paths, because the project decides which config files exist.
    EngineConfig::initialize();
    // After the config, which may change how many backups are kept.
    removeOldLogBackups(logDirectory, info.logFileName);

    startUpApplication();
    BaseApplication& app = instance();
    app.initialize();
    app.run();
    CH_LOG_INFO(BaseApp, "Application finished successfully.");
  } catch (const Exception& e) {
    CH_LOG_ERROR(BaseApp, "Exception caught: {0}", e.what());
    exitCode = 1;
  } catch (...) {
    CH_LOG_ERROR(BaseApp, "Unknown exception caught.");
    exitCode = 1;
  }

  // Also after a failed start up, so the modules that did start are shut down.
  try {
    if (isStarted()) {
      shutDown();
    }
  } catch (const Exception& e) {
    CH_LOG_ERROR(BaseApp, "Exception caught while shutting down: {0}", e.what());
    exitCode = 1;
  } catch (...) {
    CH_LOG_ERROR(BaseApp, "Unknown exception caught while shutting down.");
    exitCode = 1;
  }

  RelaunchRequest& relaunch = getRelaunchRequest();
  if (relaunch.bRequested) {
    CH_LOG_INFO(BaseApp, "Starting again: {0}", StringUtils::join(relaunch.arguments, " "));
  }
  Logger::shutDown();

  // After the Logger, so the log file is closed before the new run opens it.
  if (relaunch.bRequested) {
    relaunch.arguments.insert(relaunch.arguments.begin(),
                              FileSystem::getExecutablePath().toString());
    if (!PlatformSystem::launchProcess(relaunch.arguments)) {
      exitCode = 1;
    }
  }
  return exitCode;
}

/*
 */
void
BaseApplication::requestRelaunch(Vector<String> arguments)
{
  RelaunchRequest& request = getRelaunchRequest();
  request.bRequested = true;
  request.arguments = std::move(arguments);
}

/*
*/
void
BaseApplication::onShutDown()
{
  // Module::shutDown calls this before the delete, while the object still has its full
  // type. Called from the destructor, destroyModules would not reach the derived classes.
  destroyModules();
}

/*
*/
void
BaseApplication::initialize() {
  initializeModules();
  onPostInitialize();

  CH_LOG_INFO(BaseApp, "BaseApplication initialized successfully.");
}

/*
 */
void
BaseApplication::run() {
  const double fixedTimeStamp = 1.0 / 60.0;
  double accumulator = 0.0;
  auto previousTime = high_resolution_clock::now();

  while (m_running) {
    //Calculate delta time
    auto currentTime = high_resolution_clock::now();
    auto deltaTime = duration_cast<duration<double>>(currentTime - previousTime).count();
    previousTime = currentTime;

    if (!m_running) { break; }

    // Update the accumulator
    accumulator += deltaTime;
    while (accumulator >= fixedTimeStamp) {
      fixedUpdate(static_cast<float>(fixedTimeStamp));
      accumulator -= fixedTimeStamp;
    }
    update(static_cast<float>(deltaTime));
  }
}

/*
*/
void
BaseApplication::requestExit(const String& reason) {
  CH_LOG_INFO(BaseApp, "Requesting exit: {0}", reason);
  m_running = false;
}

} // namespace chEngineSDK
