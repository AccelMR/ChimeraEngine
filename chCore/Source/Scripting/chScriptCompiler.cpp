/************************************************************************/
/**
 * @file chScriptCompiler.cpp
 * @author AccelMR
 * @date 2026/11/21
 * @brief
 *  Compiles Lua text into the bytecode packaged games load.
 */
/************************************************************************/
#include "chScriptCompiler.h"

#if USING(CH_EDITOR)

#include "lauxlib.h"
#include "lua.h"

namespace chEngineSDK {

namespace {
int32
appendBytes(lua_State* state, const void* data, SIZE_T size, void* userData)
{
  CH_PARAMETER_UNUSED(state);
  auto* output = static_cast<Vector<uint8>*>(userData);
  const auto* bytes = static_cast<const uint8*>(data);
  output->insert(output->end(), bytes, bytes + size);
  return 0;
}
} // namespace

/*
 */
bool
ScriptCompiler::compile(StringView source, StringView chunkName, bool bStripDebugInfo,
                        Vector<uint8>& outBytecode, String& outError)
{
  outBytecode.clear();
  lua_State* state = luaL_newstate();
  if (!state) {
    outError = "Could not make a Lua state to compile in.";
    return false;
  }

  const String name(chunkName);
  bool bCompiled =
      luaL_loadbufferx(state, source.data(), source.size(), name.c_str(), "t") == LUA_OK;
  if (bCompiled) {
    bCompiled = lua_dump(state, &appendBytes, &outBytecode, bStripDebugInfo ? 1 : 0) == 0;
    if (!bCompiled) {
      outError = "Could not write the compiled " + name + ".";
    }
  }
  else {
    const ANSICHAR* message = lua_tostring(state, -1);
    outError = message ? String(message) : "Could not compile " + name + ".";
  }
  lua_close(state);
  if (!bCompiled) {
    outBytecode.clear();
  }
  return bCompiled;
}

} // namespace chEngineSDK

#endif // USING(CH_EDITOR)
