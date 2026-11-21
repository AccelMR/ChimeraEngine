/************************************************************************/
/**
 * @file chScriptCompiler.h
 * @author AccelMR
 * @date 2026/11/21
 * @brief
 *  Compiles Lua text into the bytecode packaged games load.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#if USING(CH_EDITOR)

namespace chEngineSDK {

/**
 * Exists so Package Project ships scripts already compiled: games load them without parsing
 * text. It runs nothing; each call uses a Lua state of its own, so any thread may call it.
 */
class CH_CORE_EXPORT ScriptCompiler
{
 public:
  /**
   * Compiles source under chunkName ("@/Game/Scripts/Player.lua", what errors show) into
   * outBytecode. bStripDebugInfo drops line numbers and local names: smaller, but errors in
   * the game then lose their file and line. False with outError set on a syntax error.
   */
  NODISCARD static bool
  compile(StringView source, StringView chunkName, bool bStripDebugInfo,
          Vector<uint8>& outBytecode, String& outError);
};

} // namespace chEngineSDK

#endif // USING(CH_EDITOR)
