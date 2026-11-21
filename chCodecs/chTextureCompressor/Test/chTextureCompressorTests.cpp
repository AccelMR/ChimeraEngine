/************************************************************************/
/**
 * @file chTextureCompressorTests.cpp
 * @author AccelMR
 * @date 2026/11/20
 * @brief Compresses images with every BC format, reads them back and measures the error.
 */
/************************************************************************/
#include <chrono>
#include <cmath>
#include <cstring>

#include "chBcTextureCompressor.h"
#include "chMath.h"

#include "bc7decomp.h"
#include "rgbcx.h"

#include "catch.hpp"

using namespace chEngineSDK;

namespace {
constexpr uint32 kSize = 64;

// Smooth gradients with a little deterministic noise in every channel, alpha included.
Vector<uint8>
makeImage()
{
  Vector<uint8> pixels(kSize * kSize * 4);
  uint32 noise = 12345;
  for (uint32 y = 0; y < kSize; ++y) {
    for (uint32 x = 0; x < kSize; ++x) {
      uint8* pixel = pixels.data() + (y * kSize + x) * 4;
      for (uint32 channel = 0; channel < 4; ++channel) {
        noise = noise * 1664525u + 1013904223u;
        const float wave = 0.5f + 0.5f * std::sin(static_cast<float>(x * (channel + 1)) * 0.05f +
                                                  static_cast<float>(y) * 0.07f);
        const int32 value = static_cast<int32>(wave * 230.0f) + static_cast<int32>(noise >> 29);
        pixel[channel] = static_cast<uint8>(Math::clamp(value, 0, 255));
      }
    }
  }
  return pixels;
}

// Decodes every block of a 64x64 level back to RGBA.
Vector<uint8>
decode(Format format, const Vector<uint8>& blocks)
{
  Vector<uint8> pixels(kSize * kSize * 4, 0);
  const SIZE_T blockBytes = FormatUtils::getInfo(format).bytesPerBlock;
  const uint32 blocksWide = kSize / 4;
  for (uint32 block = 0; block < blocksWide * blocksWide; ++block) {
    uint8 decoded[16 * 4] = {};
    const uint8* bits = blocks.data() + block * blockBytes;
    switch (format) {
    case Format::BC1_RGBA_UNORM:
      rgbcx::unpack_bc1(bits, decoded);
      break;
    case Format::BC4_UNORM:
      rgbcx::unpack_bc4(bits, decoded);
      break;
    case Format::BC5_UNORM:
      rgbcx::unpack_bc5(bits, decoded);
      break;
    default:
      bc7decomp::unpack_bc7(bits, reinterpret_cast<bc7decomp::color_rgba*>(decoded));
      break;
    }
    const uint32 blockX = block % blocksWide;
    const uint32 blockY = block / blocksWide;
    for (uint32 row = 0; row < 4; ++row) {
      memcpy(pixels.data() + ((blockY * 4 + row) * kSize + blockX * 4) * 4,
             decoded + row * 16, 16);
    }
  }
  return pixels;
}

// Peak signal to noise ratio in decibels over the first channelCount channels.
double
measurePsnr(const Vector<uint8>& source, const Vector<uint8>& decoded, uint32 channelCount)
{
  double squaredError = 0.0;
  for (SIZE_T i = 0; i < source.size(); i += 4) {
    for (uint32 channel = 0; channel < channelCount; ++channel) {
      const double difference =
          static_cast<double>(source[i + channel]) - static_cast<double>(decoded[i + channel]);
      squaredError += difference * difference;
    }
  }
  const double meanError = squaredError / static_cast<double>(kSize * kSize * channelCount);
  return meanError <= 0.0 ? 100.0 : 10.0 * std::log10(255.0 * 255.0 / meanError);
}

double
compressAndMeasure(const BcTextureCompressor& compressor,
                   const Vector<uint8>& image,
                   Format format,
                   TextureCompressionQuality quality,
                   uint32 channelCount)
{
  const TextureCookSource source{.format = Format::R8G8B8A8_UNORM,
                                 .width = kSize,
                                 .height = kSize,
                                 .mipLevels = 1,
                                 .data = image};
  Vector<uint8> blocks;
  if (!TextureCooker::compress(compressor, source, format, quality, blocks)) {
    return 0.0;
  }
  return measurePsnr(image, decode(format, blocks), channelCount);
}
} // namespace

TEST_CASE("chTextureCompressor - Formats", "[TextureCompression]")
{
  const BcTextureCompressor compressor;
  REQUIRE(compressor.supportsFormat(Format::BC7_SRGB));
  REQUIRE(compressor.supportsFormat(Format::BC5_UNORM));
  REQUIRE_FALSE(compressor.supportsFormat(Format::R8G8B8A8_UNORM));
}

TEST_CASE("chTextureCompressor - Quality", "[TextureCompression]")
{
  const BcTextureCompressor compressor;
  const Vector<uint8> image = makeImage();

  // Lower bounds well under what each format gives on this image, so a broken encoder or
  // block order fails while a better encoder version still passes.
  struct Expectation
  {
    Format format;
    uint32 channels;
    double minimumPsnr;
  };
  const Expectation expectations[] = {{Format::BC1_RGBA_UNORM, 3, 30.0},
                                      {Format::BC4_UNORM, 1, 38.0},
                                      {Format::BC5_UNORM, 2, 38.0},
                                      {Format::BC7_UNORM, 4, 36.0}};
  for (const Expectation& expectation : expectations) {
    double previous = 0.0;
    for (uint32 q = 0; q < static_cast<uint32>(TextureCompressionQuality::COUNT); ++q) {
      const double psnr =
          compressAndMeasure(compressor, image, expectation.format,
                             static_cast<TextureCompressionQuality>(q), expectation.channels);
      INFO("format " << static_cast<uint32>(expectation.format) << ", quality " << q
                     << ": " << psnr << " dB");
      REQUIRE(psnr >= expectation.minimumPsnr);
      // A slower setting never looks clearly worse.
      REQUIRE(psnr >= previous - 0.25);
      previous = psnr;
    }
  }
}

TEST_CASE("chTextureCompressor - Solid blocks", "[TextureCompression]")
{
  const BcTextureCompressor compressor;
  Vector<uint8> image(kSize * kSize * 4);
  for (SIZE_T i = 0; i < image.size(); i += 4) {
    image[i] = 255;
    image[i + 1] = 0;
    image[i + 2] = 0;
    image[i + 3] = 255;
  }
  // BC7 keeps its endpoints at 7 bits plus a shared bit, so pure red comes back a step off.
  REQUIRE(compressAndMeasure(compressor, image, Format::BC7_UNORM,
                             TextureCompressionQuality::Fast, 4) >= 48.0);
  REQUIRE(compressAndMeasure(compressor, image, Format::BC1_RGBA_UNORM,
                             TextureCompressionQuality::Fast, 3) >= 60.0);
  REQUIRE(compressAndMeasure(compressor, image, Format::BC5_UNORM,
                             TextureCompressionQuality::Fast, 2) >= 60.0);
}

TEST_CASE("chTextureCompressor - Benchmark", "[TextureCompression][.benchmark]")
{
  // A 2048x2048 image with its whole mip chain, like a big color texture of a model.
  constexpr uint32 kBigSize = 2048;
  Vector<uint8> image;
  uint32 mipLevels = 0;
  uint32 noise = 777;
  for (uint32 size = kBigSize; size > 0; size /= 2, ++mipLevels) {
    for (uint32 y = 0; y < size; ++y) {
      for (uint32 x = 0; x < size; ++x) {
        noise = noise * 1664525u + 1013904223u;
        const float wave = 0.5f + 0.5f * std::sin(static_cast<float>(x) * 0.013f +
                                                  static_cast<float>(y) * 0.021f);
        const uint8 value = static_cast<uint8>(wave * 220.0f) + static_cast<uint8>(noise >> 28);
        image.insert(image.end(), {value, static_cast<uint8>(255 - value),
                                   static_cast<uint8>(value / 2), 255});
      }
    }
  }
  const TextureCookSource source{.format = Format::R8G8B8A8_SRGB,
                                 .width = kBigSize,
                                 .height = kBigSize,
                                 .mipLevels = mipLevels,
                                 .data = image};
  const BcTextureCompressor compressor;
  const Format formats[] = {Format::BC7_SRGB, Format::BC1_RGBA_SRGB, Format::BC5_UNORM};
  for (const Format format : formats) {
    for (uint32 q = 0; q < static_cast<uint32>(TextureCompressionQuality::COUNT); ++q) {
      Vector<uint8> output;
      const auto start = std::chrono::steady_clock::now();
      REQUIRE(TextureCooker::compress(compressor, source, format,
                                      static_cast<TextureCompressionQuality>(q), output));
      const double seconds =
          std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
      WARN("format " << static_cast<uint32>(format) << ", quality " << q << ": " << seconds
                     << " s for 2048x2048 with mips");
    }
  }
}
