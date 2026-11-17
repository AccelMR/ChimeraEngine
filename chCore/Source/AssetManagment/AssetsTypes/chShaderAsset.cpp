/************************************************************************/
/**
 * @file chShaderAsset.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Asset that holds a project shader compiled for every graphics API.
 */
/************************************************************************/
#include "chShaderAsset.h"

#include "chAssetFile.h"
#include "chAssetManager.h"
#include "chEnginePaths.h"
#include "chFileStream.h"
#include "chIGraphicsAPI.h"
#include "chIShader.h"
#include "chLogger.h"
#include "chMath.h"
#include "chStringUtils.h"

#if USING(CH_EDITOR)
#include "chConfigFile.h"
#include "chFileSystem.h"
#endif // USING(CH_EDITOR)

namespace chEngineSDK {

namespace {
// Written per parameter after its name, before its tooltip.
struct ParameterFileData
{
  // ShaderParameterType
  uint32 type = 0;
  uint32 offset = 0;
  float defaultValue[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  float minimum = 0.0f;
  float maximum = 0.0f;
};
static_assert(sizeof(ParameterFileData) == 32, "ParameterFileData must have no padding");

// Written per binary after its format name.
struct BinaryFileData
{
  uint32 vertexSize = 0;
  uint32 pixelSize = 0;
};
static_assert(sizeof(BinaryFileData) == 8, "BinaryFileData must have no padding");

#if USING(CH_EDITOR)
constexpr StringView kShaderSection = "Shader";
constexpr StringView kIncludeKeyPrefix = "Include";
#endif // USING(CH_EDITOR)

const SPtr<IShader> kNoShader;

NODISCARD bool
isValidParameter(const ShaderParameter& parameter, uint32 bufferSize)
{
  return parameter.type < ShaderParameterType::COUNT && !parameter.name.empty() &&
         parameter.offset % ShaderParameters::SLOT_SIZE == 0 &&
         parameter.offset + ShaderParameters::SLOT_SIZE <= bufferSize &&
         Math::isFinite(parameter.defaultValue.x) && Math::isFinite(parameter.defaultValue.y) &&
         Math::isFinite(parameter.defaultValue.z) && Math::isFinite(parameter.defaultValue.w) &&
         Math::isFinite(parameter.minimum) && Math::isFinite(parameter.maximum);
}

NODISCARD bool
readCode(DataStream& stream, uint32 size, Vector<uint8>& outCode)
{
  outCode.resize(size);
  return size == 0 || AssetFile::readBytes(stream, outCode.data(), size);
}
} // namespace

/*
 */
ShaderAsset::ShaderAsset(const AssetMetadata& metadata, ShaderAssetData data)
  : IAsset(metadata),
    m_data(std::move(data))
{}

/*
 */
ShaderAsset::~ShaderAsset() = default;

/*
 */
uint32
ShaderAsset::findParameter(const StringView name) const noexcept
{
  for (SIZE_T i = 0; i < m_data.parameters.size(); ++i) {
    if (m_data.parameters[i].name == name) {
      return static_cast<uint32>(i);
    }
  }
  return NOT_FOUND;
}

/*
 */
const ShaderBinary*
ShaderAsset::findBinary(const StringView format) const noexcept
{
  for (const ShaderBinary& binary : m_data.binaries) {
    if (binary.format == format) {
      return &binary;
    }
  }
  return nullptr;
}

/*
 */
const SPtr<IShader>&
ShaderAsset::getPixelShader()
{
  createShaders();
  return m_pixelShader ? m_pixelShader : kNoShader;
}

/*
 */
const SPtr<IShader>&
ShaderAsset::getVertexShader()
{
  createShaders();
  return m_vertexShader ? m_vertexShader : kNoShader;
}

/*
 */
void
ShaderAsset::createShaders()
{
  if (m_bShadersCreated || !IGraphicsAPI::isStarted()) {
    return;
  }
  m_bShadersCreated = true;

  IGraphicsAPI& graphicsAPI = IGraphicsAPI::instance();
  const ShaderBinary* binary = findBinary(graphicsAPI.getShaderBinaryFormat().folder);
  if (!binary || binary->pixelCode.empty()) {
    CH_LOG_ERROR(AssetSystem, "Shader {0} has no code for {1}; import it again.", getName(),
                 graphicsAPI.getAPIName());
    return;
  }
  const String filePath = getFilePath().toString();
  m_pixelShader = graphicsAPI.createShader({.stage = ShaderStage::Fragment,
                                            .entryPoint = "PSMain",
                                            .sourceCode = binary->pixelCode,
                                            .filePath = filePath,
                                            .defines = {}});
  if (!binary->vertexCode.empty()) {
    m_vertexShader = graphicsAPI.createShader({.stage = ShaderStage::Vertex,
                                               .entryPoint = "VSMain",
                                               .sourceCode = binary->vertexCode,
                                               .filePath = filePath,
                                               .defines = {}});
  }
}

#if USING(CH_EDITOR)
/*
 */
void
ShaderAsset::setData(ShaderAssetData data)
{
  m_data = std::move(data);
  m_vertexShader.reset();
  m_pixelShader.reset();
  m_bShadersCreated = false;
  m_compileError.clear();
  ++m_revision;
}

/*
 */
void
ShaderAsset::setIncludedFiles(Vector<Path> files)
{
  if (files.size() > MAX_INCLUDED_FILES) {
    files.resize(MAX_INCLUDED_FILES);
  }
  m_includedFiles = std::move(files);
}

/*
 */
void
ShaderAsset::setCompileError(String error)
{
  m_compileError = std::move(error);
}

/*
 */
void
ShaderAsset::writeEditorValues(ConfigFile& file) const
{
  const Path& projectDirectory = EnginePaths::getProjectDirectory();
  ANSICHAR numberBuffer[StringUtils::MAX_INTEGER_CHARS];
  for (SIZE_T i = 0; i < m_includedFiles.size(); ++i) {
    const Path relative = FileSystem::relativePath(projectDirectory, m_includedFiles[i]);
    const String key = String(kIncludeKeyPrefix) +
                       String(StringUtils::toChars(numberBuffer, static_cast<uint64>(i + 1)));
    file.setValue(kShaderSection, key,
                  relative.empty() ? m_includedFiles[i].toString() : relative.toString());
  }
}

/*
 */
void
ShaderAsset::readEditorValues(const ConfigFile& file)
{
  const Path& projectDirectory = EnginePaths::getProjectDirectory();
  ANSICHAR numberBuffer[StringUtils::MAX_INTEGER_CHARS];
  m_includedFiles.clear();
  for (uint32 i = 1; i <= MAX_INCLUDED_FILES; ++i) {
    const String key = String(kIncludeKeyPrefix) +
                       String(StringUtils::toChars(numberBuffer, static_cast<uint64>(i)));
    const String* text = file.getValue(kShaderSection, key);
    if (!text) {
      break;
    }
    const Path path(*text);
    m_includedFiles.push_back(
        FileSystem::absolutePath(path.isRelative() ? projectDirectory.join(path) : path));
  }
}
#endif // USING(CH_EDITOR)

/*
 */
bool
ShaderAsset::serialize(SPtr<DataStream> stream)
{
  if (!stream || m_data.binaries.empty()) {
    return false;
  }
  const ShaderAssetHeader header{
      .kind = static_cast<uint32>(m_data.kind),
      .stage = static_cast<uint32>(m_data.stage),
      .parameterCount = static_cast<uint32>(m_data.parameters.size()),
      .parameterBufferSize = m_data.parameterBufferSize,
      .binaryCount = static_cast<uint32>(m_data.binaries.size())};
  *stream << header;

  for (const ShaderParameter& parameter : m_data.parameters) {
    AssetFile::writeString(*stream, parameter.name);
    const Vector4& value = parameter.defaultValue;
    const ParameterFileData data{.type = static_cast<uint32>(parameter.type),
                                 .offset = parameter.offset,
                                 .defaultValue = {value.x, value.y, value.z, value.w},
                                 .minimum = parameter.minimum,
                                 .maximum = parameter.maximum};
    *stream << data;
    AssetFile::writeString(*stream, parameter.tooltip);
  }

  for (const ShaderBinary& binary : m_data.binaries) {
    AssetFile::writeString(*stream, binary.format);
    const BinaryFileData data{.vertexSize = static_cast<uint32>(binary.vertexCode.size()),
                              .pixelSize = static_cast<uint32>(binary.pixelCode.size())};
    *stream << data;
    if (stream->write(binary.vertexCode.data(), binary.vertexCode.size()) !=
            binary.vertexCode.size() ||
        stream->write(binary.pixelCode.data(), binary.pixelCode.size()) !=
            binary.pixelCode.size()) {
      return false;
    }
  }
  return true;
}

/*
 */
bool
ShaderAsset::deserialize(SPtr<DataStream> stream)
{
  ShaderAssetHeader header;
  if (!stream || !AssetFile::readValue(*stream, header)) {
    CH_LOG_ERROR(AssetSystem, "Shader {0} is cut short.", getName());
    return false;
  }
  if (header.version != ShaderAssetHeader::VERSION) {
    CH_LOG_ERROR(AssetSystem,
                 "Shader {0} has version {1}, this build reads {2}; import it again.",
                 getName(),
                 header.version,
                 ShaderAssetHeader::VERSION);
    return false;
  }
  if (header.kind >= static_cast<uint32>(ShaderAssetKind::COUNT) ||
      header.stage >= static_cast<uint32>(PostProcessStage::COUNT) ||
      header.parameterCount > ShaderParameters::MAX_PARAMETERS ||
      header.parameterBufferSize > ShaderParameters::MAX_BUFFER_SIZE ||
      header.binaryCount == 0 || header.binaryCount > MAX_BINARIES) {
    CH_LOG_ERROR(AssetSystem, "Shader {0} has broken settings.", getName());
    return false;
  }

  ShaderAssetData data{.kind = static_cast<ShaderAssetKind>(header.kind),
                       .stage = static_cast<PostProcessStage>(header.stage),
                       .parameters = {},
                       .parameterBufferSize = header.parameterBufferSize,
                       .binaries = {}};
  data.parameters.resize(header.parameterCount);
  for (ShaderParameter& parameter : data.parameters) {
    ParameterFileData fileData;
    if (!AssetFile::readString(*stream, parameter.name, ShaderParameters::MAX_NAME_LENGTH) ||
        !AssetFile::readValue(*stream, fileData) ||
        !AssetFile::readString(*stream, parameter.tooltip,
                               ShaderParameters::MAX_TOOLTIP_LENGTH)) {
      CH_LOG_ERROR(AssetSystem, "Shader {0} is cut short.", getName());
      return false;
    }
    parameter.type = static_cast<ShaderParameterType>(fileData.type);
    parameter.offset = fileData.offset;
    parameter.defaultValue = Vector4(fileData.defaultValue[0], fileData.defaultValue[1],
                                     fileData.defaultValue[2], fileData.defaultValue[3]);
    parameter.minimum = fileData.minimum;
    parameter.maximum = fileData.maximum;
    if (fileData.type >= static_cast<uint32>(ShaderParameterType::COUNT) ||
        !isValidParameter(parameter, header.parameterBufferSize)) {
      CH_LOG_ERROR(AssetSystem, "Shader {0} has a broken parameter.", getName());
      return false;
    }
  }

  data.binaries.resize(header.binaryCount);
  for (ShaderBinary& binary : data.binaries) {
    BinaryFileData fileData;
    if (!AssetFile::readString(*stream, binary.format, MAX_FORMAT_LENGTH) ||
        !AssetFile::readValue(*stream, fileData)) {
      CH_LOG_ERROR(AssetSystem, "Shader {0} is cut short.", getName());
      return false;
    }
    const uint64 codeSize = static_cast<uint64>(fileData.vertexSize) + fileData.pixelSize;
    if (fileData.pixelSize == 0 || codeSize > MAX_CODE_SIZE ||
        codeSize > AssetFile::getBytesLeft(*stream) ||
        !readCode(*stream, fileData.vertexSize, binary.vertexCode) ||
        !readCode(*stream, fileData.pixelSize, binary.pixelCode)) {
      CH_LOG_ERROR(AssetSystem, "Shader {0} is cut short.", getName());
      return false;
    }
  }

  m_data = std::move(data);
  m_vertexShader.reset();
  m_pixelShader.reset();
  m_bShadersCreated = false;
  ++m_revision;
  return true;
}

/*
 */
void
ShaderAsset::clearAssetData()
{
  m_data = ShaderAssetData();
  m_vertexShader.reset();
  m_pixelShader.reset();
  m_bShadersCreated = false;
  ++m_revision;
}

} // namespace chEngineSDK
