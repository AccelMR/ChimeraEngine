/************************************************************************/
/**
 * @file chCommandLine.h
 * @author AccelMR
 * @date 2022/08/27
 * @brief Read-only access to the program's command line arguments.
 */
/************************************************************************/
#pragma once

/************************************************************************/
/*
 * Includes
 */
/************************************************************************/
#include "chPrerequisitesUtilities.h"

namespace chEngineSDK {

/**
 * Holds the command line options, so any system can read them without them being passed
 * around. It is set once in main, before other threads start, and is read-only after
 * that, so it needs no locks and no start up or shut down.
 *
 * Options are "-Key=Value" or "-Flag" (any number of leading '-'). Keys and flags ignore
 * case; values keep it. Arguments without a leading '-' are ignored.
 *
 * Sample usage:
 * chEditor -GraphicsAPI=chVulkan -Width=1280 -NoSplash
 * CommandLine::getValue("graphicsapi", "chVulkan");
 * CommandLine::getInt("Width", 1920);
 * CommandLine::hasFlag("nosplash");
 */
class CH_UTILITY_EXPORT CommandLine
{
 public:
  /**
   * Reads the arguments of the running process, given main's argc and argv. On Windows
   * argv is in the system code page, so the arguments are read again as UTF-16 and kept
   * as UTF-8.
   */
  static void
  initialize(int32 argc, const ANSICHAR* const* argv);

  /**
   * Replaces any options read before with these UTF-8 arguments. argv is kept for
   * getArgv, so it must live as long as the program, as main's argv does.
   */
  static void
  parse(int32 argc, const ANSICHAR* const* argv);

  NODISCARD static String
  getValue(const String& key, const String& defaultValue = "");

  /**
   * Returns nullptr when the option is missing, so "-Key=" (an empty value) can be told
   * apart from no option. The pointer stays valid until the next initialize or parse.
   */
  NODISCARD static const String*
  tryGetValue(const String& key);

  /**
   * Returns defaultValue when the option is missing or its value is not a whole number.
   */
  NODISCARD static int32
  getInt(const String& key, int32 defaultValue = 0);

  NODISCARD static bool
  hasFlag(const String& flag);

  NODISCARD static int32
  getArgc() noexcept;

  NODISCARD static const ANSICHAR* const*
  getArgv() noexcept;

  /**
   * Every option given, as written without the leading '-' and "=value", sorted.
   */
  NODISCARD static Vector<String>
  getOptionNames();

  /**
   * True when name is a console variable (by name or alias) or a CommandLineOption.
   */
  NODISCARD static bool
  isKnownOption(StringView name);
};

/**
 * Exists so options that are not console variables (-project, -log) are listed by -help and
 * not reported as typos. Declared as a global next to the code that reads the option.
 */
class CH_UTILITY_EXPORT CommandLineOption
{
 public:
  CommandLineOption(StringView name, StringView help);

  ~CommandLineOption();

  CommandLineOption(const CommandLineOption&) = delete;

  CommandLineOption&
  operator=(const CommandLineOption&) = delete;

  NODISCARD FORCEINLINE const String&
  getName() const noexcept
  {
    return m_name;
  }

  NODISCARD FORCEINLINE const String&
  getHelp() const noexcept
  {
    return m_help;
  }

  /**
   * Sorted by name.
   */
  NODISCARD static Vector<const CommandLineOption*>
  getAll();

 private:
  String m_name;
  String m_help;
};

} // namespace chEngineSDK
