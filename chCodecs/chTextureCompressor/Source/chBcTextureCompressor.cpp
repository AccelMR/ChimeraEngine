/************************************************************************/
/**
 * @file chBcTextureCompressor.cpp
 * @author AccelMR
 * @date 2026/11/20
 * @brief
 *  BC1, BC4, BC5 and BC7 encoding with bc7enc_rdo.
 */
/************************************************************************/
#include "chBcTextureCompressor.h"

#include "bc7enc.h"
#include "rgbcx.h"

namespace chEngineSDK {

namespace {
// rgbcx BC1 levels go from 0 to 18; higher is slower and better.
constexpr uint32 kBc1Levels[] = {4, 10, 18};
// bc7enc uber levels go from 0 to 4, partitions from 0 to 64.
constexpr uint32 kBc7UberLevels[] = {0, 1, 4};
constexpr uint32 kBc7Partitions[] = {16, 64, 64};
static_assert(std::size(kBc1Levels) == static_cast<SIZE_T>(TextureCompressionQuality::COUNT));

// Both libraries fill tables once before any block is encoded.
void
initializeEncoders()
{
  static const bool bInitialized = [] {
    rgbcx::init();
    bc7enc_compress_block_init();
    return true;
  }();
  CH_PARAMETER_UNUSED(bInitialized);
}
} // namespace

/*
 */
BcTextureCompressor::BcTextureCompressor()
{
  initializeEncoders();
}

/*
 */
bool
BcTextureCompressor::supportsFormat(Format format) const
{
  switch (format) {
  case Format::BC1_RGBA_UNORM:
  case Format::BC1_RGBA_SRGB:
  case Format::BC4_UNORM:
  case Format::BC5_UNORM:
  case Format::BC7_UNORM:
  case Format::BC7_SRGB:
    return true;
  default:
    return false;
  }
}

/*
 */
void
BcTextureCompressor::compressBlocks(Format format,
                                    const uint8* blocks,
                                    SIZE_T blockCount,
                                    TextureCompressionQuality quality,
                                    uint8* output) const
{
  const SIZE_T qualityIndex = static_cast<SIZE_T>(quality);
  const SIZE_T blockBytes = FormatUtils::getInfo(format).bytesPerBlock;
  const bool bHigh = quality == TextureCompressionQuality::High;

  bc7enc_compress_block_params bc7Params;
  if (format == Format::BC7_UNORM || format == Format::BC7_SRGB) {
    bc7enc_compress_block_params_init(&bc7Params);
    // Perceptual weights favor what the eye sees in color images; linear data such as
    // masks wants every channel weighed the same.
    if (format == Format::BC7_UNORM) {
      bc7enc_compress_block_params_init_linear_weights(&bc7Params);
    }
    bc7Params.m_uber_level = kBc7UberLevels[qualityIndex];
    bc7Params.m_max_partitions = kBc7Partitions[qualityIndex];
  }

  for (SIZE_T i = 0; i < blockCount; ++i) {
    const uint8* pixels = blocks + i * TextureCooker::BLOCK_PIXEL_BYTES;
    uint8* block = output + i * blockBytes;
    switch (format) {
    case Format::BC1_RGBA_UNORM:
    case Format::BC1_RGBA_SRGB:
      // Opaque blocks only: the three color mode would turn some texels transparent.
      rgbcx::encode_bc1(kBc1Levels[qualityIndex], block, pixels, false, false);
      break;
    case Format::BC4_UNORM:
      if (bHigh) {
        rgbcx::encode_bc4_hq(block, pixels);
      }
      else {
        rgbcx::encode_bc4(block, pixels);
      }
      break;
    case Format::BC5_UNORM:
      if (bHigh) {
        rgbcx::encode_bc5_hq(block, pixels);
      }
      else {
        rgbcx::encode_bc5(block, pixels);
      }
      break;
    case Format::BC7_UNORM:
    case Format::BC7_SRGB:
      bc7enc_compress_block(block, pixels, &bc7Params);
      break;
    default:
      return;
    }
  }
}

} // namespace chEngineSDK
