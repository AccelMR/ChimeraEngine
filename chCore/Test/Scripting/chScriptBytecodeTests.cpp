/************************************************************************/
/**
 * @file chScriptBytecodeTests.cpp
 * @author AccelMR
 * @date 2026/11/21
 * @brief Tests of compiling scripts for packages and of who may run compiled Lua.
 */
/************************************************************************/
#include "chPrerequisitesCore.h"

#if USING(CH_EDITOR)

#include "chScriptCompiler.h"
#include "chScriptTestHelpers.h"

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "catch.hpp"

using namespace chEngineSDK;

namespace {
constexpr const ANSICHAR* kSource = "-- chimera-uuid: 00000000-0000-0000-0000-000000000000\n"
                                    "local value = 40 + 2\n"
                                    "if value == 42 then error('boom') end\n";

// Runs a chunk with mode in a plain state and gives what it raised, or "ok".
String
runChunk(Span<const uint8> chunk, const ANSICHAR* mode)
{
  lua_State* state = luaL_newstate();
  luaL_openlibs(state);
  String result = "ok";
  const ANSICHAR* chunkData = reinterpret_cast<const ANSICHAR*>(chunk.data());
  if (luaL_loadbufferx(state, chunkData, chunk.size(), "@/Game/Test.lua", mode) != LUA_OK
      || lua_pcall(state, 0, 0, 0) != LUA_OK) {
    result = lua_tostring(state, -1);
  }
  lua_close(state);
  return result;
}
} // namespace

TEST_CASE("chCore - Script bytecode", "[Scripting]")
{
  SECTION("Compiled scripts run and keep their lines unless stripped")
  {
    Vector<uint8> bytecode;
    String error;
    REQUIRE(ScriptCompiler::compile(kSource, "@/Game/Test.lua", false, bytecode, error));
    REQUIRE(ScriptAsset::isBytecode(
        StringView(reinterpret_cast<const ANSICHAR*>(bytecode.data()), bytecode.size())));
    // Line 3 counts the UUID line, as in the editor.
    REQUIRE(runChunk(bytecode, "b") == "/Game/Test.lua:3: boom");

    Vector<uint8> stripped;
    REQUIRE(ScriptCompiler::compile(kSource, "@/Game/Test.lua", true, stripped, error));
    REQUIRE(stripped.size() < bytecode.size());
    // Without line information Lua cannot say where the error was raised.
    REQUIRE(runChunk(stripped, "b") == "boom");
  }

  SECTION("Text mode refuses compiled code")
  {
    Vector<uint8> bytecode;
    String error;
    REQUIRE(ScriptCompiler::compile(kSource, "@/Game/Test.lua", false, bytecode, error));
    REQUIRE(runChunk(bytecode, "t").find("binary chunk") != String::npos);
  }

  SECTION("Syntax errors name the line")
  {
    Vector<uint8> bytecode;
    String error;
    REQUIRE_FALSE(ScriptCompiler::compile("local a = \nlocal b = )", "@/Game/Bad.lua", false,
                                          bytecode, error));
    REQUIRE(bytecode.empty());
    REQUIRE(error.find("/Game/Bad.lua:2:") != String::npos);
  }

  SECTION("Compiled code from outside the game's own content never runs")
  {
    Vector<uint8> bytecode;
    String error;
    REQUIRE(ScriptCompiler::compile("return { onStart = function() end }",
                                    "@/Game/Scripts/Modded.lua", false, bytecode, error));
    // Made from bytes in memory, the script comes from no layer of the game, as a mod's.
    AssetMetadata metadata;
    metadata.uuid = chEngineSDK::UUID::createRandom();
    metadata.assetType = AssetTypeTraits<ScriptAsset>::getTypeId();
    StringUtils::copyToBuffer(metadata.name, "Modded");
    StringUtils::copyToBuffer(metadata.assetPath, "/Game/Scripts");
    const auto script = chMakeShared<ScriptAsset>(
        metadata, String(reinterpret_cast<const ANSICHAR*>(bytecode.data()), bytecode.size()));
    REQUIRE(script->isBytecode());
    REQUIRE_FALSE(script->isFromGameLayer());
    const ScriptClassInfo* info = getTestScriptManager().getClass(*script);
    REQUIRE(info);
    REQUIRE_FALSE(info->bValid);
  }
}

#endif // USING(CH_EDITOR)
