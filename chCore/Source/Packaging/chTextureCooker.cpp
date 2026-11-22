/************************************************************************/
/**
 * @file chTextureCooker.cpp
 * @author AccelMR
 * @date 2026/11/20
 * @brief
 *  Turns texture pixels into the block compressed formats packaged games load.
 */
/************************************************************************/
#include "chTextureCooker.h"

#if USING(CH_EDITOR)

#include <cmath>
#include <cstring>

#include "chFileStream.h"
#include "chFileSystem.h"
#include "chHash.h"
#include "chJobSystem.h"
#include "chLogger.h"
#include "chMath.h"
#include "chStringUtils.h"

namespace chEngineSDK {
CH_LOG_DECLARE_STATIC(TextureCookerLog, All);

namespace {
constexpr StringView kCompressionNames[] = {"Auto",      "Color",     "ColorNoAlpha",
                                            "NormalMap", "Grayscale", "Uncompressed"};
static_assert(std::size(kCompressionNames) == static_cast<SIZE_T>(TextureCompression::COUNT));

constexpr StringView kQualityNames[] = {"Fast", "Normal", "High"};
static_assert(std::size(kQualityNames) ==
              static_cast<SIZE_T>(TextureCompressionQuality::COUNT));

// "CHTC" read as a little endian number.
constexpr uint32 kCacheMagic = 0x43544843u;

// Blocks a worker takes at a time: big enough to amortize the call, small enough to cancel
// quickly and to keep the scratch buffers on the stack.
constexpr SIZE_T kBlocksPerBatch = 64;

// Fewer blocks than this are not worth a job of their own.
constexpr SIZE_T kMinBlocksPerJob = 256;

// In front of every cache file.
struct CacheFileHeader
{
  uint32 magic = kCacheMagic;
  uint32 version = TextureCooker::VERSION;
  Format format = Format::Unknown;
  uint32 padding = 0;
  uint64 dataSize = 0;
};
static_assert(sizeof(CacheFileHeader) == 24, "CacheFileHeader must have no padding");

// One mip level of a compression: where its pixels and blocks are.
struct LevelRange
{
  SIZE_T firstBlock = 0;
  SIZE_T sourceOffset = 0;
  SIZE_T sourceSize = 0;
  SIZE_T outputOffset = 0;
  uint32 width = 0;
  uint32 height = 0;
  uint32 blocksWide = 0;
};

bool
isRgba8(Format format)
{
  return format == Format::R8G8B8A8_UNORM || format == Format::R8G8B8A8_SRGB ||
         format == Format::B8G8R8A8_UNORM || format == Format::B8G8R8A8_SRGB;
}

bool
isBgra8(Format format)
{
  return format == Format::B8G8R8A8_UNORM || format == Format::B8G8R8A8_SRGB;
}

// The sRGB curve as a table from 8-bit encoded to 8-bit linear, made on first use.
const uint8*
getSrgbToLinearTable()
{
  static const Array<uint8, 256> table = [] {
    Array<uint8, 256> values{};
    for (uint32 i = 0; i < 256; ++i) {
      const double encoded = static_cast<double>(i) / 255.0;
      const double linear = encoded <= 0.04045 ? encoded / 12.92
                                               : std::pow((encoded + 0.055) / 1.055, 2.4);
      values[i] = static_cast<uint8>(std::lround(linear * 255.0));
    }
    return values;
  }();
  return table.data();
}

Format
pickColorFormat(Format source, Format unormFormat, Format srgbFormat)
{
  return FormatUtils::getInfo(source).isSrgb ? srgbFormat : unormFormat;
}

void
compressRange(const ITextureCompressor& compressor,
              const TextureCookSource& source,
              Format target,
              TextureCompressionQuality quality,
              const Vector<LevelRange>& levels,
              SIZE_T firstBlock,
              SIZE_T endBlock,
              uint8* output,
              const Atomic<bool>* cancel)
{
  const SIZE_T blockBytes = FormatUtils::getInfo(target).bytesPerBlock;
  Array<uint8, kBlocksPerBatch * TextureCooker::BLOCK_PIXEL_BYTES> pixels;
  SIZE_T levelIndex = 0;
  SIZE_T block = firstBlock;
  while (block < endBlock) {
    if (cancel && cancel->load(std::memory_order_relaxed)) {
      return;
    }
    // Batches never cross a level, so their blocks are written one after the other.
    while (levelIndex + 1 < levels.size() && levels[levelIndex + 1].firstBlock <= block) {
      ++levelIndex;
    }
    const LevelRange& level = levels[levelIndex];
    const SIZE_T levelEnd = levelIndex + 1 < levels.size() ? levels[levelIndex + 1].firstBlock
                                                           : endBlock;
    const SIZE_T count = Math::min(kBlocksPerBatch, Math::min(endBlock, levelEnd) - block);
    const Span<const uint8> levelPixels(source.data.data() + level.sourceOffset,
                                        level.sourceSize);
    for (SIZE_T i = 0; i < count; ++i) {
      const SIZE_T inLevel = block + i - level.firstBlock;
      TextureCooker::readBlock(levelPixels, source.format, level.width, level.height,
                               static_cast<uint32>(inLevel % level.blocksWide),
                               static_cast<uint32>(inLevel / level.blocksWide), target,
                               pixels.data() + i * TextureCooker::BLOCK_PIXEL_BYTES);
    }
    compressor.compressBlocks(target, pixels.data(), count, quality,
                              output + level.outputOffset +
                                  (block - level.firstBlock) * blockBytes);
    block += count;
  }
}
} // namespace

/*
 */
TextureCookFormat
TextureCooker::resolveFormat(TextureCompression compression,
                             const TextureCookSource& source,
                             bool bUsedAsNormalMap)
{
  if (compression == TextureCompression::Uncompressed) {
    return {.reason = "set to Uncompressed"};
  }
  if (!isRgba8(source.format)) {
    return {.reason = "only 8-bit RGBA textures are compressed"};
  }
  if (source.width % 4 != 0 || source.height % 4 != 0) {
    return {.reason = "its size is not a multiple of 4"};
  }

  switch (compression) {
  case TextureCompression::Color:
    return {.format = pickColorFormat(source.format, Format::BC7_UNORM, Format::BC7_SRGB)};
  case TextureCompression::ColorNoAlpha:
    return {.format =
                pickColorFormat(source.format, Format::BC1_RGBA_UNORM, Format::BC1_RGBA_SRGB)};
  case TextureCompression::NormalMap:
    return {.format = Format::BC5_UNORM};
  case TextureCompression::Grayscale:
    return {.format = Format::BC4_UNORM};
  default:
    break;
  }

  // Auto.
  if (source.mipLevels <= 1) {
    return {.reason = "it has no mips (sprites and UI stay sharp)"};
  }
  if (bUsedAsNormalMap) {
    return {.format = Format::BC5_UNORM};
  }
  // BC4 has no sRGB form, so gray color images keep their curve in BC7.
  if (!FormatUtils::getInfo(source.format).isSrgb && isOpaqueGrayscale(source)) {
    return {.format = Format::BC4_UNORM};
  }
  return {.format = pickColorFormat(source.format, Format::BC7_UNORM, Format::BC7_SRGB)};
}

/*
 */
bool
TextureCooker::isOpaqueGrayscale(const TextureCookSource& source)
{
  if (!isRgba8(source.format)) {
    return false;
  }
  const SIZE_T size = Math::min(source.data.size(),
                                static_cast<SIZE_T>(source.width) * source.height * 4);
  const uint8* pixels = source.data.data();
  for (SIZE_T i = 0; i + 3 < size; i += 4) {
    if (pixels[i] != pixels[i + 1] || pixels[i] != pixels[i + 2] || pixels[i + 3] != 255) {
      return false;
    }
  }
  return size > 0;
}

/*
 */
void
TextureCooker::readBlock(Span<const uint8> level,
                         Format sourceFormat,
                         uint32 width,
                         uint32 height,
                         uint32 blockX,
                         uint32 blockY,
                         Format target,
                         uint8* outPixels)
{
  const bool bSwap = isBgra8(sourceFormat);
  const uint8* toLinear = FormatUtils::getInfo(sourceFormat).isSrgb &&
                                  !FormatUtils::getInfo(target).isSrgb
                              ? getSrgbToLinearTable()
                              : nullptr;
  for (uint32 row = 0; row < 4; ++row) {
    const uint32 y = Math::min(blockY * 4 + row, height - 1);
    for (uint32 column = 0; column < 4; ++column) {
      const uint32 x = Math::min(blockX * 4 + column, width - 1);
      const uint8* pixel = level.data() + (static_cast<SIZE_T>(y) * width + x) * 4;
      uint8* out = outPixels + (row * 4 + column) * 4;
      out[0] = pixel[bSwap ? 2 : 0];
      out[1] = pixel[1];
      out[2] = pixel[bSwap ? 0 : 2];
      out[3] = pixel[3];
      if (toLinear) {
        out[0] = toLinear[out[0]];
        out[1] = toLinear[out[1]];
        out[2] = toLinear[out[2]];
      }
    }
  }
}

/*
 */
bool
TextureCooker::compress(const ITextureCompressor& compressor,
                        const TextureCookSource& source,
                        Format target,
                        TextureCompressionQuality quality,
                        Vector<uint8>& outData,
                        const Atomic<bool>* cancel)
{
  outData.clear();
  if (!isRgba8(source.format) || !FormatUtils::isCompressed(target) ||
      !compressor.supportsFormat(target) || source.width == 0 || source.height == 0 ||
      source.mipLevels == 0) {
    CH_LOG_ERROR(TextureCookerLog, "Cannot compress a texture of format {0} into format {1}.",
                 source.format, target);
    return false;
  }

  Vector<LevelRange> levels;
  levels.reserve(source.mipLevels);
  SIZE_T blockCount = 0;
  SIZE_T sourceOffset = 0;
  SIZE_T outputSize = 0;
  for (uint32 mip = 0; mip < source.mipLevels; ++mip) {
    const uint32 width = Math::max(source.width >> mip, 1u);
    const uint32 height = Math::max(source.height >> mip, 1u);
    const SIZE_T sourceSize = FormatUtils::getMipSize(source.format, width, height);
    const uint32 blocksWide = (width + 3) / 4;
    const uint32 blocksHigh = (height + 3) / 4;
    levels.push_back({.firstBlock = blockCount,
                      .sourceOffset = sourceOffset,
                      .sourceSize = sourceSize,
                      .outputOffset = outputSize,
                      .width = width,
                      .height = height,
                      .blocksWide = blocksWide});
    blockCount += static_cast<SIZE_T>(blocksWide) * blocksHigh;
    sourceOffset += sourceSize;
    outputSize += FormatUtils::getMipSize(target, width, height);
  }
  if (sourceOffset > source.data.size()) {
    CH_LOG_ERROR(TextureCookerLog, "The texture holds {0} bytes but its {1} mips need {2}.",
                 source.data.size(), source.mipLevels, sourceOffset);
    return false;
  }
  outData.resize(outputSize);

  uint8* output = outData.data();
  JobSystem::parallelFor("CompressTexture", blockCount, kMinBlocksPerJob,
                         [&](SIZE_T first, SIZE_T end) {
                           compressRange(compressor, source, target, quality, levels, first,
                                         end, output, cancel);
                         });

  if (cancel && cancel->load(std::memory_order_relaxed)) {
    outData.clear();
    return false;
  }
  return true;
}

/*
 */
uint64
TextureCooker::getCacheKey(const TextureCookSource& source,
                           Format target,
                           TextureCompressionQuality quality)
{
  uint64 hash = HashUtils::hashBytes(source.data.data(), source.data.size());
  hash = HashUtils::combine(hash, source.format);
  hash = HashUtils::combine(hash, source.width);
  hash = HashUtils::combine(hash, source.height);
  hash = HashUtils::combine(hash, source.mipLevels);
  hash = HashUtils::combine(hash, target);
  hash = HashUtils::combine(hash, quality);
  return HashUtils::combine(hash, VERSION);
}

/*
 */
bool
TextureCooker::cook(const ITextureCompressor& compressor,
                    const TextureCookSource& source,
                    Format target,
                    TextureCompressionQuality quality,
                    const Path& cacheFolder,
                    Vector<uint8>& outData,
                    bool* outFromCache,
                    const Atomic<bool>* cancel)
{
  if (outFromCache) {
    *outFromCache = false;
  }

  Array<ANSICHAR, 17> keyText{};
  const uint64 key = getCacheKey(source, target, quality);
  static constexpr ANSICHAR kDigits[] = "0123456789abcdef";
  for (uint32 i = 0; i < 16; ++i) {
    keyText[15 - i] = kDigits[(key >> (i * 4)) & 0xF];
  }
  const Path cacheFile = cacheFolder.join(Path(String(keyText.data()) + ".bin"));

  // A cache file that is cut, of another version or of another format is made again.
  if (FileSystem::isFile(cacheFile)) {
    Vector<uint8> bytes = FileSystem::fastRead(cacheFile);
    CacheFileHeader header;
    if (bytes.size() >= sizeof(header)) {
      memcpy(&header, bytes.data(), sizeof(header));
      if (header.magic == kCacheMagic && header.version == VERSION &&
          header.format == target && header.dataSize == bytes.size() - sizeof(header)) {
        outData.assign(bytes.begin() + sizeof(header), bytes.end());
        if (outFromCache) {
          *outFromCache = true;
        }
        return true;
      }
    }
    CH_LOG_WARNING(TextureCookerLog, "Cache file {0} does not match; compressing again.",
                   cacheFile);
  }

  if (!compress(compressor, source, target, quality, outData, cancel)) {
    return false;
  }

  // A cache that cannot be written only costs the next package the compression.
  const SPtr<DataStream> stream = FileSystem::createAndOpenFile(cacheFile);
  if (!stream) {
    return true;
  }
  const CacheFileHeader header{.format = target, .dataSize = outData.size()};
  if (stream->write(&header, sizeof(header)) != sizeof(header) ||
      stream->write(outData.data(), outData.size()) != outData.size()) {
    stream->close();
    // A half written file is refused by its size anyway; removing it only saves the space.
    CH_PARAMETER_UNUSED(FileSystem::removeFile(cacheFile));
    CH_LOG_WARNING(TextureCookerLog, "Could not write the cache file {0}.", cacheFile);
  }
  return true;
}

/*
 */
StringView
TextureCooker::getCompressionName(TextureCompression compression)
{
  const SIZE_T index = static_cast<SIZE_T>(compression);
  return index < std::size(kCompressionNames) ? kCompressionNames[index] : StringView();
}

/*
 */
bool
TextureCooker::parseCompression(StringView name, TextureCompression& outCompression)
{
  for (SIZE_T i = 0; i < std::size(kCompressionNames); ++i) {
    if (StringUtils::equalsIgnoreCase(name, kCompressionNames[i])) {
      outCompression = static_cast<TextureCompression>(i);
      return true;
    }
  }
  return false;
}

/*
 */
StringView
TextureCooker::getQualityName(TextureCompressionQuality quality)
{
  const SIZE_T index = static_cast<SIZE_T>(quality);
  return index < std::size(kQualityNames) ? kQualityNames[index] : StringView();
}

/*
 */
bool
TextureCooker::parseQuality(StringView name, TextureCompressionQuality& outQuality)
{
  for (SIZE_T i = 0; i < std::size(kQualityNames); ++i) {
    if (StringUtils::equalsIgnoreCase(name, kQualityNames[i])) {
      outQuality = static_cast<TextureCompressionQuality>(i);
      return true;
    }
  }
  return false;
}

} // namespace chEngineSDK

#endif // USING(CH_EDITOR)
