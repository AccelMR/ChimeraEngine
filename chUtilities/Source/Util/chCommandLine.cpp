/************************************************************************/
/**
 * @file chCommandLine.cpp
 * @author AccelMR
 * @date 2022/08/27
 * @brief Read-only access to the program's command line arguments.
 */
/************************************************************************/

/************************************************************************/
/*
 * Includes
 */
/************************************************************************/
#include "chCommandLine.h"

#include <charconv>

#include "chAlgorithm.h"
#include "chConsoleVariable.h"
#include "chSTDThreading.h"
#include "chStringUtils.h"

#if USING(CH_PLATFORM_WIN32)
# include "chUnicode.h"
# include "chWindows.h"
# include <shellapi.h>
#endif

namespace chEngineSDK {

namespace {

struct CommandLineData
{
  UnorderedMap<String, String> values;
  UnorderedSet<String> flags;
  int32 argc = 0;
  const ANSICHAR* const* argv = nullptr;
#if USING(CH_PLATFORM_WIN32)
  // UTF-8 copies of the process arguments that getArgv points into.
  Vector<String> processArgs;
  Vector<const ANSICHAR*> processArgv;
#endif
};

NODISCARD CommandLineData&
getData() noexcept
{
  static CommandLineData data;
  return data;
}

/**
 * Created with new and never deleted: options are globals of other modules, some destroyed
 * after the statics of this file.
 */
struct OptionRegistry
{
  Mutex mutex;
  Vector<const CommandLineOption*> options;
};

NODISCARD OptionRegistry&
getOptionRegistry()
{
  static OptionRegistry* registry = new OptionRegistry();
  return *registry;
}

} // namespace

/*
 */
void
CommandLine::parse(int32 argc, const ANSICHAR* const* argv)
{
  CommandLineData& data = getData();
  data.values.clear();
  data.flags.clear();
  data.argc = argc;
  data.argv = argv;

  // argv[0] is the program path.
  for (int32 i = 1; i < argc; ++i) {
    StringView arg = argv[i];
    const SIZE_T nameStart = arg.find_first_not_of('-');
    if (0 == nameStart || StringView::npos == nameStart) {
      continue;
    }
    arg.remove_prefix(nameStart);

    const SIZE_T equals = arg.find('=');
    if (StringView::npos == equals) {
      data.flags.insert(StringUtils::toLower(String(arg)));
    }
    else if (equals > 0) {
      data.values[StringUtils::toLower(String(arg.substr(0, equals)))] =
          String(arg.substr(equals + 1));
    }
  }
}

/*
 */
void
CommandLine::initialize(int32 argc, const ANSICHAR* const* argv)
{
#if USING(CH_PLATFORM_WIN32)
  int32 wideCount = 0;
  LPWSTR* wideArgs = CommandLineToArgvW(GetCommandLineW(), &wideCount);
  if (wideArgs) {
    CommandLineData& data = getData();
    data.processArgs.clear();
    data.processArgs.reserve(static_cast<SIZE_T>(wideCount));
    for (int32 i = 0; i < wideCount; ++i) {
      data.processArgs.push_back(UTF8::fromWide(WString(wideArgs[i])));
    }
    LocalFree(wideArgs);

    data.processArgv.clear();
    data.processArgv.reserve(data.processArgs.size());
    for (const String& arg : data.processArgs) {
      data.processArgv.push_back(arg.c_str());
    }
    parse(wideCount, data.processArgv.data());
    return;
  }
#endif
  parse(argc, argv);
}

/*
 */
String
CommandLine::getValue(const String& key, const String& defaultValue)
{
  const CommandLineData& data = getData();
  const auto it = data.values.find(StringUtils::toLower(key));
  return data.values.end() != it ? it->second : defaultValue;
}

/*
 */
const String*
CommandLine::tryGetValue(const String& key)
{
  const CommandLineData& data = getData();
  const auto it = data.values.find(StringUtils::toLower(key));
  return data.values.end() != it ? &it->second : nullptr;
}

/*
 */
int32
CommandLine::getInt(const String& key, int32 defaultValue)
{
  const CommandLineData& data = getData();
  const auto it = data.values.find(StringUtils::toLower(key));
  if (data.values.end() == it) {
    return defaultValue;
  }

  const String& text = it->second;
  const ANSICHAR* const end = text.data() + text.size();
  int32 value = 0;
  const std::from_chars_result result = std::from_chars(text.data(), end, value);
  if (result.ec != std::errc() || result.ptr != end) {
    return defaultValue;
  }
  return value;
}

/*
 */
bool
CommandLine::hasFlag(const String& flag)
{
  const CommandLineData& data = getData();
  return data.flags.contains(StringUtils::toLower(flag));
}

/*
 */
int32
CommandLine::getArgc() noexcept
{
  return getData().argc;
}

/*
 */
const ANSICHAR* const*
CommandLine::getArgv() noexcept
{
  return getData().argv;
}

/*
 */
Vector<String>
CommandLine::getOptionNames()
{
  const CommandLineData& data = getData();
  Vector<String> names;
  names.reserve(data.values.size() + data.flags.size());
  for (const auto& [key, value] : data.values) {
    names.push_back(key);
  }
  for (const String& flag : data.flags) {
    names.push_back(flag);
  }
  Algorithm::sort(names);
  return names;
}

/*
 */
bool
CommandLine::isKnownOption(StringView name)
{
  if (ConsoleVariables::find(name)) {
    return true;
  }
  OptionRegistry& registry = getOptionRegistry();
  LockGuard<Mutex> lock(registry.mutex);
  return Algorithm::containsIf(registry.options, [name](const CommandLineOption* option) {
    return StringUtils::equalsIgnoreCase(option->getName(), name);
  });
}

/*
 */
CommandLineOption::CommandLineOption(StringView name, StringView help)
 : m_name(name),
   m_help(help)
{
  OptionRegistry& registry = getOptionRegistry();
  LockGuard<Mutex> lock(registry.mutex);
  registry.options.push_back(this);
}

/*
 */
CommandLineOption::~CommandLineOption()
{
  OptionRegistry& registry = getOptionRegistry();
  LockGuard<Mutex> lock(registry.mutex);
  Algorithm::removeFirst(registry.options, this);
}

/*
 */
Vector<const CommandLineOption*>
CommandLineOption::getAll()
{
  OptionRegistry& registry = getOptionRegistry();
  Vector<const CommandLineOption*> options;
  {
    LockGuard<Mutex> lock(registry.mutex);
    options = registry.options;
  }
  Algorithm::sort(options, [](const CommandLineOption* a, const CommandLineOption* b) {
    return a->getName() < b->getName();
  });
  return options;
}

} // namespace chEngineSDK
