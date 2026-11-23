/************************************************************************/
/**
 * @file chConfigTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of the command line, config files and console variables.
 */
/************************************************************************/
#include "chAlgorithm.h"
#include "chCommandLine.h"
#include "chConsole.h"
#include "chConfigFile.h"
#include "chConsoleVariable.h"
#include "chFileSystem.h"
#include "chPath.h"

#include "catch.hpp"

using namespace chEngineSDK;

TEST_CASE("chUtilities - CommandLine", "[Config]") {
  SECTION("values and flags") {
    const ANSICHAR* argv[] = {"program", "-option1=value1", "-Option2=Value=2", "-flag"};
    CommandLine::parse(4, argv);

    REQUIRE(CommandLine::getValue("option1") == "value1");
    REQUIRE(CommandLine::getValue("OPTION2") == "Value=2");
    REQUIRE(CommandLine::hasFlag("FLAG"));
    REQUIRE_FALSE(CommandLine::hasFlag("option1"));
    REQUIRE(CommandLine::getValue("missing", "default") == "default");
    REQUIRE_FALSE(CommandLine::hasFlag("missing"));
    REQUIRE(CommandLine::getArgc() == 4);
  }

  SECTION("dashes and plain arguments") {
    const ANSICHAR* argv[] = {"program", "--double", "plain", "-", "--", "-=x", "-empty="};
    CommandLine::parse(7, argv);

    REQUIRE(CommandLine::hasFlag("double"));
    REQUIRE_FALSE(CommandLine::hasFlag("plain"));
    REQUIRE_FALSE(CommandLine::hasFlag("lain"));
    REQUIRE_FALSE(CommandLine::hasFlag(""));
    REQUIRE(CommandLine::getValue("", "none") == "none");
    REQUIRE(CommandLine::getValue("empty", "none").empty());
  }

  SECTION("integers") {
    const ANSICHAR* argv[] = {"program", "-Width=1280", "-Height=abc", "-Depth=12px",
                              "-Big=99999999999", "-Negative=-5", "-Empty="};
    CommandLine::parse(7, argv);

    REQUIRE(CommandLine::getInt("width", 1920) == 1280);
    REQUIRE(CommandLine::getInt("height", 1080) == 1080);
    REQUIRE(CommandLine::getInt("depth", 3) == 3);
    REQUIRE(CommandLine::getInt("big", 7) == 7);
    REQUIRE(CommandLine::getInt("negative") == -5);
    REQUIRE(CommandLine::getInt("empty", 4) == 4);
    REQUIRE(CommandLine::getInt("missing", 9) == 9);
  }

  SECTION("initialize replaces the previous options") {
    const ANSICHAR* first[] = {"program", "-old=1", "-oldFlag"};
    CommandLine::parse(3, first);
    const ANSICHAR* second[] = {"program"};
    CommandLine::parse(1, second);

    REQUIRE(CommandLine::getValue("old").empty());
    REQUIRE_FALSE(CommandLine::hasFlag("oldflag"));
  }

  SECTION("tryGetValue tells an empty value from a missing one") {
    const ANSICHAR* argv[] = {"program", "-Empty=", "-Flag"};
    CommandLine::parse(3, argv);

    REQUIRE(CommandLine::tryGetValue("empty") != nullptr);
    REQUIRE(CommandLine::tryGetValue("empty")->empty());
    REQUIRE(CommandLine::tryGetValue("flag") == nullptr);
    REQUIRE(CommandLine::tryGetValue("missing") == nullptr);
  }
}

TEST_CASE("chUtilities - ConfigFile", "[Config]") {
  SECTION("sections, keys, comments and quotes") {
    ConfigFile file;
    file.parse("Global = 1\r\n"
               "; comment\n"
               "# comment\n"
               "\n"
               "[Window]\n"
               "  Width = 1280  \n"
               "Title=\"  My Game  \"\n"
               "Path=a=b\n"
               "[ Graphics ]\n"
               "API=chDX12\n"
               "[window]\n"
               "WIDTH=1920");

    REQUIRE(*file.getValue("", "global") == "1");
    REQUIRE(*file.getValue("WINDOW", "width") == "1920");
    REQUIRE(*file.getValue("Window", "Title") == "  My Game  ");
    REQUIRE(*file.getValue("Window", "Path") == "a=b");
    REQUIRE(*file.getValue("Graphics", "API") == "chDX12");
    REQUIRE(file.getValue("Window", "Missing") == nullptr);
    REQUIRE(file.getValue("Missing", "Width") == nullptr);
    REQUIRE(file.getSections().size() == 3);
  }

  SECTION("bad lines are skipped") {
    ConfigFile file;
    file.parse("[Broken\nNoEquals\n=NoKey\n[Ok]\nKey=Value", "test.ini");

    REQUIRE(file.getSections().size() == 1);
    REQUIRE(*file.getValue("Ok", "Key") == "Value");
  }

  SECTION("merge replaces equal keys and keeps the rest") {
    ConfigFile base;
    base.parse("[Window]\nWidth=2560\nHeight=1440");
    ConfigFile over;
    over.parse("[window]\nwidth=1280\n[Graphics]\nVSync=true");
    base.merge(over);

    REQUIRE(*base.getValue("Window", "Width") == "1280");
    REQUIRE(*base.getValue("Window", "Height") == "1440");
    REQUIRE(*base.getValue("Graphics", "VSync") == "true");
  }

  SECTION("toText reads back the same entries") {
    ConfigFile file;
    file.setValue("Window", "Title", " spaced ");
    file.setValue("Window", "Width", "1280");
    file.setValue("", "Global", "1");
    file.setValue("Empty", "Value", "");

    const String text = file.toText();
    REQUIRE(text.starts_with("Global=1\n"));

    ConfigFile copy;
    copy.parse(text);
    REQUIRE(*copy.getValue("Window", "Title") == " spaced ");
    REQUIRE(*copy.getValue("Window", "Width") == "1280");
    REQUIRE(*copy.getValue("", "Global") == "1");
    REQUIRE(copy.getValue("Empty", "Value")->empty());
  }

  SECTION("save and load") {
    const Path path = FileSystem::absolutePath(Path("chConfigFileTest/Test.ini"));
    ConfigFile file;
    file.setValue("Window", "Width", "800");
    REQUIRE(file.save(path));

    ConfigFile loaded;
    REQUIRE(loaded.load(path));
    REQUIRE(*loaded.getValue("Window", "Width") == "800");
    REQUIRE_FALSE(loaded.load(path.getDirectory().join(Path("Missing.ini"))));

    REQUIRE(FileSystem::removeAll(path.getDirectory()));
  }
}

TEST_CASE("chUtilities - ConsoleVariable", "[Config]") {
  SECTION("parsing") {
    ConsoleVariable<bool> boolVar("Test.Bool", false, "");
    ConsoleVariable<int32> intVar("Test.Int", 3, "");
    ConsoleVariable<float> floatVar("Test.Float", 1.0f, "");
    ConsoleVariable<String> stringVar("Test.String", "a", "");

    REQUIRE(boolVar.setFromString("ON", ConsoleVariableSource::Console));
    REQUIRE(boolVar.get());
    REQUIRE(boolVar.setFromString(" 0 ", ConsoleVariableSource::Console));
    REQUIRE_FALSE(boolVar.get());
    REQUIRE_FALSE(boolVar.setFromString("maybe", ConsoleVariableSource::Console));
    REQUIRE_FALSE(boolVar.get());

    REQUIRE(intVar.setFromString("-42", ConsoleVariableSource::Console));
    REQUIRE(intVar.get() == -42);
    REQUIRE_FALSE(intVar.setFromString("12px", ConsoleVariableSource::Console));
    REQUIRE_FALSE(intVar.setFromString("", ConsoleVariableSource::Console));
    REQUIRE(intVar.get() == -42);

    REQUIRE(floatVar.setFromString("0.5", ConsoleVariableSource::Console));
    REQUIRE(floatVar.get() == 0.5f);
    REQUIRE(floatVar.toString() == "0.5");

    REQUIRE(stringVar.setFromString("chDX12", ConsoleVariableSource::Console));
    REQUIRE(stringVar.get() == "chDX12");
    REQUIRE(stringVar.getDefault() == "a");
  }

  SECTION("a lower source does not replace a higher one") {
    ConsoleVariable<int32> var("Test.Priority", 1, "");
    REQUIRE(var.getSource() == ConsoleVariableSource::Default);

    REQUIRE(var.setFromString("2", ConsoleVariableSource::CommandLine));
    REQUIRE_FALSE(var.setFromString("3", ConsoleVariableSource::UserConfig));
    REQUIRE(var.get() == 2);
    REQUIRE(var.set(4));
    REQUIRE(var.get() == 4);
    REQUIRE(var.getSource() == ConsoleVariableSource::Code);
  }

  SECTION("find by name or alias, and getAll") {
    ConsoleVariable<int32> var("Test.Find", 1, "", "FindAlias");

    REQUIRE(ConsoleVariables::find("test.find") == &var);
    REQUIRE(ConsoleVariables::find("FINDALIAS") == &var);
    REQUIRE(ConsoleVariables::find("Test.Missing") == nullptr);
    REQUIRE(Algorithm::contains(ConsoleVariables::getAll(), &var));
  }

  SECTION("config values and the command line, before and after registering") {
    const ANSICHAR* argv[] = {"program", "-Test.FromCommandLine=7", "-Alias=8",
                              "-Test.Flag", "-Test.Both=1", "-BothAlias=2"};
    CommandLine::parse(6, argv);

    ConsoleVariable<int32> early("Test.Early", 0, "");
    ConsoleVariable<int32> fromCommandLine("Test.FromCommandLine", 0, "");
    ConsoleVariable<int32> fromAlias("Test.FromAlias", 0, "", "Alias");
    ConsoleVariable<bool> flag("Test.Flag", false, "");
    ConsoleVariable<int32> both("Test.Both", 0, "", "BothAlias");

    ConsoleVariables::setStartupValue("Test.Early", "1", ConsoleVariableSource::EngineConfig);
    ConsoleVariables::setStartupValue("test.early", "3", ConsoleVariableSource::UserConfig);
    ConsoleVariables::setStartupValue("Test.Early", "2", ConsoleVariableSource::ProjectConfig);
    ConsoleVariables::setStartupValue("Test.FromCommandLine", "5",
                                      ConsoleVariableSource::UserConfig);
    ConsoleVariables::setStartupValue("Test.Late", "9", ConsoleVariableSource::ProjectConfig);
    REQUIRE(early.get() == 0);

    ConsoleVariables::applyStartupValues();
    REQUIRE(early.get() == 3);
    REQUIRE(early.getSource() == ConsoleVariableSource::UserConfig);
    REQUIRE(fromCommandLine.get() == 7);
    REQUIRE(fromAlias.get() == 8);
    REQUIRE(flag.get());
    REQUIRE(both.get() == 1);

    // Registered after the start up, as a plugin's variables are.
    ConsoleVariable<int32> late("Test.Late", 0, "");
    REQUIRE(late.get() == 9);

    ConsoleVariables::setStartupValue("Test.Late", "10", ConsoleVariableSource::UserConfig);
    REQUIRE(late.get() == 10);

    const ANSICHAR* empty[] = {"program"};
    CommandLine::parse(1, empty);
  }

  SECTION("a destroyed variable leaves the registry") {
    {
      ConsoleVariable<int32> var("Test.Scoped", 1, "");
      REQUIRE(ConsoleVariables::find("Test.Scoped") == &var);
    }
    REQUIRE(ConsoleVariables::find("Test.Scoped") == nullptr);
  }
}

TEST_CASE("chUtilities - Config data keys and options", "[Config]") {
  SECTION("variables, exact keys and whole sections") {
    ConsoleVariable<int32> var("TestKeys.Setting", 1, "", "",
                               ConsoleVariableBehavior::RequiresRestart);
    ConfigDataKey exact("TestKeys.Remembered", "A value the editor keeps.");
    ConfigDataKey section("TestData.*", "A section of values.");

    REQUIRE(var.requiresRestart());
    REQUIRE(var.getTypeName() == "int");
    REQUIRE(ConsoleVariables::isKnownConfigKey("testkeys.setting"));
    REQUIRE(ConsoleVariables::isKnownConfigKey("TestKeys.Remembered"));
    REQUIRE(ConsoleVariables::isKnownConfigKey("TestData.Anything"));
    REQUIRE_FALSE(ConsoleVariables::isKnownConfigKey("TestKeys.Setting2"));
    REQUIRE_FALSE(ConsoleVariables::isKnownConfigKey("TestDataX.Anything"));
    // The section alone names no value.
    REQUIRE_FALSE(ConsoleVariables::isKnownConfigKey("TestData."));
    REQUIRE(section.matches("testdata.Key"));
    REQUIRE(Algorithm::contains(ConfigDataKey::getAll(), &exact));
  }

  SECTION("a destroyed data key leaves the registry") {
    {
      ConfigDataKey scoped("TestKeys.Scoped", "");
      REQUIRE(ConsoleVariables::isKnownConfigKey("TestKeys.Scoped"));
    }
    REQUIRE_FALSE(ConsoleVariables::isKnownConfigKey("TestKeys.Scoped"));
  }

  SECTION("command line options") {
    ConsoleVariable<int32> var("TestOptions.Value", 1, "", "OptionAlias");
    CommandLineOption option("TestProjectOption", "A path.");
    const ANSICHAR* argv[] = {"program", "-TestOptions.Value=2", "-optionalias=3",
                              "-testprojectoption=x", "-Typo"};
    CommandLine::parse(5, argv);

    const Vector<String> names = CommandLine::getOptionNames();
    REQUIRE(names.size() == 4);
    REQUIRE(Algorithm::contains(names, String("typo")));
    REQUIRE(CommandLine::isKnownOption("testoptions.value"));
    REQUIRE(CommandLine::isKnownOption("OptionAlias"));
    REQUIRE(CommandLine::isKnownOption("TESTPROJECTOPTION"));
    REQUIRE_FALSE(CommandLine::isKnownOption("typo"));
    REQUIRE(Algorithm::contains(CommandLineOption::getAll(), &option));

    const ANSICHAR* empty[] = {"program"};
    CommandLine::parse(1, empty);
  }
}

#if USING(CH_CONSOLE)
TEST_CASE("chUtilities - Console", "[Config]") {
  SECTION("commands get their words and text") {
    Vector<String> words;
    String text;
    ConsoleCommand command("testEcho", "<words>", "Keeps what it gets.",
                           [&words, &text](const ConsoleArguments& arguments) {
                             words.clear();
                             for (SIZE_T i = 0; i < arguments.getCount(); ++i) {
                               words.emplace_back(arguments.get(i));
                             }
                             text = arguments.text;
                             REQUIRE(arguments.get(arguments.getCount()).empty());
                           });

    REQUIRE(Console::findCommand("TESTECHO") == &command);
    REQUIRE(Console::execute("  testEcho one \"two words\"   three  "));
    REQUIRE(words == Vector<String>{"one", "two words", "three"});
    REQUIRE(text == "one \"two words\"   three");

    REQUIRE(Console::execute("TestEcho"));
    REQUIRE(words.empty());
    REQUIRE(text.empty());
  }

  SECTION("a variable is shown and set with the console source") {
    ConsoleVariable<int32> var("TestConsole.Count", 1, "A count.");
    ConsoleVariable<String> name("TestConsole.Name", "a", "A name.");
    REQUIRE(Console::execute("TestConsole.Count"));
    REQUIRE(var.get() == 1);

    REQUIRE(Console::execute("testconsole.count 5"));
    REQUIRE(var.get() == 5);
    REQUIRE(var.getSource() == ConsoleVariableSource::Console);

    // A value that does not fit keeps the old one; the line still named something.
    REQUIRE(Console::execute("TestConsole.Count five"));
    REQUIRE(var.get() == 5);

    REQUIRE(Console::execute("TestConsole.Name \"two words\""));
    REQUIRE(name.get() == "two words");
    REQUIRE(Console::execute("TestConsole.Name more words"));
    REQUIRE(name.get() == "more words");
  }

  SECTION("unknown names and empty lines") {
    REQUIRE_FALSE(Console::execute("   "));
    REQUIRE_FALSE(Console::execute("TestConsole.Missing 3"));
  }

  SECTION("history keeps no line twice in a row") {
    ConsoleCommand command("testNothing", "", "", [](const ConsoleArguments&) {});
    REQUIRE(Console::execute("testNothing a"));
    REQUIRE(Console::execute("testNothing a"));
    REQUIRE(Console::execute("testNothing b"));
    const Vector<String>& history = Console::getHistory();
    REQUIRE(history.size() >= 2);
    REQUIRE(history[history.size() - 1] == "testNothing b");
    REQUIRE(history[history.size() - 2] == "testNothing a");
    REQUIRE(history.size() <= Console::MAX_HISTORY);
  }

  SECTION("completion lists commands and variables by prefix") {
    ConsoleCommand command("testCompleteCommand", "", "", [](const ConsoleArguments&) {});
    ConsoleVariable<bool> var("TestComplete.Flag", false, "");
    Vector<String> names;
    Console::complete("testcomplete", names);
    REQUIRE(names.size() == 2);
    REQUIRE(Algorithm::contains(names, String("testCompleteCommand")));
    REQUIRE(Algorithm::contains(names, String("TestComplete.Flag")));
    Console::complete("TestCompleteX", names);
    REQUIRE(names.empty());
  }

  SECTION("help and find run") {
    ConsoleVariable<float> var("TestHelp.Speed", 2.0f, "How fast the test runs.");
    REQUIRE(Console::findCommand("help"));
    REQUIRE(Console::findCommand("find"));
    REQUIRE(Console::execute("help"));
    REQUIRE(Console::execute("help TestHelp.Speed"));
    REQUIRE(Console::execute("help find"));
    REQUIRE(Console::execute("find test runs"));
    REQUIRE(Console::execute("find"));
  }

  SECTION("a destroyed command leaves the console") {
    {
      ConsoleCommand scoped("testScoped", "", "", [](const ConsoleArguments&) {});
      REQUIRE(Console::findCommand("testScoped"));
    }
    REQUIRE_FALSE(Console::findCommand("testScoped"));
  }
}
#endif // USING(CH_CONSOLE)
