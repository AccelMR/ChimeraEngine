/************************************************************************/
/**
 * @file chConsole.h
 * @author AccelMR
 * @date 2026/11/23
 * @brief Commands and console variables typed at run time.
 */
/************************************************************************/
#pragma once

/************************************************************************/
/*
 * Includes
 */
/************************************************************************/
#include "chPrerequisitesUtilities.h"

#if USING(CH_CONSOLE)

#include "chLogger.h"

CH_LOG_DECLARE_EXTERN(CH_UTILITY_EXPORT, ConsoleLog);

namespace chEngineSDK {

class IConsoleVariable;

/**
 * What a command gets after its name: the whole text, and the words split by spaces, where
 * a "quoted text" is one word without its quotes. Views into the typed line, valid during
 * the call only.
 */
struct ConsoleArguments
{
  StringView text{};
  Span<const StringView> words{};

  NODISCARD FORCEINLINE SIZE_T
  getCount() const noexcept
  {
    return words.size();
  }

  /**
   * Empty past the last word.
   */
  NODISCARD FORCEINLINE StringView
  get(SIZE_T index) const noexcept
  {
    return index < words.size() ? words[index] : StringView();
  }
};

using ConsoleCommandFunction = Function<void(const ConsoleArguments&)>;

/**
 * Exists so any module or plugin adds a command without touching the console: declared as
 * a global where its code lives, like a ConsoleVariable. Names ignore case.
 */
class CH_UTILITY_EXPORT ConsoleCommand
{
 public:
  /**
   * usage lists the arguments, "<map> [onDone]", empty without any.
   */
  ConsoleCommand(StringView name,
                 StringView usage,
                 StringView help,
                 ConsoleCommandFunction function);

  ~ConsoleCommand();

  ConsoleCommand(const ConsoleCommand&) = delete;

  ConsoleCommand&
  operator=(const ConsoleCommand&) = delete;

  void
  execute(const ConsoleArguments& arguments) const;

  NODISCARD FORCEINLINE const String&
  getName() const noexcept
  {
    return m_name;
  }

  NODISCARD FORCEINLINE const String&
  getUsage() const noexcept
  {
    return m_usage;
  }

  NODISCARD FORCEINLINE const String&
  getHelp() const noexcept
  {
    return m_help;
  }

 private:
  String m_name;
  String m_usage;
  String m_help;
  ConsoleCommandFunction m_function;
};

/**
 * Runs lines typed by a person: "command arguments", or a console variable name to see it,
 * or a name and a value to set it (with ConsoleVariableSource::Console). Every result goes
 * to the log under ConsoleLog. Main thread only.
 */
class CH_UTILITY_EXPORT Console
{
 public:
  /**
   * False when the line was empty or named nothing. A line is kept in the history even when
   * it fails, so a typo can be fixed.
   */
  static bool
  execute(StringView line);

  /**
   * Names of commands and variables starting with prefix, ignoring case, sorted.
   */
  static void
  complete(StringView prefix, Vector<String>& outNames);

  /**
   * Oldest first, at most MAX_HISTORY lines, no line twice in a row.
   */
  NODISCARD static const Vector<String>&
  getHistory();

  NODISCARD static const ConsoleCommand*
  findCommand(StringView name);

  /**
   * Sorted by name.
   */
  NODISCARD static Vector<const ConsoleCommand*>
  getCommands();

  /**
   * Logs a variable's value, type, source and help.
   */
  static void
  describeVariable(const IConsoleVariable& variable);

  static constexpr SIZE_T MAX_HISTORY = 64;
  static constexpr SIZE_T MAX_WORDS = 32;
};

} // namespace chEngineSDK

#endif // USING(CH_CONSOLE)
