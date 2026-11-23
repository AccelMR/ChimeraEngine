/************************************************************************/
/**
 * @file chConsoleVariable.cpp
 * @author AccelMR
 * @date 2026/10/06
 * @brief Named settings that config files and the command line can change.
 */
/************************************************************************/

/************************************************************************/
/*
 * Includes
 */
/************************************************************************/
#include "chConsoleVariable.h"

#include <charconv>

#include "chAlgorithm.h"
#include "chCommandLine.h"
#include "chLogger.h"
#include "chSTDThreading.h"
#include "chStringUtils.h"

namespace chEngineSDK {

CH_LOG_DECLARE_STATIC(ConsoleVariableLog, All);

namespace {

struct StartupValue
{
  String name;
  String value;
  ConsoleVariableSource source;
};

/**
 * Created with new and never deleted: variables are statics of other modules and plugins,
 * and some are destroyed after the statics of this file, so the list must still be there
 * when they remove themselves.
 */
struct VariableRegistry
{
  Mutex mutex;
  Vector<IConsoleVariable*> variables;
  Vector<StartupValue> startupValues;
  bool startupApplied = false;
};

NODISCARD VariableRegistry&
getRegistry()
{
  static VariableRegistry* registry = new VariableRegistry();
  return *registry;
}

/**
 * Created with new and never deleted, for the same reason as the variable registry.
 */
struct DataKeyRegistry
{
  Mutex mutex;
  Vector<const ConfigDataKey*> keys;
};

NODISCARD DataKeyRegistry&
getDataKeyRegistry()
{
  static DataKeyRegistry* registry = new DataKeyRegistry();
  return *registry;
}

NODISCARD bool
matchesName(const IConsoleVariable& variable, StringView name) noexcept
{
  return StringUtils::equalsIgnoreCase(variable.getName(), name) ||
         (!variable.getCommandLineAlias().empty() &&
          StringUtils::equalsIgnoreCase(variable.getCommandLineAlias(), name));
}

/**
 * The full name wins over the alias when both are given.
 */
void
applyCommandLine(IConsoleVariable& variable)
{
  const String& alias = variable.getCommandLineAlias();
  const String* value = CommandLine::tryGetValue(variable.getName());
  if (!value && !alias.empty()) {
    value = CommandLine::tryGetValue(alias);
  }

  if (value) {
    variable.setFromString(*value, ConsoleVariableSource::CommandLine);
  }
  else if (CommandLine::hasFlag(variable.getName()) ||
           (!alias.empty() && CommandLine::hasFlag(alias))) {
    variable.setFromString("true", ConsoleVariableSource::CommandLine);
  }
}

/**
 * The registry mutex must be held.
 */
void
applyToVariable(const VariableRegistry& registry, IConsoleVariable& variable)
{
  for (const StartupValue& startupValue : registry.startupValues) {
    if (StringUtils::equalsIgnoreCase(startupValue.name, variable.getName())) {
      variable.setFromString(startupValue.value, startupValue.source);
    }
  }
  applyCommandLine(variable);
}

} // namespace

/*
 */
IConsoleVariable::IConsoleVariable(StringView name,
                                   StringView help,
                                   StringView commandLineAlias,
                                   const ConsoleVariableBehaviorFlags behavior)
 : m_name(name),
   m_help(help),
   m_commandLineAlias(commandLineAlias),
   m_behavior(behavior)
{}

/*
 */
IConsoleVariable::~IConsoleVariable()
{
  ConsoleVariables::remove(*this);
}

/*
 */
bool
IConsoleVariable::setFromString(StringView text, ConsoleVariableSource source)
{
  if (!canBeSetBy(source)) {
    return false;
  }
  if (!parse(StringUtils::trimView(text))) {
    CH_LOG_WARNING(ConsoleVariableLog, "'{0}' is not a valid value for {1}; it stays {2}.",
                   text, m_name, toString());
    return false;
  }
  m_source = source;
  return true;
}

/*
 */
void
IConsoleVariable::registerVariable()
{
  ConsoleVariables::add(*this);
}

/*
 */
bool
IConsoleVariable::parseValue(StringView text, bool& value)
{
  for (const StringView word : {"true", "1", "yes", "on"}) {
    if (StringUtils::equalsIgnoreCase(text, word)) {
      value = true;
      return true;
    }
  }
  for (const StringView word : {"false", "0", "no", "off"}) {
    if (StringUtils::equalsIgnoreCase(text, word)) {
      value = false;
      return true;
    }
  }
  return false;
}

/*
 */
bool
IConsoleVariable::parseValue(StringView text, int32& value)
{
  const ANSICHAR* const end = text.data() + text.size();
  const std::from_chars_result result = std::from_chars(text.data(), end, value);
  return !text.empty() && result.ec == std::errc() && result.ptr == end;
}

/*
 */
bool
IConsoleVariable::parseValue(StringView text, float& value)
{
  const ANSICHAR* const end = text.data() + text.size();
  const std::from_chars_result result = std::from_chars(text.data(), end, value);
  return !text.empty() && result.ec == std::errc() && result.ptr == end;
}

/*
 */
bool
IConsoleVariable::parseValue(StringView text, String& value)
{
  value = text;
  return true;
}

/*
 */
String
IConsoleVariable::valueToString(bool value)
{
  return value ? "true" : "false";
}

/*
 */
String
IConsoleVariable::valueToString(int32 value)
{
  ANSICHAR buffer[StringUtils::MAX_INTEGER_CHARS];
  return String(StringUtils::toChars(buffer, value));
}

/*
 */
String
IConsoleVariable::valueToString(float value)
{
  ANSICHAR buffer[StringUtils::MAX_FLOAT_CHARS];
  return String(StringUtils::toChars(buffer, value));
}

/*
 */
String
IConsoleVariable::valueToString(const String& value)
{
  return value;
}

/*
 */
IConsoleVariable*
ConsoleVariables::find(StringView name)
{
  VariableRegistry& registry = getRegistry();
  LockGuard<Mutex> lock(registry.mutex);
  for (IConsoleVariable* variable : registry.variables) {
    if (matchesName(*variable, name)) {
      return variable;
    }
  }
  return nullptr;
}

/*
 */
Vector<IConsoleVariable*>
ConsoleVariables::getAll()
{
  VariableRegistry& registry = getRegistry();
  Vector<IConsoleVariable*> variables;
  {
    LockGuard<Mutex> lock(registry.mutex);
    variables = registry.variables;
  }
  Algorithm::sort(variables, [](const IConsoleVariable* a, const IConsoleVariable* b) {
    return a->getName() < b->getName();
  });
  return variables;
}

/*
 */
void
ConsoleVariables::setStartupValue(StringView name,
                                  StringView value,
                                  ConsoleVariableSource source)
{
  VariableRegistry& registry = getRegistry();
  LockGuard<Mutex> lock(registry.mutex);

  bool kept = false;
  for (StartupValue& startupValue : registry.startupValues) {
    if (StringUtils::equalsIgnoreCase(startupValue.name, name)) {
      if (source < startupValue.source) {
        return;
      }
      startupValue.value = value;
      startupValue.source = source;
      kept = true;
      break;
    }
  }
  if (!kept) {
    registry.startupValues.push_back(
        {.name = String(name), .value = String(value), .source = source});
  }

  if (registry.startupApplied) {
    for (IConsoleVariable* variable : registry.variables) {
      if (StringUtils::equalsIgnoreCase(variable->getName(), name)) {
        variable->setFromString(value, source);
      }
    }
  }
}

/*
 */
void
ConsoleVariables::applyStartupValues()
{
  VariableRegistry& registry = getRegistry();
  LockGuard<Mutex> lock(registry.mutex);
  registry.startupApplied = true;
  for (IConsoleVariable* variable : registry.variables) {
    applyToVariable(registry, *variable);
  }
}

/*
 */
bool
ConsoleVariables::isKnownConfigKey(StringView name)
{
  // Config files name a variable by its full name only; aliases are for the command line.
  {
    VariableRegistry& registry = getRegistry();
    LockGuard<Mutex> lock(registry.mutex);
    for (const IConsoleVariable* variable : registry.variables) {
      if (StringUtils::equalsIgnoreCase(variable->getName(), name)) {
        return true;
      }
    }
  }
  DataKeyRegistry& keys = getDataKeyRegistry();
  LockGuard<Mutex> lock(keys.mutex);
  for (const ConfigDataKey* key : keys.keys) {
    if (key->matches(name)) {
      return true;
    }
  }
  return false;
}

/*
 */
void
ConsoleVariables::logHelp()
{
  CH_LOG_INFO(ConsoleVariableLog,
              "Settings (Section.Key in Engine.ini, Game.ini or Editor.ini; "
              "-Section.Key=Value or the alias on the command line):");
  for (const IConsoleVariable* variable : getAll()) {
    const String& alias = variable->getCommandLineAlias();
    CH_LOG_INFO(ConsoleVariableLog, "  {0}{1}{2} = {3} ({4}{5}): {6}", variable->getName(),
                alias.empty() ? "" : ", -", alias, variable->toString(),
                variable->getTypeName(),
                variable->requiresRestart() ? ", read at start" : "", variable->getHelp());
  }

  const Vector<const ConfigDataKey*> keys = ConfigDataKey::getAll();
  if (!keys.empty()) {
    CH_LOG_INFO(ConsoleVariableLog, "Config values that are not settings:");
    for (const ConfigDataKey* key : keys) {
      CH_LOG_INFO(ConsoleVariableLog, "  {0}: {1}", key->getName(), key->getHelp());
    }
  }

  CH_LOG_INFO(ConsoleVariableLog, "Command line options:");
  for (const CommandLineOption* option : CommandLineOption::getAll()) {
    CH_LOG_INFO(ConsoleVariableLog, "  -{0}: {1}", option->getName(), option->getHelp());
  }
}

/*
 */
void
ConsoleVariables::add(IConsoleVariable& variable)
{
  VariableRegistry& registry = getRegistry();
  LockGuard<Mutex> lock(registry.mutex);
  for (const IConsoleVariable* other : registry.variables) {
    if (StringUtils::equalsIgnoreCase(other->getName(), variable.getName())) {
      CH_LOG_WARNING(ConsoleVariableLog,
                     "{0} is registered twice; find returns the first one.",
                     variable.getName());
      break;
    }
  }
  registry.variables.push_back(&variable);

  if (registry.startupApplied) {
    applyToVariable(registry, variable);
  }
}

/*
 */
void
ConsoleVariables::remove(IConsoleVariable& variable)
{
  VariableRegistry& registry = getRegistry();
  LockGuard<Mutex> lock(registry.mutex);
  Algorithm::removeFirst(registry.variables, &variable);
}

/*
 */
ConfigDataKey::ConfigDataKey(StringView name, StringView help)
 : m_name(name),
   m_help(help)
{
  DataKeyRegistry& registry = getDataKeyRegistry();
  LockGuard<Mutex> lock(registry.mutex);
  registry.keys.push_back(this);
}

/*
 */
ConfigDataKey::~ConfigDataKey()
{
  DataKeyRegistry& registry = getDataKeyRegistry();
  LockGuard<Mutex> lock(registry.mutex);
  Algorithm::removeFirst(registry.keys, this);
}

/*
 */
bool
ConfigDataKey::matches(StringView name) const noexcept
{
  const StringView own = m_name;
  if (own.size() >= 2 && own.ends_with(".*")) {
    // The section with its dot, so "Package.*" never takes "Packager.X".
    const StringView section = own.substr(0, own.size() - 1);
    return name.size() > section.size() &&
           StringUtils::equalsIgnoreCase(name.substr(0, section.size()), section);
  }
  return StringUtils::equalsIgnoreCase(own, name);
}

/*
 */
Vector<const ConfigDataKey*>
ConfigDataKey::getAll()
{
  DataKeyRegistry& registry = getDataKeyRegistry();
  Vector<const ConfigDataKey*> keys;
  {
    LockGuard<Mutex> lock(registry.mutex);
    keys = registry.keys;
  }
  Algorithm::sort(keys, [](const ConfigDataKey* a, const ConfigDataKey* b) {
    return a->getName() < b->getName();
  });
  return keys;
}

} // namespace chEngineSDK
