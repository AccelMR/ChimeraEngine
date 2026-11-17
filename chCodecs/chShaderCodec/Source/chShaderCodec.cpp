/************************************************************************/
/**
 * @file chShaderCodec.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Codec that compiles a project's HLSL files into shader assets.
 */
/************************************************************************/
#include "chShaderCodec.h"

#include "chAssetManager.h"
#include "chEnginePaths.h"
#include "chFileSystem.h"
#include "chLogger.h"
#include "chStringUtils.h"

namespace chEngineSDK {

CH_LOG_DECLARE_STATIC(ShaderCodecLog, All);

namespace {
// The include chPostProcess.hlsli asks for; made from the CH_PARAM lines.
constexpr StringView kParamsFileName = "chShaderParams.hlsli";

struct TargetFormat
{
  ShaderCompileTarget target;
  // IGraphicsAPI::getShaderBinaryFormat's folder for the API that reads it.
  StringView format;
};

#if USING(CH_PLATFORM_WIN32)
constexpr TargetFormat kTargets[] = {{ShaderCompileTarget::Spirv, "SPIRV"},
                                     {ShaderCompileTarget::Dxil, "DXIL"}};
#else
constexpr TargetFormat kTargets[] = {{ShaderCompileTarget::Spirv, "SPIRV"}};
#endif // USING(CH_PLATFORM_WIN32)

/**
 * True when name( appears as a whole word, the way cmake/chShaders.cmake finds the entry
 * points of the engine's shaders.
 */
NODISCARD bool
isIdentifierChar(const ANSICHAR c) noexcept
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

NODISCARD bool
hasEntryPoint(StringView text, StringView name)
{
  SIZE_T position = text.find(name);
  while (position != StringView::npos) {
    const bool bWordStart = position == 0 || !isIdentifierChar(text[position - 1]);
    SIZE_T after = position + name.size();
    while (after < text.size() && (text[after] == ' ' || text[after] == '\t')) {
      ++after;
    }
    if (bWordStart && after < text.size() && text[after] == '(') {
      return true;
    }
    position = text.find(name, position + 1);
  }
  return false;
}

/**
 * Exists so the compile runs on the import's worker thread and only the asset is made or
 * updated on the main thread.
 */
class ShaderImportTask final : public IImportTask
{
 public:
  ShaderImportTask(const ShaderCompiler& compiler,
                   const Path& filePath,
                   const String& assetName,
                   const ImportSettings& settings)
    : m_compiler(compiler),
      m_filePath(FileSystem::absolutePath(filePath)),
      m_assetName(assetName),
      m_settings(settings),
      m_engineShaderDirectory(
          FileSystem::absolutePath(EnginePaths::getEngineContentDirectory().join(Path("Shaders"))))
  {}

  NODISCARD bool
  readSource(ImportProgress& progress) override
  {
    progress.set("Reading the shader", -1.0f);
    const Vector<uint8> bytes = FileSystem::fastRead(m_filePath);
    if (bytes.empty()) {
      CH_LOG_ERROR(ShaderCodecLog, "The shader {0} could not be read.", m_filePath);
      return false;
    }
    SIZE_T start = 0;
    if (bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
      start = 3;
    }
    const String text(reinterpret_cast<const ANSICHAR*>(bytes.data()) + start,
                      bytes.size() - start);
    compile(text, progress);
    return !progress.isCancelled();
  }

  SPtr<IAsset>
  createAssets(const Path& assetFolder) override
  {
    AssetManager& assetManager = AssetManager::instance();
    SPtr<ShaderAsset> existing;
    if (const SPtr<IAsset> found = assetManager.findAssetBySource(m_filePath, StringView())) {
      existing = found->as<ShaderAsset>();
    }

    if (!m_error.empty()) {
      CH_LOG_ERROR(ShaderCodecLog, "The shader {0} does not compile:\n{1}", m_filePath, m_error);
      if (existing) {
        existing->setCompileError(m_error);
      }
      return nullptr;
    }
    if (!m_warnings.empty()) {
      CH_LOG_WARNING(ShaderCodecLog, "The shader {0} compiled with warnings:\n{1}", m_filePath,
                     m_warnings);
    }

    if (existing) {
      if (!existing->isLoaded() && !assetManager.syncLoadAsset(existing)) {
        CH_LOG_WARNING(ShaderCodecLog, "Shader {0} could not be loaded; its file is replaced.",
                       existing->getName());
      }
      existing->setData(std::move(m_data));
      existing->setIncludedFiles(std::move(m_includedFiles));
      if (!assetManager.saveAsset(existing)) {
        return nullptr;
      }
      CH_LOG_INFO(ShaderCodecLog, "Compiled shader {0} again from {1}", existing->getName(),
                  m_filePath);
      return existing;
    }

    const AssetMetadata metadata =
        IAssetCodec::makeMetadata<ShaderAsset>(m_assetName, assetFolder);
    const SPtr<ShaderAsset> asset = chMakeShared<ShaderAsset>(metadata, std::move(m_data));
    asset->setIncludedFiles(std::move(m_includedFiles));
    if (!IAssetCodec::saveAndRegister(asset, m_filePath, StringView(), m_settings)) {
      CH_LOG_ERROR(ShaderCodecLog, "Failed to save shader asset: {0}", m_assetName);
      return nullptr;
    }
    CH_LOG_INFO(ShaderCodecLog, "Imported shader {0} from {1}", asset->getName(), m_filePath);
    return asset;
  }

 private:
  void
  compile(const String& text, ImportProgress& progress)
  {
    ShaderParameters::SourceInfo info;
    String error;
    if (!ShaderParameters::parseSource(text, info, error)) {
      m_error = std::move(error);
      return;
    }
    if (!hasEntryPoint(text, "PSMain")) {
      m_error = "The shader has no PSMain.";
      return;
    }
    const bool bHasVertex = hasEntryPoint(text, "VSMain");

    const ShaderVirtualFile virtualFiles[] = {
        {String(kParamsFileName), ShaderParameters::makeHlslStruct(info.parameters)}};
    const Path includeDirectories[] = {m_engineShaderDirectory};

    m_data = ShaderAssetData{.kind = ShaderAssetKind::PostProcess,
                             .stage = info.stage,
                             .parameters = std::move(info.parameters),
                             .parameterBufferSize = info.bufferSize,
                             .binaries = {}};
    for (const TargetFormat& target : kTargets) {
      progress.set("Compiling the shader", -1.0f);
      ShaderBinary binary{.format = String(target.format), .vertexCode = {}, .pixelCode = {}};
      if (!compileStage(text, "ps", "PSMain", target.target, virtualFiles, includeDirectories,
                        binary.pixelCode)) {
        return;
      }
      if (bHasVertex && !compileStage(text, "vs", "VSMain", target.target, virtualFiles,
                                      includeDirectories, binary.vertexCode)) {
        return;
      }
      m_data.binaries.push_back(std::move(binary));
      if (progress.isCancelled()) {
        return;
      }
    }
  }

  NODISCARD bool
  compileStage(const String& text,
               StringView stage,
               StringView entryPoint,
               ShaderCompileTarget target,
               Span<const ShaderVirtualFile> virtualFiles,
               Span<const Path> includeDirectories,
               Vector<uint8>& outCode)
  {
    const ShaderCompileRequest request{.sourcePath = m_filePath,
                                       .sourceText = text,
                                       .stage = stage,
                                       .entryPoint = entryPoint,
                                       .target = target,
                                       .includeDirectories = includeDirectories,
                                       .virtualFiles = virtualFiles};
    ShaderCompileResult result;
    const bool bCompiled = m_compiler.compile(request, result);
    for (Path& file : result.includedFiles) {
      bool bKnown = false;
      for (const Path& known : m_includedFiles) {
        bKnown = bKnown || known == file;
      }
      if (!bKnown) {
        m_includedFiles.push_back(std::move(file));
      }
    }
    if (!bCompiled) {
      m_error = result.messages.empty() ? String("The shader does not compile.")
                                        : std::move(result.messages);
      return false;
    }
    // Every target reports the same warnings; keeping the first one's is enough.
    if (m_warnings.empty()) {
      m_warnings = std::move(result.messages);
    }
    outCode = std::move(result.code);
    return true;
  }

  const ShaderCompiler& m_compiler;
  Path m_filePath;
  String m_assetName;
  ImportSettings m_settings;
  Path m_engineShaderDirectory;
  ShaderAssetData m_data;
  Vector<Path> m_includedFiles;
  String m_error;
  String m_warnings;
};
} // namespace

/*
 */
UniquePtr<IImportTask>
ShaderCodec::createImportTask(const Path& filePath,
                              const String& assetName,
                              const ImportSettings& settings)
{
  return chMakeUnique<ShaderImportTask>(m_compiler, filePath, assetName, settings);
}

} // namespace chEngineSDK
