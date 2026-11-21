/************************************************************************/
/**
 * @file chTextureCooker.h
 * @author AccelMR
 * @date 2026/11/20
 * @brief
 *  Turns texture pixels into the block compressed formats packaged games load.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#if USING(CH_EDITOR)

#include "chGraphicsTypes.h"
#include "chPath.h"
#include "chSTDThreading.h"
#include "chTextureAsset.h"

namespace chEngineSDK {

enum class TextureCompressionQuality : uint32
{
  Fast = 0,
  Normal = 1,
  High = 2,

  COUNT
};

/**
 * Turns 4x4 blocks of RGBA8 pixels into blocks of a BC format. A codec plugin implements it,
 * so the encoder never reaches chCore or a packaged game.
 */
class CH_CORE_EXPORT ITextureCompressor
{
 public:
  virtual ~ITextureCompressor() = default;

  NODISCARD virtual bool
  supportsFormat(Format format) const = 0;

  /**
   * blocks holds blockCount blocks of TextureCooker::BLOCK_PIXEL_BYTES (16 RGBA pixels, rows
   * top to bottom); output receives blockCount blocks of the format. Called from several
   * threads at once.
   */
  virtual void
  compressBlocks(Format format,
                 const uint8* blocks,
                 SIZE_T blockCount,
                 TextureCompressionQuality quality,
                 uint8* output) const = 0;
};

/**
 * The pixels of a texture asset: every mip level one after the other, largest first.
 */
struct TextureCookSource
{
  Format format = Format::Unknown;
  uint32 width = 0;
  uint32 height = 0;
  uint32 mipLevels = 0;
  Span<const uint8> data;
};

/**
 * The format a texture is packaged in, and why it stays as it is when it does.
 */
struct TextureCookFormat
{
  // Unknown when the texture is packaged as it is.
  Format format = Format::Unknown;
  // Why it stays as it is; empty when it is compressed.
  StringView reason = {};
};

/**
 * Picks the compressed format of each texture and compresses its whole mip chain, keeping
 * the results in a cache folder so packaging again only compresses what changed.
 */
class CH_CORE_EXPORT TextureCooker
{
 public:
  /**
   * Picks the packaged format. bUsedAsNormalMap tells Auto that a material reads the texture
   * as its normal map.
   */
  NODISCARD static TextureCookFormat
  resolveFormat(TextureCompression compression,
                const TextureCookSource& source,
                bool bUsedAsNormalMap);

  /**
   * Whether every pixel of the largest level has equal red, green and blue and full alpha.
   */
  NODISCARD static bool
  isOpaqueGrayscale(const TextureCookSource& source);

  /**
   * Compresses every level of source into target, on every core. False when the source is
   * not 8-bit RGBA or BGRA, the compressor lacks the format, or cancel became true.
   */
  NODISCARD static bool
  compress(const ITextureCompressor& compressor,
           const TextureCookSource& source,
           Format target,
           TextureCompressionQuality quality,
           Vector<uint8>& outData,
           const Atomic<bool>* cancel = nullptr);

  /**
   * compress through a cache folder: a result made before from the same pixels, target and
   * quality is read back instead. bFromCache, when given, tells which one happened.
   */
  NODISCARD static bool
  cook(const ITextureCompressor& compressor,
       const TextureCookSource& source,
       Format target,
       TextureCompressionQuality quality,
       const Path& cacheFolder,
       Vector<uint8>& outData,
       bool* outFromCache = nullptr,
       const Atomic<bool>* cancel = nullptr);

  /**
   * The 16 pixels of the block at (blockX, blockY) of one level as RGBA, repeating the last
   * row and column where the level ends inside the block. BGRA turns into RGBA, and an sRGB
   * source going to a linear target is turned linear, so it looks the same.
   */
  static void
  readBlock(Span<const uint8> level,
            Format sourceFormat,
            uint32 width,
            uint32 height,
            uint32 blockX,
            uint32 blockY,
            Format target,
            uint8* outPixels);

  NODISCARD static uint64
  getCacheKey(const TextureCookSource& source, Format target, TextureCompressionQuality quality);

  NODISCARD static StringView
  getCompressionName(TextureCompression compression);

  NODISCARD static bool
  parseCompression(StringView name, TextureCompression& outCompression);

  NODISCARD static StringView
  getQualityName(TextureCompressionQuality quality);

  NODISCARD static bool
  parseQuality(StringView name, TextureCompressionQuality& outQuality);

  // Raise it whenever the same input would give other bytes, so old cache files are skipped.
  static constexpr uint32 VERSION = 1;
  static constexpr uint32 BLOCK_PIXEL_BYTES = 16 * 4;
};

} // namespace chEngineSDK

#endif // USING(CH_EDITOR)
