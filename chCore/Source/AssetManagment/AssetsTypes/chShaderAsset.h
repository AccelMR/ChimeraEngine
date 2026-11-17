/************************************************************************/
/**
 * @file chShaderAsset.h
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Asset that holds a project shader compiled for every graphics API.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chIAsset.h"
#include "chShaderParameters.h"
#include "chTypeTraits.h"

namespace chEngineSDK {

class IShader;

// Stored in shader assets, so the values are fixed.
enum class ShaderAssetKind : uint32
{
  PostProcess = 0,
  COUNT
};

// Written after the common asset start.
struct ShaderAssetHeader
{
  static constexpr uint32 VERSION = 1;

  uint32 version = VERSION;
  // ShaderAssetKind
  uint32 kind = 0;
  // PostProcessStage
  uint32 stage = 0;
  uint32 parameterCount = 0;
  uint32 parameterBufferSize = 0;
  uint32 binaryCount = 0;
  // ShaderAsset::INTERFACE_VERSION of the engine headers the code was compiled against.
  uint32 interfaceVersion = 0;
};
static_assert(sizeof(ShaderAssetHeader) == 28, "ShaderAssetHeader must have no padding");

/**
 * The compiled code for one shader binary format (IGraphicsAPI::getShaderBinaryFormat's
 * folder). A shader without its own vertex stage leaves vertexCode empty and is drawn with
 * the engine's vertex shader for its kind.
 */
struct ShaderBinary
{
  String format;
  Vector<uint8> vertexCode;
  Vector<uint8> pixelCode;
};

struct ShaderAssetData
{
  ShaderAssetKind kind = ShaderAssetKind::PostProcess;
  PostProcessStage stage = PostProcessStage::AfterTonemap;
  Vector<ShaderParameter> parameters;
  uint32 parameterBufferSize = 0;
  Vector<ShaderBinary> binaries;
  uint32 interfaceVersion = 0;
};

/**
 * Exists so a project can bring its own shaders without building any C++: the editor
 * compiles the source when it is imported and the asset keeps the code for each graphics
 * API, so a game only loads bytes. The GPU shaders are made the first time they are asked
 * for, for the API that runs.
 */
class CH_CORE_EXPORT ShaderAsset : public IAsset
{
 public:
  ShaderAsset() = delete;

  ShaderAsset(const AssetMetadata& metadata) : IAsset(metadata) {}

  /**
   * For assets made in code (importers, tests).
   */
  ShaderAsset(const AssetMetadata& metadata, ShaderAssetData data);

  ~ShaderAsset() override;

  NODISCARD FORCEINLINE const ShaderAssetData&
  getData() const noexcept
  {
    return m_data;
  }

  NODISCARD FORCEINLINE ShaderAssetKind
  getKind() const noexcept
  {
    return m_data.kind;
  }

  NODISCARD FORCEINLINE PostProcessStage
  getStage() const noexcept
  {
    return m_data.stage;
  }

  NODISCARD FORCEINLINE Span<const ShaderParameter>
  getParameters() const noexcept
  {
    return m_data.parameters;
  }

  NODISCARD FORCEINLINE uint32
  getParameterBufferSize() const noexcept
  {
    return m_data.parameterBufferSize;
  }

  /**
   * Index into getParameters, or NOT_FOUND. Linear, for load time and scripts.
   */
  NODISCARD uint32
  findParameter(StringView name) const noexcept;

  /**
   * The binary of that format, or null.
   */
  NODISCARD const ShaderBinary*
  findBinary(StringView format) const noexcept;

  /**
   * The pixel shader for the graphics API that runs, made on first use (main thread). Null
   * when the asset has no code for that API or the API is not started.
   */
  NODISCARD const SPtr<IShader>&
  getPixelShader();

  /**
   * Null when the shader has no vertex stage of its own.
   */
  NODISCARD const SPtr<IShader>&
  getVertexShader();

  /**
   * Goes up every time the code changes, so whoever built pipelines from the shaders knows
   * to build them again.
   */
  NODISCARD FORCEINLINE uint32
  getRevision() const noexcept
  {
    return m_revision;
  }

  /**
   * False when the code was compiled against older engine headers: the engine no longer
   * gives it what it reads, so it is never drawn (the editor compiles it again).
   */
  NODISCARD FORCEINLINE bool
  isInterfaceCurrent() const noexcept
  {
    return m_data.interfaceVersion == INTERFACE_VERSION;
  }

#if USING(CH_EDITOR)
  /**
   * Replaces everything with a new compile of the source.
   */
  void
  setData(ShaderAssetData data);

  /**
   * Files the source included, so the editor can compile again when one of them changes.
   */
  NODISCARD FORCEINLINE const Vector<Path>&
  getIncludedFiles() const noexcept
  {
    return m_includedFiles;
  }

  void
  setIncludedFiles(Vector<Path> files);

  /**
   * The error of the last compile that failed, empty after one that worked. Kept only in
   * memory: the asset holds the last code that compiled.
   */
  NODISCARD FORCEINLINE const String&
  getCompileError() const noexcept
  {
    return m_compileError;
  }

  void
  setCompileError(String error);
#endif // USING(CH_EDITOR)

  // Goes up whenever what chPostProcessCommon.hlsli and chPostProcess.hlsli read from the
  // engine changes (push constants, how the parameters are found).
  static constexpr uint32 INTERFACE_VERSION = 1;
  static constexpr uint32 NOT_FOUND = 0xFFFFFFFFu;
  static constexpr uint32 MAX_BINARIES = 8;
  static constexpr uint32 MAX_CODE_SIZE = 16 * 1024 * 1024;
  static constexpr uint32 MAX_FORMAT_LENGTH = 32;
  static constexpr uint32 MAX_INCLUDED_FILES = 256;

 protected:
  bool
  serialize(SPtr<DataStream> stream) override;

  bool
  deserialize(SPtr<DataStream> stream) override;

  void
  clearAssetData() override;

#if USING(CH_EDITOR)
  void
  writeEditorValues(ConfigFile& file) const override;

  void
  readEditorValues(const ConfigFile& file) override;
#endif // USING(CH_EDITOR)

 private:
  void
  createShaders();

  ShaderAssetData m_data;
  SPtr<IShader> m_vertexShader;
  SPtr<IShader> m_pixelShader;
  uint32 m_revision = 0;
  bool m_bShadersCreated = false;

#if USING(CH_EDITOR)
  Vector<Path> m_includedFiles;
  String m_compileError;
#endif // USING(CH_EDITOR)
};

DECLARE_ASSET_TYPE(ShaderAsset);

} // namespace chEngineSDK
