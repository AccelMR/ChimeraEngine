/************************************************************************/
/**
 * @file chShaderCompiler.h
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Compiles HLSL with the DirectX Shader Compiler library.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chPath.h"

namespace chEngineSDK {

class DynamicLibrary;

enum class ShaderCompileTarget
{
  Spirv,
  Dxil
};

/**
 * A file the include handler gives without reading the disk, such as the parameter struct
 * the codec makes from the CH_PARAM lines.
 */
struct ShaderVirtualFile
{
  String name;
  String text;
};

struct ShaderCompileRequest
{
  // Shown in errors and used to find includes next to the source.
  Path sourcePath;
  StringView sourceText;
  // "vs" or "ps".
  StringView stage;
  StringView entryPoint;
  ShaderCompileTarget target = ShaderCompileTarget::Spirv;
  Span<const Path> includeDirectories;
  Span<const ShaderVirtualFile> virtualFiles;
};

struct ShaderCompileResult
{
  Vector<uint8> code;
  // Every file from disk the source included, absolute.
  Vector<Path> includedFiles;
  // Errors and warnings as the compiler wrote them.
  String messages;
};

/**
 * Exists so the shader codec compiles a project's shaders the same way the build compiles
 * the engine's (cmake/chShaders.cmake), without running dxc as a program. The library is
 * opened once when the codec loads; compile may run on any thread, each call makes its own
 * compiler.
 */
class ShaderCompiler
{
 public:
  ShaderCompiler();

  ~ShaderCompiler();

  ShaderCompiler(const ShaderCompiler&) = delete;

  ShaderCompiler&
  operator=(const ShaderCompiler&) = delete;

  NODISCARD bool
  isLoaded() const noexcept
  {
    return m_createInstance != nullptr;
  }

  NODISCARD bool
  compile(const ShaderCompileRequest& request, ShaderCompileResult& outResult) const;

 private:
  UniquePtr<DynamicLibrary> m_library;
  void* m_createInstance = nullptr;
};

} // namespace chEngineSDK
