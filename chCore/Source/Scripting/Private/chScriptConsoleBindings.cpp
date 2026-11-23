/************************************************************************/
/**
 * @file chScriptConsoleBindings.cpp
 * @author AccelMR
 * @date 2026/11/23
 * @brief The Console table scripts use to add console commands and run console lines.
 */
/************************************************************************/
#include "chScriptBindings.h"

#include "chScriptApi.h"
#include "chScriptBinder.h"
#include "chScriptManager.h"

#if USING(CH_CONSOLE)
#include "chConsole.h"
#endif // USING(CH_CONSOLE)

#include "lauxlib.h"
#include "lua.h"

namespace chEngineSDK {
namespace {

#if USING(CH_CONSOLE)
/*
 * Console.addCommand(name, function(words, text), options?): a console command that calls
 * function with the words typed after the name (a table of strings) and the whole text.
 * options: usage, help, cheat (runs only while Game.CheatsEnabled). A command a script added
 * before with that name is replaced; false when the name has spaces or is the engine's.
 */
int32
addCommand(lua_State* state)
{
  const StringView name = ScriptTraits<StringView>::check(state, 1);
  luaL_checktype(state, 2, LUA_TFUNCTION);
  String usage;
  String help;
  bool bCheat = false;
  if (!lua_isnoneornil(state, 3)) {
    luaL_checktype(state, 3, LUA_TTABLE);
    lua_pushnil(state);
    while (lua_next(state, 3) != 0) {
      const int32 value = lua_gettop(state);
      if (lua_type(state, -2) != LUA_TSTRING) {
        luaL_error(state, "Console.addCommand: options are named fields");
      }
      const StringView key = lua_tostring(state, -2);
      if (key == "usage") {
        usage = ScriptTraits<StringView>::check(state, value);
      }
      else if (key == "help") {
        help = ScriptTraits<StringView>::check(state, value);
      }
      else if (key == "cheat") {
        bCheat = lua_toboolean(state, value) != 0;
      }
      else {
        luaL_error(state, "Console.addCommand: unknown option %s", String(key).c_str());
      }
      lua_pop(state, 1);
    }
  }
  if (!ScriptManager::isStarted() || ScriptManager::instance().getState() != state) {
    lua_pushboolean(state, false);
    return 1;
  }
  lua_pushvalue(state, 2);
  const int32 function = luaL_ref(state, LUA_REGISTRYINDEX);
  lua_pushboolean(state, ScriptManager::instance().addConsoleCommand(
                             name, usage, help, bCheat, function));
  return 1;
}

/*
 * Console.removeCommand(name): false when no script added a command of that name.
 */
int32
removeCommand(lua_State* state)
{
  const StringView name = ScriptTraits<StringView>::check(state, 1);
  const bool bRemoved = ScriptManager::isStarted() &&
                        ScriptManager::instance().getState() == state &&
                        ScriptManager::instance().removeConsoleCommand(name);
  lua_pushboolean(state, bRemoved);
  return 1;
}

/*
 * Console.execute(line): runs a console line as if typed; false when it named nothing.
 */
int32
executeLine(lua_State* state)
{
  const StringView line = ScriptTraits<StringView>::check(state, 1);
  lua_pushboolean(state, Console::execute(line));
  return 1;
}

int32
areCheatsEnabled(lua_State* state)
{
  lua_pushboolean(state, Console::areCheatsEnabled());
  return 1;
}
#else
/*
 * Without the console the functions exist and do nothing, so a script runs the same in a
 * game built without it.
 */
int32
returnFalse(lua_State* state)
{
  lua_pushboolean(state, false);
  return 1;
}
#endif // USING(CH_CONSOLE)

} // namespace

/*
 */
void
ScriptBindings::registerConsole(lua_State* state)
{
#if USING(CH_CONSOLE)
  constexpr lua_CFunction kAddCommand = &addCommand;
  constexpr lua_CFunction kRemoveCommand = &removeCommand;
  constexpr lua_CFunction kExecute = &executeLine;
  constexpr lua_CFunction kAreCheatsEnabled = &areCheatsEnabled;
#else
  constexpr lua_CFunction kAddCommand = &returnFalse;
  constexpr lua_CFunction kRemoveCommand = &returnFalse;
  constexpr lua_CFunction kExecute = &returnFalse;
  constexpr lua_CFunction kAreCheatsEnabled = &returnFalse;
#endif // USING(CH_CONSOLE)

  if (ScriptApiDescription* api = ScriptApiDescription::find(state)) {
    ScriptApiType& options = api->addType("ConsoleCommandOptions", ScriptApiTypeKind::Shape);
    options.description = "How Console.addCommand describes a command; every field is "
                          "optional. A cheat runs only while Game.CheatsEnabled is on.";
    options.fields = {{"usage?", "string"}, {"help?", "string"}, {"cheat?", "boolean"}};
  }

  ScriptTable(state, "Console",
              "Console commands for the game's console and the editor's Output Log (cheats, "
              "debug tools). Without the console built in, every function does nothing.")
      .rawFunction("addCommand", kAddCommand,
                   "fun(name: string, run: fun(words: string[], text: string), options?: "
                   "ConsoleCommandOptions): boolean")
      .rawFunction("removeCommand", kRemoveCommand, "fun(name: string): boolean")
      .rawFunction("execute", kExecute, "fun(line: string): boolean")
      .rawFunction("areCheatsEnabled", kAreCheatsEnabled, "fun(): boolean");
}

} // namespace chEngineSDK
