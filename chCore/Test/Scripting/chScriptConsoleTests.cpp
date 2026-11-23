/************************************************************************/
/**
 * @file chScriptConsoleTests.cpp
 * @author AccelMR
 * @date 2026/11/23
 * @brief Tests of the console commands scripts add.
 */
/************************************************************************/
#include "chPrerequisitesCore.h"

#include "chConsoleVariable.h"
#include "chScriptTestHelpers.h"

#if USING(CH_CONSOLE)
#include "chConsole.h"
#endif // USING(CH_CONSOLE)

#include "lua.h"

#include "catch.hpp"

using namespace chEngineSDK;

namespace {
// A number the script left in a global.
double
readGlobalNumber(ScriptManager& scripts, const ANSICHAR* name)
{
  lua_State* state = scripts.getState();
  lua_getglobal(state, name);
  const double value = lua_tonumber(state, -1);
  lua_pop(state, 1);
  return value;
}
} // namespace

TEST_CASE("chCore - Script console", "[Scripting]")
{
  ScriptManager& scripts = getTestScriptManager();

#if USING(CH_CONSOLE)
  SECTION("a script adds a command that gets its words and text")
  {
    REQUIRE(scripts.runString(R"lua(
      TestWords, TestText, TestRuns = 0, "", 0
      assert(Console.addCommand("testGive", function(words, text)
        TestRuns = TestRuns + 1
        TestWords = #words
        TestText = text
        TestCount = tonumber(words[2]) or 1
      end, { usage = "<item> [count]", help = "Gives an item." }))
    )lua"));
    const ConsoleCommand* command = Console::findCommand("testgive");
    REQUIRE(command);
    REQUIRE(command->getUsage() == "<item> [count]");
    REQUIRE_FALSE(command->isCheat());

    REQUIRE(Console::execute("testGive sword 3"));
    REQUIRE(readGlobalNumber(scripts, "TestRuns") == 1.0);
    REQUIRE(readGlobalNumber(scripts, "TestWords") == 2.0);
    REQUIRE(readGlobalNumber(scripts, "TestCount") == 3.0);
    REQUIRE(scripts.runString(R"lua(assert(TestText == "sword 3"))lua"));

    // Added again, it replaces the first.
    REQUIRE(scripts.runString(R"lua(
      assert(Console.addCommand("testGive", function() TestRuns = TestRuns + 10 end))
    )lua"));
    REQUIRE(Console::execute("testGive"));
    REQUIRE(readGlobalNumber(scripts, "TestRuns") == 11.0);

    REQUIRE(scripts.runString(R"lua(assert(Console.removeCommand("testGive")))lua"));
    REQUIRE_FALSE(Console::findCommand("testGive"));
    REQUIRE(scripts.runString(R"lua(assert(not Console.removeCommand("testGive")))lua"));
  }

  SECTION("names of the engine, names with spaces and misspelt options are refused")
  {
    REQUIRE(scripts.runString(R"lua(
      assert(not Console.addCommand("help", function() end))
      assert(not Console.addCommand("two words", function() end))
    )lua"));
    REQUIRE(Console::findCommand("help"));
    REQUIRE_FALSE(scripts.runString(
        R"lua(Console.addCommand("testBad", function() end, { hlep = "x" }))lua"));
    REQUIRE_FALSE(Console::findCommand("testBad"));
  }

  SECTION("a command runs a console line and cannot remove itself while it runs")
  {
    REQUIRE(scripts.runString(R"lua(
      Console.addCommand("testSelf", function()
        TestRemoved = Console.removeCommand("testSelf")
        TestRan = Console.execute("testOther")
      end)
      Console.addCommand("testOther", function() TestOther = 1 end)
    )lua"));
    REQUIRE(Console::execute("testSelf"));
    REQUIRE(scripts.runString(
        R"lua(assert(TestRemoved == false and TestRan and TestOther == 1))lua"));
    REQUIRE(Console::findCommand("testSelf"));
    scripts.removeConsoleCommands();
    REQUIRE_FALSE(Console::findCommand("testSelf"));
    REQUIRE_FALSE(Console::findCommand("testOther"));
  }

  SECTION("cheats run only while Game.CheatsEnabled is on")
  {
    IConsoleVariable* cheats = ConsoleVariables::find("Game.CheatsEnabled");
    REQUIRE(cheats);
    const String before = cheats->toString();
    REQUIRE(scripts.runString(R"lua(
      TestGod = 0
      Console.addCommand("testGod", function() TestGod = TestGod + 1 end, { cheat = true })
    )lua"));
    REQUIRE(Console::findCommand("testGod")->isCheat());

    REQUIRE(cheats->setFromString("false", ConsoleVariableSource::Console));
    REQUIRE(scripts.runString(R"lua(assert(not Console.areCheatsEnabled()))lua"));
    REQUIRE(Console::execute("testGod"));
    REQUIRE(readGlobalNumber(scripts, "TestGod") == 0.0);

    REQUIRE(cheats->setFromString("true", ConsoleVariableSource::Console));
    REQUIRE(Console::execute("testGod"));
    REQUIRE(readGlobalNumber(scripts, "TestGod") == 1.0);

    REQUIRE(cheats->setFromString(before, ConsoleVariableSource::Console));
    scripts.removeConsoleCommands();
  }
#else
  SECTION("without the console every function does nothing")
  {
    REQUIRE(scripts.runString(R"lua(
      assert(not Console.addCommand("testGive", function() end))
      assert(not Console.execute("help"))
      assert(not Console.areCheatsEnabled())
    )lua"));
  }
#endif // USING(CH_CONSOLE)
}
