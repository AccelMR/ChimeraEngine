/************************************************************************/
/**
 * @file chShaderCompiler.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Compiles HLSL with the DirectX Shader Compiler library.
 */
/************************************************************************/
#include "chShaderCompiler.h"

#include <cstring>

#include "chDynamicLibrary.h"
#include "chEnginePaths.h"
#include "chFileSystem.h"
#include "chLogger.h"
#include "chStringUtils.h"
#include "chUnicode.h"

#if USING(CH_PLATFORM_WIN32)
// dxcapi.h names Windows types and IUnknown without including them.
#include <windows.h>
#include <unknwn.h>
#endif // USING(CH_PLATFORM_WIN32)
#include <dxcapi.h>

namespace chEngineSDK {

CH_LOG_DECLARE_STATIC(ShaderCompilerLog, All);

namespace {
#if USING(CH_PLATFORM_WIN32)
constexpr StringView kLibraryName = "dxcompiler.dll";
#else
constexpr StringView kLibraryName = "libdxcompiler.so";
#endif // USING(CH_PLATFORM_WIN32)

// Same shader model and language version as cmake/chShaders.cmake.
constexpr StringView kShaderModel = "6_6";

// The interface ids are written here because taking them with __uuidof is a compiler
// extension some of our compilers refuse.
const GUID kIidUnknown = {0x00000000, 0x0000, 0x0000,
                          {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const GUID kIidCompiler3 = {0x228B4687, 0x5A6A, 0x4730,
                            {0x90, 0x0C, 0x97, 0x02, 0xB2, 0x20, 0x3F, 0x54}};
const GUID kIidUtils = {0x4605C4CB, 0x2019, 0x492A,
                        {0xAD, 0xA4, 0x65, 0xF2, 0x0B, 0xB7, 0xD6, 0x7F}};
const GUID kIidResult = {0x58346CDA, 0xDDE7, 0x4497,
                         {0x94, 0x61, 0x6F, 0x87, 0xAF, 0x5E, 0x06, 0x59}};
const GUID kIidBlob = {0x8BA5FB08, 0x5195, 0x40E2,
                       {0xAC, 0x58, 0x0D, 0x98, 0x9C, 0x3A, 0x01, 0x02}};
const GUID kIidBlobUtf8 = {0x3DA636C9, 0xBA71, 0x4024,
                           {0xA3, 0x01, 0x30, 0xCB, 0xF1, 0x25, 0x30, 0x5B}};
const GUID kIidIncludeHandler = {0x7F61FC7D, 0x950D, 0x467F,
                                 {0xB3, 0xE3, 0x3C, 0x02, 0xFB, 0x49, 0x18, 0x7C}};

NODISCARD bool
isSameGuid(const GUID& first, const GUID& second) noexcept
{
  return std::memcmp(&first, &second, sizeof(GUID)) == 0;
}

/**
 * Releases a COM object when it goes out of scope.
 */
template<typename T>
class ComRef
{
 public:
  ComRef() = default;

  ~ComRef()
  {
    if (m_object) {
      m_object->Release();
    }
  }

  ComRef(const ComRef&) = delete;

  ComRef&
  operator=(const ComRef&) = delete;

  NODISCARD void**
  put() noexcept
  {
    return reinterpret_cast<void**>(&m_object);
  }

  NODISCARD T*
  get() const noexcept
  {
    return m_object;
  }

  T*
  operator->() const noexcept
  {
    return m_object;
  }

 private:
  T* m_object = nullptr;
};

/**
 * Gives the compiler the files the source includes: the codec's virtual files first, then
 * files from disk, which it keeps in a list so the editor knows what to watch.
 */
class IncludeHandler final : public IDxcIncludeHandler
{
 public:
  IncludeHandler(IDxcUtils& utils, const ShaderCompileRequest& request, Vector<Path>& outFiles)
    : m_utils(utils),
      m_request(request),
      m_files(outFiles)
  {}

  ~IncludeHandler() = default;

  HRESULT STDMETHODCALLTYPE
  LoadSource(LPCWSTR fileName, IDxcBlob** outSource) override
  {
    if (!outSource) {
      return E_POINTER;
    }
    *outSource = nullptr;

    const String name = UTF8::fromWide(WString(fileName));
    const String fileNameOnly = Path(name).getFileName();
    for (const ShaderVirtualFile& file : m_request.virtualFiles) {
      if (file.name == fileNameOnly) {
        return makeBlob(file.text.data(), file.text.size(), outSource);
      }
    }

    const Path path = findFile(name);
    if (path.empty()) {
      return E_FAIL;
    }
    const Vector<uint8> data = FileSystem::fastRead(path);
    const Path absolute = FileSystem::absolutePath(path);
    bool bKnown = false;
    for (const Path& known : m_files) {
      bKnown = bKnown || known == absolute;
    }
    if (!bKnown) {
      m_files.push_back(absolute);
    }
    return makeBlob(data.data(), data.size(), outSource);
  }

  HRESULT STDMETHODCALLTYPE
  QueryInterface(REFIID id, void** outObject) override
  {
    if (!outObject) {
      return E_POINTER;
    }
    if (isSameGuid(id, kIidIncludeHandler) || isSameGuid(id, kIidUnknown)) {
      *outObject = static_cast<IDxcIncludeHandler*>(this);
      return S_OK;
    }
    *outObject = nullptr;
    return E_NOINTERFACE;
  }

  // Lives on the stack for one compile, so counting references would change nothing.
  ULONG STDMETHODCALLTYPE
  AddRef() override
  {
    return 1;
  }

  ULONG STDMETHODCALLTYPE
  Release() override
  {
    return 1;
  }

 private:
  NODISCARD HRESULT
  makeBlob(const void* data, const SIZE_T size, IDxcBlob** outSource)
  {
    IDxcBlobEncoding* blob = nullptr;
    const HRESULT result =
        m_utils.CreateBlob(data, static_cast<UINT32>(size), DXC_CP_UTF8, &blob);
    *outSource = blob;
    return result;
  }

  /**
   * The compiler passes the include joined with each folder it searches; a name it
   * could not place is tried next to the source and in every include folder.
   */
  NODISCARD Path
  findFile(const String& name) const
  {
    const Path asGiven(name);
    if (FileSystem::isFile(asGiven)) {
      return asGiven;
    }
    String relative = name;
    while (relative.size() > 2 && relative[0] == '.' &&
           (relative[1] == '/' || relative[1] == '\\')) {
      relative.erase(0, 2);
    }
    const Path relativePath(relative);
    const Path besideSource = m_request.sourcePath.getDirectory().join(relativePath);
    if (FileSystem::isFile(besideSource)) {
      return besideSource;
    }
    for (const Path& directory : m_request.includeDirectories) {
      const Path candidate = directory.join(relativePath);
      if (FileSystem::isFile(candidate)) {
        return candidate;
      }
    }
    return Path();
  }

  IDxcUtils& m_utils;
  const ShaderCompileRequest& m_request;
  Vector<Path>& m_files;
};

NODISCARD WString
toWide(StringView text)
{
  return UTF8::toWide(String(text));
}
} // namespace

/*
 */
ShaderCompiler::ShaderCompiler()
{
  // The build copies the library into Codecs/ShaderCompiler, where the editor does not take
  // it for a codec; a system install is tried after it.
  const Path besideCodec(EnginePaths::getCodecDirectory(), Path("ShaderCompiler"),
                         Path(String(kLibraryName)));
  m_library = chMakeUnique<DynamicLibrary>(
      FileSystem::isFile(besideCodec) ? besideCodec : Path(String(kLibraryName)));
  if (!m_library->isLoaded()) {
    CH_LOG_ERROR(ShaderCompilerLog,
                 "The DirectX Shader Compiler ({0}) could not be opened; shaders cannot be "
                 "imported. It comes with the Vulkan SDK.",
                 kLibraryName);
    return;
  }
  m_createInstance = m_library->getSymbol("DxcCreateInstance");
  if (!m_createInstance) {
    CH_LOG_ERROR(ShaderCompilerLog, "{0} has no DxcCreateInstance.", kLibraryName);
  }
}

/*
 */
ShaderCompiler::~ShaderCompiler() = default;

/*
 */
bool
ShaderCompiler::compile(const ShaderCompileRequest& request,
                        ShaderCompileResult& outResult) const
{
  outResult = ShaderCompileResult();
  if (!m_createInstance) {
    outResult.messages = "The DirectX Shader Compiler is not available.";
    return false;
  }
  const DxcCreateInstanceProc createInstance =
      reinterpret_cast<DxcCreateInstanceProc>(m_createInstance);

  ComRef<IDxcUtils> utils;
  ComRef<IDxcCompiler3> compiler;
  if (FAILED(createInstance(CLSID_DxcUtils, kIidUtils, utils.put())) ||
      FAILED(createInstance(CLSID_DxcCompiler, kIidCompiler3, compiler.put()))) {
    outResult.messages = "The DirectX Shader Compiler could not be started.";
    return false;
  }

  Vector<WString> arguments;
  arguments.push_back(UTF8::toWide(request.sourcePath.toString()));
  arguments.push_back(L"-E");
  arguments.push_back(toWide(request.entryPoint));
  arguments.push_back(L"-T");
  arguments.push_back(toWide(StringUtils::format("{0}_{1}", request.stage, kShaderModel)));
  arguments.push_back(L"-HV");
  arguments.push_back(L"2021");
  arguments.push_back(L"-O3");
  for (const Path& directory : request.includeDirectories) {
    arguments.push_back(L"-I");
    arguments.push_back(UTF8::toWide(directory.toString()));
  }
  if (request.target == ShaderCompileTarget::Spirv) {
    arguments.push_back(L"-spirv");
    arguments.push_back(L"-fspv-target-env=vulkan1.3");
    // Storage images get no format from their HLSL type, as in the engine's shaders.
    arguments.push_back(L"-fspv-use-unknown-image-format");
  }
  else {
    arguments.push_back(L"-Wno-ignored-attributes");
  }
  Vector<LPCWSTR> argumentPointers;
  argumentPointers.reserve(arguments.size());
  for (const WString& argument : arguments) {
    argumentPointers.push_back(argument.c_str());
  }

  const DxcBuffer source{.Ptr = request.sourceText.data(),
                         .Size = request.sourceText.size(),
                         .Encoding = DXC_CP_UTF8};
  IncludeHandler includeHandler(*utils.get(), request, outResult.includedFiles);
  ComRef<IDxcResult> result;
  if (FAILED(compiler->Compile(&source,
                               argumentPointers.data(),
                               static_cast<UINT32>(argumentPointers.size()),
                               &includeHandler,
                               kIidResult,
                               result.put()))) {
    outResult.messages = "The DirectX Shader Compiler failed to run.";
    return false;
  }

  ComRef<IDxcBlobUtf8> errors;
  if (SUCCEEDED(result->GetOutput(DXC_OUT_ERRORS, kIidBlobUtf8, errors.put(), nullptr)) &&
      errors.get() && errors->GetStringLength() > 0) {
    outResult.messages.assign(errors->GetStringPointer(), errors->GetStringLength());
  }

  HRESULT status = S_OK;
  if (FAILED(result->GetStatus(&status)) || FAILED(status)) {
    return false;
  }
  ComRef<IDxcBlob> object;
  if (FAILED(result->GetOutput(DXC_OUT_OBJECT, kIidBlob, object.put(), nullptr)) ||
      !object.get() || object->GetBufferSize() == 0) {
    if (outResult.messages.empty()) {
      outResult.messages = "The compiler gave no code.";
    }
    return false;
  }
  const uint8* bytes = static_cast<const uint8*>(object->GetBufferPointer());
  outResult.code.assign(bytes, bytes + object->GetBufferSize());
  return true;
}

} // namespace chEngineSDK
