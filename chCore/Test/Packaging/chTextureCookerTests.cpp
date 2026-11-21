/************************************************************************/
/**
 * @file chTextureCookerTests.cpp
 * @author AccelMR
 * @date 2026/11/20
 * @brief Tests of the texture format choice, the block reading and the cook cache.
 */
/************************************************************************/
#include "chPrerequisitesCore.h"

#if USING(CH_EDITOR)

#include "chAssetCooker.h"
#include "chFileStream.h"
#include "chFileSystem.h"
#include "chMaterial.h"
#include "chMaterialAsset.h"
#include "chMath.h"
#include "chProjectPackager.h"
#include "chTextureCooker.h"
#include "chTextureMips.h"

#include "catch.hpp"

using namespace chEngineSDK;

namespace {
// Writes, per block, the red of its first and last pixel and the call's quality, so the
// tests see which pixels reached which block. Counts its calls.
class FakeCompressor : public ITextureCompressor
{
 public:
  explicit FakeCompressor(bool bAllFormats = false) : m_bAllFormats(bAllFormats) {}

  bool
  supportsFormat(Format format) const override
  {
    return format == Format::BC4_UNORM || (m_bAllFormats && FormatUtils::isCompressed(format));
  }

  void
  compressBlocks(Format format,
                 const uint8* blocks,
                 SIZE_T blockCount,
                 TextureCompressionQuality quality,
                 uint8* output) const override
  {
    calls.fetch_add(1);
    const SIZE_T blockBytes = FormatUtils::getInfo(format).bytesPerBlock;
    for (SIZE_T i = 0; i < blockCount; ++i) {
      const uint8* pixels = blocks + i * TextureCooker::BLOCK_PIXEL_BYTES;
      uint8* block = output + i * blockBytes;
      block[0] = pixels[0];
      block[1] = pixels[15 * 4];
      block[2] = static_cast<uint8>(quality);
      for (SIZE_T b = 3; b < blockBytes; ++b) {
        block[b] = 0xAB;
      }
    }
  }

  mutable Atomic<uint32> calls{0};

 private:
  bool m_bAllFormats = false;
};

// A chain of 8-bit RGBA levels whose red is 40 times the level plus the block index of the
// pixel, so every block of every level has its own value.
Vector<uint8>
makeChain(uint32 width, uint32 height, uint32 mipLevels)
{
  Vector<uint8> data;
  for (uint32 mip = 0; mip < mipLevels; ++mip) {
    const uint32 levelWidth = Math::max(width >> mip, 1u);
    const uint32 levelHeight = Math::max(height >> mip, 1u);
    const uint32 blocksWide = (levelWidth + 3) / 4;
    for (uint32 y = 0; y < levelHeight; ++y) {
      for (uint32 x = 0; x < levelWidth; ++x) {
        const uint32 block = (y / 4) * blocksWide + x / 4;
        data.push_back(static_cast<uint8>(mip * 40 + block));
        data.push_back(0);
        data.push_back(0);
        data.push_back(255);
      }
    }
  }
  return data;
}
void
writeTestFile(const Path& path, StringView text)
{
  SPtr<DataStream> stream = FileSystem::createAndOpenFile(path);
  REQUIRE(stream);
  stream->write(text.data(), text.size());
  stream->close();
}

// A texture asset file of gray pixels with a full mip chain (or one level), as the editor
// saves it.
void
writeTestTexture(const Path& path,
                 const chEngineSDK::UUID& uuid,
                 Format format,
                 uint32 size,
                 bool bMips,
                 StringView editorData)
{
  const TextureAssetHeader header{.width = size,
                                  .height = size,
                                  .format = format,
                                  .mipLevels = bMips ? TextureMips::getMipCount(size, size) : 1};
  SIZE_T pixelBytes = 0;
  for (uint32 mip = 0; mip < header.mipLevels; ++mip) {
    pixelBytes += FormatUtils::getMipSize(format, Math::max(size >> mip, 1u),
                                          Math::max(size >> mip, 1u));
  }
  const Vector<uint8> pixels(pixelBytes, 120);
  SPtr<DataStream> stream = FileSystem::createAndOpenFile(path);
  REQUIRE(stream);
  AssetFile::writeStart(*stream, uuid, TypeTraits<TextureAsset>::getTypeId(), {}, editorData);
  stream->write(&header, sizeof(header));
  stream->write(pixels.data(), pixels.size());
  stream->close();
}

TextureAssetHeader
readPackagedTextureHeader(const Path& path)
{
  TextureAssetHeader header;
  SPtr<DataStream> stream = FileSystem::openFile(path);
  REQUIRE(stream);
  AssetFileInfo info;
  Vector<chEngineSDK::UUID> references;
  String editorData;
  REQUIRE(AssetFile::readInfo(*stream, info));
  REQUIRE(AssetFile::readReferences(*stream, references));
  REQUIRE(AssetFile::readEditorData(*stream, &editorData));
  REQUIRE(editorData.empty());
  REQUIRE(AssetFile::readValue(*stream, header));
  // What follows is exactly the chain of the packaged format.
  SIZE_T expected = 0;
  for (uint32 mip = 0; mip < header.mipLevels; ++mip) {
    expected += FormatUtils::getMipSize(header.format, Math::max(header.width >> mip, 1u),
                                        Math::max(header.height >> mip, 1u));
  }
  REQUIRE(AssetFile::getBytesLeft(*stream) == expected);
  return header;
}
} // namespace

TEST_CASE("chCore - Texture cook format", "[Packaging]")
{
  // Gray and opaque.
  Vector<uint8> pixels(16 * 16 * 4, 200);
  for (SIZE_T i = 3; i < pixels.size(); i += 4) {
    pixels[i] = 255;
  }
  TextureCookSource source{.format = Format::R8G8B8A8_SRGB,
                           .width = 16,
                           .height = 16,
                           .mipLevels = 5,
                           .data = pixels};

  SECTION("Auto picks by use, color space and content")
  {
    REQUIRE(TextureCooker::resolveFormat(TextureCompression::Auto, source, false).format ==
            Format::BC7_SRGB);
    REQUIRE(TextureCooker::resolveFormat(TextureCompression::Auto, source, true).format ==
            Format::BC5_UNORM);

    // Gray and opaque: BC4 only when linear, since BC4 has no sRGB form.
    REQUIRE(TextureCooker::isOpaqueGrayscale(source));
    source.format = Format::R8G8B8A8_UNORM;
    REQUIRE(TextureCooker::resolveFormat(TextureCompression::Auto, source, false).format ==
            Format::BC4_UNORM);
    pixels[1] = 10;
    REQUIRE_FALSE(TextureCooker::isOpaqueGrayscale(source));
    REQUIRE(TextureCooker::resolveFormat(TextureCompression::Auto, source, false).format ==
            Format::BC7_UNORM);
  }

  SECTION("Auto keeps textures without mips, sizes off the blocks and floats")
  {
    source.mipLevels = 1;
    const TextureCookFormat noMips =
        TextureCooker::resolveFormat(TextureCompression::Auto, source, false);
    REQUIRE(noMips.format == Format::Unknown);
    REQUIRE_FALSE(noMips.reason.empty());
    // Chosen by hand, a texture without mips is compressed.
    REQUIRE(TextureCooker::resolveFormat(TextureCompression::Color, source, false).format ==
            Format::BC7_SRGB);

    source.mipLevels = 5;
    source.width = 18;
    REQUIRE(TextureCooker::resolveFormat(TextureCompression::Color, source, false).format ==
            Format::Unknown);

    source.width = 16;
    source.format = Format::R16G16B16A16_SFLOAT;
    REQUIRE(TextureCooker::resolveFormat(TextureCompression::Color, source, false).format ==
            Format::Unknown);
  }

  SECTION("Explicit choices")
  {
    REQUIRE(TextureCooker::resolveFormat(TextureCompression::ColorNoAlpha, source, false)
                .format == Format::BC1_RGBA_SRGB);
    REQUIRE(TextureCooker::resolveFormat(TextureCompression::NormalMap, source, false)
                .format == Format::BC5_UNORM);
    REQUIRE(TextureCooker::resolveFormat(TextureCompression::Grayscale, source, false)
                .format == Format::BC4_UNORM);
    REQUIRE(TextureCooker::resolveFormat(TextureCompression::Uncompressed, source, true)
                .format == Format::Unknown);
  }

  SECTION("Names")
  {
    TextureCompression compression = TextureCompression::Auto;
    for (uint32 i = 0; i < static_cast<uint32>(TextureCompression::COUNT); ++i) {
      const auto value = static_cast<TextureCompression>(i);
      REQUIRE(TextureCooker::parseCompression(TextureCooker::getCompressionName(value),
                                              compression));
      REQUIRE(compression == value);
    }
    REQUIRE(TextureCooker::parseCompression("normalmap", compression));
    REQUIRE(compression == TextureCompression::NormalMap);
    REQUIRE_FALSE(TextureCooker::parseCompression("BC7", compression));

    TextureCompressionQuality quality = TextureCompressionQuality::Fast;
    REQUIRE(TextureCooker::parseQuality("high", quality));
    REQUIRE(quality == TextureCompressionQuality::High);
    REQUIRE(TextureCooker::getQualityName(TextureCompressionQuality::Normal) == "Normal");
  }
}

TEST_CASE("chCore - Texture cook blocks", "[Packaging]")
{
  SECTION("A block past the edge repeats the last row and column")
  {
    // 3x2: red is x + 10 y.
    Vector<uint8> level;
    for (uint32 y = 0; y < 2; ++y) {
      for (uint32 x = 0; x < 3; ++x) {
        level.insert(level.end(), {static_cast<uint8>(x + 10 * y), 1, 2, 3});
      }
    }
    Array<uint8, TextureCooker::BLOCK_PIXEL_BYTES> block{};
    TextureCooker::readBlock(level, Format::R8G8B8A8_UNORM, 3, 2, 0, 0, Format::BC7_UNORM,
                             block.data());
    REQUIRE(block[(0 * 4 + 2) * 4] == 2);
    REQUIRE(block[(0 * 4 + 3) * 4] == 2);
    REQUIRE(block[(1 * 4 + 1) * 4] == 11);
    REQUIRE(block[(3 * 4 + 3) * 4] == 12);
    REQUIRE(block[(3 * 4 + 3) * 4 + 3] == 3);
  }

  SECTION("BGRA turns into RGBA, and sRGB into linear for linear targets")
  {
    const Vector<uint8> level = {10, 128, 200, 77};
    Array<uint8, TextureCooker::BLOCK_PIXEL_BYTES> block{};
    TextureCooker::readBlock(level, Format::B8G8R8A8_UNORM, 1, 1, 0, 0, Format::BC7_UNORM,
                             block.data());
    REQUIRE(block[0] == 200);
    REQUIRE(block[1] == 128);
    REQUIRE(block[2] == 10);
    REQUIRE(block[3] == 77);

    // 128 encoded is 0.2158 linear, 55 of 255; alpha never goes through the curve.
    const Vector<uint8> srgb = {128, 255, 0, 128};
    TextureCooker::readBlock(srgb, Format::R8G8B8A8_SRGB, 1, 1, 0, 0, Format::BC4_UNORM,
                             block.data());
    REQUIRE(block[0] == 55);
    REQUIRE(block[1] == 255);
    REQUIRE(block[2] == 0);
    REQUIRE(block[3] == 128);
    TextureCooker::readBlock(srgb, Format::R8G8B8A8_SRGB, 1, 1, 0, 0, Format::BC7_SRGB,
                             block.data());
    REQUIRE(block[0] == 128);
  }
}

TEST_CASE("chCore - Texture cook compress", "[Packaging]")
{
  FakeCompressor compressor;

  SECTION("Every level is compressed in order, small levels take a whole block")
  {
    // 16x8: 8 blocks, then 8x4 (2), 4x2, 2x1 and 1x1 (1 each).
    const Vector<uint8> data = makeChain(16, 8, 5);
    const TextureCookSource source{.format = Format::R8G8B8A8_UNORM,
                                   .width = 16,
                                   .height = 8,
                                   .mipLevels = 5,
                                   .data = data};
    Vector<uint8> output;
    REQUIRE(TextureCooker::compress(compressor, source, Format::BC4_UNORM,
                                    TextureCompressionQuality::High, output));
    REQUIRE(output.size() == 13 * 8);
    const uint8 expected[13] = {0, 1, 2, 3, 4, 5, 6, 7, 40, 41, 80, 120, 160};
    for (uint32 block = 0; block < 13; ++block) {
      INFO("block " << block);
      REQUIRE(output[block * 8] == expected[block]);
      REQUIRE(output[block * 8 + 1] == expected[block]);
      REQUIRE(output[block * 8 + 2] == static_cast<uint8>(TextureCompressionQuality::High));
    }
  }

  SECTION("Big textures are split between threads and every block is written once")
  {
    // 256x256 is 4096 blocks: several threads and batches. Red wraps every 256 blocks.
    const Vector<uint8> data = makeChain(256, 256, 1);
    const TextureCookSource source{.format = Format::R8G8B8A8_UNORM,
                                   .width = 256,
                                   .height = 256,
                                   .mipLevels = 1,
                                   .data = data};
    Vector<uint8> output;
    REQUIRE(TextureCooker::compress(compressor, source, Format::BC4_UNORM,
                                    TextureCompressionQuality::Fast, output));
    REQUIRE(output.size() == 4096 * 8);
    bool bAllRight = true;
    for (uint32 block = 0; block < 4096; ++block) {
      bAllRight = bAllRight && output[block * 8] == static_cast<uint8>(block) &&
                  output[block * 8 + 3] == 0xAB;
    }
    REQUIRE(bAllRight);
  }

  SECTION("Refused sources and a cancel")
  {
    const Vector<uint8> data = makeChain(8, 8, 1);
    TextureCookSource source{.format = Format::R8G8B8A8_UNORM,
                             .width = 8,
                             .height = 8,
                             .mipLevels = 1,
                             .data = data};
    Vector<uint8> output;
    REQUIRE_FALSE(TextureCooker::compress(compressor, source, Format::BC7_UNORM,
                                          TextureCompressionQuality::Fast, output));
    source.mipLevels = 4;
    REQUIRE_FALSE(TextureCooker::compress(compressor, source, Format::BC4_UNORM,
                                          TextureCompressionQuality::Fast, output));
    source.mipLevels = 1;
    const Atomic<bool> cancel{true};
    REQUIRE_FALSE(TextureCooker::compress(compressor, source, Format::BC4_UNORM,
                                          TextureCompressionQuality::Fast, output, &cancel));
    REQUIRE(output.empty());
  }
}

TEST_CASE("chCore - Texture cook cache", "[Packaging]")
{
  const Path cacheFolder = FileSystem::absolutePath(Path("TextureCookCacheTestDir"));
  FileSystem::removeAll(cacheFolder);

  FakeCompressor compressor;
  Vector<uint8> data = makeChain(16, 16, 1);
  const TextureCookSource source{.format = Format::R8G8B8A8_UNORM,
                                 .width = 16,
                                 .height = 16,
                                 .mipLevels = 1,
                                 .data = data};
  Vector<uint8> first;
  bool bFromCache = true;
  REQUIRE(TextureCooker::cook(compressor, source, Format::BC4_UNORM,
                              TextureCompressionQuality::Normal, cacheFolder, first,
                              &bFromCache));
  REQUIRE_FALSE(bFromCache);
  const uint32 callsAfterFirst = compressor.calls.load();
  REQUIRE(callsAfterFirst > 0);

  SECTION("The same input comes back from the cache")
  {
    Vector<uint8> second;
    REQUIRE(TextureCooker::cook(compressor, source, Format::BC4_UNORM,
                                TextureCompressionQuality::Normal, cacheFolder, second,
                                &bFromCache));
    REQUIRE(bFromCache);
    REQUIRE(second == first);
    REQUIRE(compressor.calls.load() == callsAfterFirst);
  }

  SECTION("Other pixels or another quality compress again")
  {
    Vector<uint8> other;
    REQUIRE(TextureCooker::cook(compressor, source, Format::BC4_UNORM,
                                TextureCompressionQuality::High, cacheFolder, other,
                                &bFromCache));
    REQUIRE_FALSE(bFromCache);

    // The fake compressor writes the first pixel of each block.
    data[0] = 99;
    REQUIRE(TextureCooker::cook(compressor, source, Format::BC4_UNORM,
                                TextureCompressionQuality::Normal, cacheFolder, other,
                                &bFromCache));
    REQUIRE_FALSE(bFromCache);
    REQUIRE(other != first);
  }

  SECTION("A cut cache file is made again")
  {
    Vector<Path> files;
    Vector<Path> directories;
    FileSystem::getChildren(cacheFolder, files, directories);
    REQUIRE(files.size() == 1);
    const SPtr<DataStream> stream = FileSystem::createAndOpenFile(files[0]);
    REQUIRE(stream);
    const uint32 garbage = 7;
    REQUIRE(stream->write(&garbage, sizeof(garbage)) == sizeof(garbage));
    stream->close();

    Vector<uint8> again;
    REQUIRE(TextureCooker::cook(compressor, source, Format::BC4_UNORM,
                                TextureCompressionQuality::Normal, cacheFolder, again,
                                &bFromCache));
    REQUIRE_FALSE(bFromCache);
    REQUIRE(again == first);
    REQUIRE(TextureCooker::cook(compressor, source, Format::BC4_UNORM,
                                TextureCompressionQuality::Normal, cacheFolder, again,
                                &bFromCache));
    REQUIRE(bFromCache);
  }

  FileSystem::removeAll(cacheFolder);
}

TEST_CASE("chCore - Packaged textures", "[Packaging]")
{
  const Path root = FileSystem::absolutePath(Path("PackagedTexturesTestDir"));
  FileSystem::removeAll(root);

  const Path binaries = root.join(Path("Build"));
  String executable(ProjectPackager::GAME_EXECUTABLE_NAME);
  executable += ProjectPackager::EXECUTABLE_EXTENSION;
  writeTestFile(binaries.join(Path(executable)), "exe");
  writeTestFile(binaries.join(Path("Shaders/SPIRV/forward.ps.spv")), "spirv");
  const Path engine = root.join(Path("EngineContent"));
  writeTestFile(engine.join(Path("Config/Engine.ini")), "[Window]\nWidth=640\n");
  const Path project = root.join(Path("Project"));
  writeTestFile(project.join(Path("Project.chproject")), "");
  const Path assets = project.join(Path("Assets"));

  const auto colorId = chEngineSDK::UUID::createRandom();
  const auto normalId = chEngineSDK::UUID::createRandom();
  writeTestTexture(assets.join(Path("T_Color.chAss")), colorId, Format::R8G8B8A8_SRGB, 64,
                   true, "[Import]\nSource=C:/Art/Color.png\n");
  writeTestTexture(assets.join(Path("T_Normal.chAss")), normalId, Format::R8G8B8A8_UNORM, 64,
                   true, "");
  writeTestTexture(assets.join(Path("T_Sprite.chAss")), chEngineSDK::UUID::createRandom(),
                   Format::R8G8B8A8_SRGB, 32, false, "");
  writeTestTexture(assets.join(Path("T_Kept.chAss")), chEngineSDK::UUID::createRandom(),
                   Format::R8G8B8A8_SRGB, 32, true, "[Texture]\nCompression=Uncompressed\n");
  writeTestTexture(assets.join(Path("T_Odd.chAss")), chEngineSDK::UUID::createRandom(),
                   Format::R8G8B8A8_SRGB, 30, true, "");

  // A material that reads T_Normal as its normal map.
  Material material;
  material.setTexture(MaterialTexture::Normal, normalId, nullptr);
  {
    const Vector<chEngineSDK::UUID> references{normalId};
    SPtr<DataStream> stream = FileSystem::createAndOpenFile(assets.join(Path("M_Test.chAss")));
    REQUIRE(stream);
    AssetFile::writeStart(*stream, chEngineSDK::UUID::createRandom(),
                          TypeTraits<MaterialAsset>::getTypeId(), references);
    material.serialize(*stream);
    stream->close();
  }

  const FakeCompressor compressor(true);
  const Path output = root.join(Path("Out/MyGame"));
  PackageDesc desc{.gameName = "MyGame",
                   .outputDirectory = output,
                   .binaryDirectory = binaries,
                   .engineContentDirectory = engine,
                   .projectDirectory = project,
                   .assetFiles = {assets.join(Path("T_Color.chAss")),
                                  assets.join(Path("T_Normal.chAss")),
                                  assets.join(Path("T_Sprite.chAss")),
                                  assets.join(Path("T_Kept.chAss")),
                                  assets.join(Path("T_Odd.chAss")),
                                  assets.join(Path("M_Test.chAss"))},
                   .startupScene = "/Game/Maps/Main",
                   .textureCompressor = &compressor,
                   .textureQuality = TextureCompressionQuality::Fast,
                   .textureCacheDirectory = root.join(Path("Cache"))};

  SECTION("Each texture gets the format its use and setting ask for")
  {
    const PackageResult result = ProjectPackager::package(desc);
    REQUIRE(result.bSucceeded);
    REQUIRE(result.cookStats.compressedTextures == 2);
    REQUIRE(result.cookStats.cachedTextures == 0);
    REQUIRE(result.cookStats.keptTextures == 3);
    REQUIRE(result.cookStats.textureBytesAfter < result.cookStats.textureBytesBefore);

    const Path packaged = output.join(Path("Assets"));
    const TextureAssetHeader color =
        readPackagedTextureHeader(packaged.join(Path("T_Color.chAss")));
    REQUIRE(color.format == Format::BC7_SRGB);
    REQUIRE(color.mipLevels == 7);
    REQUIRE(readPackagedTextureHeader(packaged.join(Path("T_Normal.chAss"))).format ==
            Format::BC5_UNORM);
    REQUIRE(readPackagedTextureHeader(packaged.join(Path("T_Sprite.chAss"))).format ==
            Format::R8G8B8A8_SRGB);
    REQUIRE(readPackagedTextureHeader(packaged.join(Path("T_Kept.chAss"))).format ==
            Format::R8G8B8A8_SRGB);
    REQUIRE(readPackagedTextureHeader(packaged.join(Path("T_Odd.chAss"))).format ==
            Format::R8G8B8A8_SRGB);

    // A second package reads both compressed textures back from the cache.
    const uint32 calls = compressor.calls.load();
    const PackageResult again = ProjectPackager::package(desc);
    REQUIRE(again.bSucceeded);
    REQUIRE(again.cookStats.compressedTextures == 0);
    REQUIRE(again.cookStats.cachedTextures == 2);
    REQUIRE(compressor.calls.load() == calls);
  }

  SECTION("Without a compressor every texture is kept")
  {
    desc.textureCompressor = nullptr;
    const PackageResult result = ProjectPackager::package(desc);
    REQUIRE(result.bSucceeded);
    REQUIRE(result.cookStats.keptTextures == 5);
    REQUIRE(readPackagedTextureHeader(output.join(Path("Assets/T_Color.chAss"))).format ==
            Format::R8G8B8A8_SRGB);
  }

  SECTION("Normal maps are found from the materials")
  {
    Vector<chEngineSDK::UUID> normals;
    AssetCooker::collectNormalMapTextures(desc.assetFiles, normals);
    REQUIRE(normals == Vector<chEngineSDK::UUID>{normalId});
  }

  FileSystem::removeAll(root);
}

#endif // USING(CH_EDITOR)
