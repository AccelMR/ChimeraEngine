/************************************************************************/
/**
 * @file chTextureAsset.cpp
 * @author AccelMR
 * @date 2025/07/20
 * @brief
 */
/************************************************************************/
#include "chTextureAsset.h"

#include <cstring>

#include "chIGraphicsAPI.h"
#include "chFileStream.h"
#include "chLogger.h"
#include "chMath.h"
#include "chTextureMips.h"

#if USING(CH_EDITOR)
#include "chConfigFile.h"
#include "chTextureCooker.h"
#endif // USING(CH_EDITOR)

namespace chEngineSDK {
CH_LOG_DECLARE_STATIC(TextureAssetLog, All);

namespace {
#if USING(CH_EDITOR)
constexpr StringView kTextureSection = "Texture";
constexpr StringView kCompressionKey = "Compression";
#endif // USING(CH_EDITOR)

SIZE_T
getChainSize(Format format, uint32 width, uint32 height, uint32 mipLevels)
{
  SIZE_T size = 0;
  for (uint32 mip = 0; mip < mipLevels; ++mip) {
    size += FormatUtils::getMipSize(format, Math::max(width >> mip, 1u),
                                    Math::max(height >> mip, 1u));
  }
  return size;
}
} // namespace

/*
 */
bool
TextureAsset::isValidSize(Format format, uint32 width, uint32 height) noexcept
{
  if (width == 0 || height == 0) {
    return false;
  }
  const FormatInfo& info = FormatUtils::getInfo(format);
  return width % info.blockWidth == 0 && height % info.blockHeight == 0;
}

/*
 */
TextureAsset::TextureAsset(const AssetMetadata& metadata,
                           Vector<uint8> textureData,
                           uint32 width,
                           uint32 height,
                           Format format,
                           uint32 mipLevels)
  : IAsset(metadata),
    m_textureData(std::move(textureData)),
    m_width(width),
    m_height(height),
    m_format(format),
    m_mipLevels(mipLevels)
{
  CH_ASSERT(m_textureData.size() == getChainSize(format, width, height, mipLevels));
  CH_ASSERT(isValidSize(format, width, height));
  createTextureFromData();
}

#if USING(CH_EDITOR)
/*
 */
Span<const uint8>
TextureAsset::getRgba8Pixels() const
{
  const bool bRgba8 = m_format == Format::R8G8B8A8_UNORM || m_format == Format::R8G8B8A8_SRGB;
  const SIZE_T size = static_cast<SIZE_T>(m_width) * m_height * 4;
  if (!bRgba8 || m_textureData.size() < size) {
    return {};
  }
  return Span<const uint8>(m_textureData.data(), size);
}

/*
 */
bool
TextureAsset::hasAlphaBelow(float alpha) const
{
  if (m_format != Format::R8G8B8A8_UNORM && m_format != Format::R8G8B8A8_SRGB &&
      m_format != Format::B8G8R8A8_UNORM && m_format != Format::B8G8R8A8_SRGB) {
    return false;
  }

  // Alpha is linear in every one of these formats, sRGB included.
  const float threshold = Math::clamp(alpha, 0.0f, 1.0f) * 255.0f;
  const SIZE_T levelSize = Math::min(m_textureData.size(),
                                     static_cast<SIZE_T>(m_width) * m_height * 4);
  for (SIZE_T i = 3; i < levelSize; i += 4) {
    if (static_cast<float>(m_textureData[i]) < threshold) {
      return true;
    }
  }
  return false;
}

/*
 */
void
TextureAsset::writeEditorValues(ConfigFile& file) const
{
  if (m_compression != TextureCompression::Auto) {
    file.setValue(kTextureSection, kCompressionKey,
                  TextureCooker::getCompressionName(m_compression));
  }
}

/*
 */
void
TextureAsset::readEditorValues(const ConfigFile& file)
{
  const String* value = file.getValue(kTextureSection, kCompressionKey);
  if (value && !TextureCooker::parseCompression(*value, m_compression)) {
    CH_LOG_WARNING(TextureAssetLog, "Texture {0}: unknown compression '{1}', using Auto.",
                   getName(), *value);
    m_compression = TextureCompression::Auto;
  }
}
#endif // USING(CH_EDITOR)

/*
 */
bool
TextureAsset::serialize(SPtr<DataStream> stream) {
  if (!stream) {
    CH_LOG_ERROR(TextureAssetLog, "Failed to serialize texture asset: stream is null");
    return false;
  }

  if (m_textureData.empty()) {
    CH_LOG_ERROR(TextureAssetLog, "Failed to serialize texture asset: texture data is empty");
    return false;
  }
  const TextureAssetHeader header{.width = m_width,
                                  .height = m_height,
                                  .format = m_format,
                                  .mipLevels = m_mipLevels};
  *stream << header;

  stream->write(m_textureData.data(), m_textureData.size());
  CH_LOG_DEBUG(TextureAssetLog, "Serialized texture asset {0} with size {1}",
                                 getName(),
                                 m_textureData.size() * sizeof(uint8));
  return true;
}

/*
 */
bool
TextureAsset::deserialize(SPtr<DataStream> stream) {
  if (!stream) {
    CH_LOG_ERROR(TextureAssetLog, "Failed to deserialize texture asset: stream is null");
    return false;
  }

  // Deserialize texture data
  m_textureData.clear();

  TextureAssetHeader header;
  if (stream->read(&header, sizeof(header)) != sizeof(header) ||
      header.version != TextureAssetHeader::VERSION) {
    CH_LOG_ERROR(TextureAssetLog,
                 "Failed to deserialize texture asset {0}: bad header or old version, import "
                 "it again",
                 getName());
    return false;
  }
  // The values come from the file, so they are checked before they index the format table
  // or size the data.
  if (static_cast<uint32>(header.format) >= static_cast<uint32>(Format::COUNT) ||
      header.mipLevels == 0 ||
      header.mipLevels > TextureMips::getMipCount(header.width, header.height)) {
    CH_LOG_ERROR(TextureAssetLog,
                 "Failed to deserialize texture asset {0}: format {1} with {2} mips",
                 getName(), header.format, header.mipLevels);
    return false;
  }
  if (!isValidSize(header.format, header.width, header.height)) {
    CH_LOG_ERROR(TextureAssetLog,
                 "Failed to deserialize texture asset {0}: {1}x{2} is not a size its format "
                 "{3} allows",
                 getName(), header.width, header.height, header.format);
    return false;
  }
  m_width = header.width;
  m_height = header.height;
  m_format = header.format;
  m_mipLevels = header.mipLevels;
  m_textureData.resize(getChainSize(m_format, m_width, m_height, m_mipLevels));
  if (m_textureData.empty()) {
    CH_LOG_ERROR(TextureAssetLog, "Failed to deserialize texture asset: texture data is empty");
    return false;
  }
  if (stream->read(m_textureData.data(), m_textureData.size()) != m_textureData.size()) {
    CH_LOG_ERROR(TextureAssetLog, "Failed to read texture data from stream");
    return false;
  }

  createTextureFromData();

  return true;
}

/*
 */
bool
TextureAsset::readAsync(Vector<uint8>& fileBytes, SIZE_T dataOffset, String& outError)
{
  TextureAssetHeader header;
  if (fileBytes.size() < dataOffset + sizeof(header)) {
    outError = "the texture is cut short";
    return false;
  }
  memcpy(&header, fileBytes.data() + dataOffset, sizeof(header));
  if (header.version != TextureAssetHeader::VERSION ||
      static_cast<uint32>(header.format) >= static_cast<uint32>(Format::COUNT) ||
      header.mipLevels == 0 ||
      header.mipLevels > TextureMips::getMipCount(header.width, header.height) ||
      !isValidSize(header.format, header.width, header.height)) {
    outError = "bad header or old version; import it again";
    return false;
  }
  const SIZE_T pixelStart = dataOffset + sizeof(header);
  const SIZE_T chainSize = getChainSize(header.format, header.width, header.height,
                                        header.mipLevels);
  if (chainSize == 0 || fileBytes.size() - pixelStart < chainSize) {
    outError = "the texture data is cut short";
    return false;
  }
  // The file's own buffer becomes the pixels: moving the start out costs one pass on this
  // thread instead of a second copy of the whole image.
  fileBytes.erase(fileBytes.begin(), fileBytes.begin() + static_cast<int64>(pixelStart));
  fileBytes.resize(chainSize);
  m_pendingPixels = std::move(fileBytes);
  m_pendingHeader = header;
  return true;
}

/*
 */
bool
TextureAsset::finishAsyncRead()
{
  m_width = m_pendingHeader.width;
  m_height = m_pendingHeader.height;
  m_format = m_pendingHeader.format;
  m_mipLevels = m_pendingHeader.mipLevels;
  m_textureData = std::move(m_pendingPixels);
  m_pendingPixels = {};
  createTextureFromData();
  return true;
}

/*
 */
void
TextureAsset::clearAssetData() {
  m_textureData.clear();
}

/*
*/
void
TextureAsset::createTextureFromData() {
  if (m_textureData.empty()) {
    CH_LOG_ERROR(TextureAssetLog, "Cannot create texture: texture data is empty");
    return;
  }
  if (!m_texture) {
    IGraphicsAPI& graphicsAPI = IGraphicsAPI::instance();
    const TextureCreateInfo textureCreateInfo{.type = TextureType::Texture2D,
                                              .format = m_format,
                                              .width = m_width,
                                              .height = m_height,
                                              .depth = 1,
                                              .mipLevels = m_mipLevels,
                                              .arrayLayers = 1,
                                              .samples = SampleCount::Count1,
                                              .usage = TextureUsage::Sampled |
                                                       TextureUsage::TransferDst,
                                              .initialData = m_textureData.data(),
                                              .initialDataSize = m_textureData.size()};
    m_texture = graphicsAPI.createTexture(textureCreateInfo);
  }

#if !USING(CH_EDITOR)
  // The upload copied the pixels into staging memory, and only editor tools read them after
  // that (saving, alpha scans), so the game gives the memory back.
  Vector<uint8>().swap(m_textureData);
#endif
}

} // namespace chEngineSDK
