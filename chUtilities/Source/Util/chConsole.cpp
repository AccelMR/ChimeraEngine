/************************************************************************/
/**
 * @file chConsole.cpp
 * @author AccelMR
 * @date 2026/11/23
 * @brief Commands and console variables typed at run time.
 */
/************************************************************************/

/************************************************************************/
/*
 * Includes
 */
/************************************************************************/
#include "chConsole.h"

#if USING(CH_CONSOLE)

#include "chAlgorithm.h"
#include "chConsoleVariable.h"
#include "chStringUtils.h"

CH_LOG_DEFINE_CATEGORY_SHARED(ConsoleLog, All);

namespace chEngineSDK {

namespace {

// More results than a person reads at once; find says how many were left out.
constexpr SIZE_T kMaxFindResults = 200;

/**
 * Created with new and never deleted: commands are globals of other modules and plugins,
 * some destroyed after the statics of this file. Changed and read on the main thread only.
 */
struct ConsoleData
{
  Vector<const ConsoleCommand*> commands;
  Vector<String> history;
};

NODISCARD ConsoleData&
getData()
{
  static ConsoleData* data = new ConsoleData();
  return *data;
}

NODISCARD bool
startsWithIgnoreCase(StringView text, StringView prefix) noexcept
{
  return text.size() >= prefix.size() &&
         StringUtils::equalsIgnoreCase(text.substr(0, prefix.size()), prefix);
}

/**
 * Splits text into words by spaces; "quoted text" is one word without its quotes. Stops at
 * the size of outWords. Gives how many it wrote.
 */
SIZE_T
splitWords(StringView text, Span<StringView> outWords) noexcept
{
  SIZE_T count = 0;
  SIZE_T position = 0;
  while (position < text.size() && count < outWords.size()) {
    if (text[position] == ' ' || text[position] == '\t') {
      ++position;
      continue;
    }
    if (text[position] == '"') {
      const SIZE_T start = position + 1;
      SIZE_T end = text.find('"', start);
      if (end == StringView::npos) {
        end = text.size();
      }
      outWords[count++] = text.substr(start, end - start);
      position = end + 1;
      continue;
    }
    SIZE_T end = text.find_first_of(" \t", position);
    if (end == StringView::npos) {
      end = text.size();
    }
    outWords[count++] = text.substr(position, end - position);
    position = end;
  }
  return count;
}

NODISCARD StringView
getSourceName(const ConsoleVariableSource source) noexcept
{
  switch (source) {
    case ConsoleVariableSource::Default:
      return "default";
    case ConsoleVariableSource::EngineConfig:
      return "engine config";
    case ConsoleVariableSource::ProjectConfig:
      return "project config";
    case ConsoleVariableSource::UserConfig:
      return "user config";
    case ConsoleVariableSource::CommandLine:
      return "command line";
    case ConsoleVariableSource::Code:
      return "code";
    case ConsoleVariableSource::Console:
      return "console";
  }
  return "unknown";
}

void
addToHistory(StringView line)
{
  Vector<String>& history = getData().history;
  if (!history.empty() && StringView(history.back()) == line) {
    return;
  }
  if (history.size() >= Console::MAX_HISTORY) {
    history.erase(history.begin());
  }
  history.emplace_back(line);
}

void
setVariable(IConsoleVariable& variable, StringView valueText)
{
  // A value written whole in quotes keeps its spaces without them.
  if (valueText.size() >= 2 && valueText.front() == '"' && valueText.back() == '"') {
    valueText = valueText.substr(1, valueText.size() - 2);
  }
  // setFromString logs why a value does not fit.
  if (!variable.setFromString(valueText, ConsoleVariableSource::Console)) {
    return;
  }
  if (variable.requiresRestart()) {
    CH_LOG_INFO(ConsoleLog, "{0} = {1} (applies the next time the engine starts)",
                variable.getName(), variable.toString());
  }
  else {
    CH_LOG_INFO(ConsoleLog, "{0} = {1}", variable.getName(), variable.toString());
  }
}

void
runHelp(const ConsoleArguments& arguments)
{
  const StringView name = arguments.get(0);
  if (!name.empty()) {
    if (const ConsoleCommand* command = Console::findCommand(name)) {
      CH_LOG_INFO(ConsoleLog, "{0} {1}: {2}", command->getName(), command->getUsage(),
                  command->getHelp());
      return;
    }
    if (const IConsoleVariable* variable = ConsoleVariables::find(name)) {
      Console::describeVariable(*variable);
      return;
    }
    CH_LOG_WARNING(ConsoleLog, "Nothing is called {0}. Try find {0}.", name);
    return;
  }

  CH_LOG_INFO(ConsoleLog, "Commands:");
  for (const ConsoleCommand* command : Console::getCommands()) {
    CH_LOG_INFO(ConsoleLog, "  {0} {1}: {2}", command->getName(), command->getUsage(),
                command->getHelp());
  }
  CH_LOG_INFO(ConsoleLog,
              "Settings: type a name to see it, a name and a value to set it, or find <text> "
              "to search them.");
}

void
runFind(const ConsoleArguments& arguments)
{
  const StringView text = arguments.text;
  if (text.empty()) {
    CH_LOG_WARNING(ConsoleLog, "find needs a text to search for.");
    return;
  }
  SIZE_T found = 0;
  for (const ConsoleCommand* command : Console::getCommands()) {
    if (StringUtils::containsIgnoreCase(command->getName(), text) ||
        StringUtils::containsIgnoreCase(command->getHelp(), text)) {
      if (found++ < kMaxFindResults) {
        CH_LOG_INFO(ConsoleLog, "  {0} {1}: {2}", command->getName(), command->getUsage(),
                    command->getHelp());
      }
    }
  }
  for (const IConsoleVariable* variable : ConsoleVariables::getAll()) {
    if (StringUtils::containsIgnoreCase(variable->getName(), text) ||
        StringUtils::containsIgnoreCase(variable->getCommandLineAlias(), text) ||
        StringUtils::containsIgnoreCase(variable->getHelp(), text)) {
      if (found++ < kMaxFindResults) {
        CH_LOG_INFO(ConsoleLog, "  {0} = {1}: {2}", variable->getName(),
                    variable->toString(), variable->getHelp());
      }
    }
  }
  if (found > kMaxFindResults) {
    CH_LOG_INFO(ConsoleLog, "{0} found, {1} shown; search for something longer.", found,
                kMaxFindResults);
  }
  else if (found == 0) {
    CH_LOG_INFO(ConsoleLog, "Nothing matches {0}.", text);
  }
}

ConsoleCommand g_commandHelp("help",
                             "[name]",
                             "Lists the commands, or tells what a command or setting does.",
                             &runHelp);
ConsoleCommand g_commandFind("find",
                             "<text>",
                             "Lists the commands and settings whose name or help has text.",
                             &runFind);

} // namespace

/*
 */
ConsoleCommand::ConsoleCommand(StringView name,
                               StringView usage,
                               StringView help,
                               ConsoleCommandFunction function)
 : m_name(name),
   m_usage(usage),
   m_help(help),
   m_function(std::move(function))
{
  ConsoleData& data = getData();
  if (Console::findCommand(m_name)) {
    CH_LOG_WARNING(ConsoleLog, "The command {0} exists twice; the first one runs.", m_name);
  }
  data.commands.push_back(this);
}

/*
 */
ConsoleCommand::~ConsoleCommand()
{
  Algorithm::removeFirst(getData().commands, this);
}

/*
 */
void
ConsoleCommand::execute(const ConsoleArguments& arguments) const
{
  if (m_function) {
    m_function(arguments);
  }
}

/*
 */
bool
Console::execute(StringView line)
{
  line = StringUtils::trimView(line);
  if (line.empty()) {
    return false;
  }
  addToHistory(line);
  CH_LOG_INFO(ConsoleLog, "> {0}", line);

  SIZE_T nameEnd = line.find_first_of(" \t");
  if (nameEnd == StringView::npos) {
    nameEnd = line.size();
  }
  const StringView name = line.substr(0, nameEnd);
  const StringView rest = StringUtils::trimView(line.substr(nameEnd));

  if (const ConsoleCommand* command = findCommand(name)) {
    StringView words[MAX_WORDS];
    const SIZE_T count = splitWords(rest, words);
    command->execute({.text = rest, .words = Span<const StringView>(words, count)});
    return true;
  }

  if (IConsoleVariable* variable = ConsoleVariables::find(name)) {
    if (rest.empty()) {
      describeVariable(*variable);
    }
    else {
      setVariable(*variable, rest);
    }
    return true;
  }

  CH_LOG_WARNING(ConsoleLog, "Nothing is called {0}. Type help, or find <text>.", name);
  return false;
}

/*
 */
void
Console::complete(StringView prefix, Vector<String>& outNames)
{
  outNames.clear();
  for (const ConsoleCommand* command : getData().commands) {
    if (startsWithIgnoreCase(command->getName(), prefix)) {
      outNames.push_back(command->getName());
    }
  }
  for (const IConsoleVariable* variable : ConsoleVariables::getAll()) {
    if (startsWithIgnoreCase(variable->getName(), prefix)) {
      outNames.push_back(variable->getName());
    }
  }
  Algorithm::sort(outNames);
}

/*
 */
const Vector<String>&
Console::getHistory()
{
  return getData().history;
}

/*
 */
const ConsoleCommand*
Console::findCommand(StringView name)
{
  for (const ConsoleCommand* command : getData().commands) {
    if (StringUtils::equalsIgnoreCase(command->getName(), name)) {
      return command;
    }
  }
  return nullptr;
}

/*
 */
Vector<const ConsoleCommand*>
Console::getCommands()
{
  Vector<const ConsoleCommand*> commands = getData().commands;
  Algorithm::sort(commands, [](const ConsoleCommand* a, const ConsoleCommand* b) {
    return a->getName() < b->getName();
  });
  return commands;
}

/*
 */
void
Console::describeVariable(const IConsoleVariable& variable)
{
  CH_LOG_INFO(ConsoleLog, "{0} = {1} ({2}, set by {3}{4}): {5}", variable.getName(),
              variable.toString(), variable.getTypeName(),
              getSourceName(variable.getSource()),
              variable.requiresRestart() ? ", applies at the next start" : "",
              variable.getHelp());
}

} // namespace chEngineSDK

#endif // USING(CH_CONSOLE)
