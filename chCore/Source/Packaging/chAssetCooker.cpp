/************************************************************************/
/**
 * @file chAssetCooker.cpp
 * @author AccelMR
 * @date 2026/11/20
 * @brief
 *  Writes the packaged form of each asset file.
 */
/************************************************************************/
#include "chAssetCooker.h"

#if USING(CH_EDITOR)

#include <cstring>

#include "chAlgorithm.h"
#include "chConfigFile.h"
#include "chEnginePaths.h"
#include "chFileStream.h"
#include "chFileSystem.h"
#include "chLogger.h"
#include "chMaterial.h"
#include "chMaterialAsset.h"
#include "chMath.h"
#include "chTextureAsset.h"
#include "chTextureMips.h"

namespace chEngineSDK {
CH_LOG_DECLARE_STATIC(AssetCookerLog, All);

namespace {
constexpr StringView kTextureSection = "Texture";
constexpr StringView kCompressionKey = "Compression";

// Built on first use, with the engine's own cookers in it. Changed only on the main thread
// while no package runs, so the worker reads it without a lock.
UnorderedMap<UUID, AssetCookFunction>&
getCookers()
{
  static UnorderedMap<UUID, AssetCookFunction> cookers = [] {
    UnorderedMap<UUID, AssetCookFunction> engineCookers;
    engineCookers.emplace(TypeTraits<TextureAsset>::getTypeId(), &AssetCooker::cookTexture);
    return engineCookers;
  }();
  return cookers;
}

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

TextureCompression
readCompression(const AssetCookSource& source)
{
  TextureCompression compression = TextureCompression::Auto;
  if (source.editorData.empty()) {
    return compression;
  }
  ConfigFile editorData;
  editorData.parse(source.editorData, source.file.toString());
  const String* value = editorData.getValue(kTextureSection, kCompressionKey);
  if (value && !TextureCooker::parseCompression(*value, compression)) {
    CH_LOG_WARNING(AssetCookerLog, "{0}: unknown compression '{1}', using Auto.", source.file,
                   *value);
  }
  return compression;
}
} // namespace

/*
 */
void
AssetCooker::registerCooker(const UUID& assetType, AssetCookFunction cooker)
{
  getCookers()[assetType] = cooker;
}

/*
 */
void
AssetCooker::unregisterCooker(const UUID& assetType)
{
  getCookers().erase(assetType);
}

/*
 */
AssetCookFunction
AssetCooker::findCooker(const UUID& assetType)
{
  const UnorderedMap<UUID, AssetCookFunction>& cookers = getCookers();
  const auto it = cookers.find(assetType);
  return it != cookers.end() ? it->second : nullptr;
}

/*
 */
bool
AssetCooker::cookFile(const Path& source,
                      const Path& destination,
                      const AssetCookSettings& settings,
                      AssetCookStats& stats,
                      String& outError)
{
  const Vector<uint8> bytes = FileSystem::fastRead(source);
  MemoryDataStream input(const_cast<uint8*>(bytes.data()), bytes.size(), false);
  AssetCookSource cookSource{.file = source};
  if (bytes.empty() || !AssetFile::readInfo(input, cookSource.info) ||
      !AssetFile::readReferences(input, cookSource.references) ||
      !AssetFile::readEditorData(input, &cookSource.editorData)) {
    outError = "Could not read the asset file " + source.toString() + ".";
    return false;
  }
  const SIZE_T dataStart = input.tell();
  cookSource.data = Span<const uint8>(bytes.data() + dataStart, bytes.size() - dataStart);

  Vector<uint8> cooked;
  Span<const uint8> data = cookSource.data;
  if (const AssetCookFunction cooker = findCooker(cookSource.info.assetType)) {
    if (!cooker(cookSource, settings, stats, cooked, outError)) {
      return false;
    }
    data = cooked;
  }

  const SPtr<DataStream> output = FileSystem::createAndOpenFile(destination);
  if (!output) {
    outError = "Could not write " + destination.toString() + ".";
    return false;
  }
  AssetFile::writeStart(*output, cookSource.info.uuid, cookSource.info.assetType,
                        cookSource.references);
  const bool bWritten = data.empty() || output->write(data.data(), data.size()) == data.size();
  output->close();
  if (!bWritten) {
    outError = "Could not write " + destination.toString() + ".";
  }
  return bWritten;
}

/*
 */
void
AssetCooker::collectNormalMapTextures(Span<const Path> assetFiles, Vector<UUID>& outTextures)
{
  const UUID materialType = TypeTraits<MaterialAsset>::getTypeId();
  for (const Path& file : assetFiles) {
    if (file.getExtension() != EnginePaths::getEngineAssetExtension()) {
      continue;
    }
    const SPtr<DataStream> stream = FileSystem::openFile(file);
    AssetFileInfo info;
    if (!stream || !AssetFile::readInfo(*stream, info) || info.assetType != materialType) {
      continue;
    }
    Vector<UUID> references;
    Material material;
    if (!AssetFile::readReferences(*stream, references) ||
        !AssetFile::readEditorData(*stream, nullptr) || !material.deserialize(*stream)) {
      CH_LOG_WARNING(AssetCookerLog, "Could not read the material {0}.", file);
      continue;
    }
    const UUID& normalTexture = material.getTextureId(MaterialTexture::Normal);
    if (!normalTexture.isNull()) {
      outTextures.push_back(normalTexture);
    }
  }
  Algorithm::sort(outTextures);
  Algorithm::removeSortedDuplicates(outTextures);
}

/*
 */
bool
AssetCooker::cookTexture(const AssetCookSource& source,
                         const AssetCookSettings& settings,
                         AssetCookStats& stats,
                         Vector<uint8>& outData,
                         String& outError)
{
  TextureAssetHeader header;
  if (source.data.size() < sizeof(header)) {
    outError = "The texture " + source.file.toString() + " is cut.";
    return false;
  }
  memcpy(&header, source.data.data(), sizeof(header));
  const Span<const uint8> pixels = source.data.subspan(sizeof(header));
  if (header.version != TextureAssetHeader::VERSION ||
      static_cast<uint32>(header.format) >= static_cast<uint32>(Format::COUNT) ||
      header.mipLevels == 0 ||
      header.mipLevels > TextureMips::getMipCount(header.width, header.height) ||
      pixels.size() != getChainSize(header.format, header.width, header.height,
                                    header.mipLevels)) {
    outError = "The texture " + source.file.toString() + " is broken or old; import it again.";
    return false;
  }
  stats.textureBytesBefore += pixels.size();

  const TextureCookSource cookSource{.format = header.format,
                                     .width = header.width,
                                     .height = header.height,
                                     .mipLevels = header.mipLevels,
                                     .data = pixels};
  const bool bNormalMap =
      Algorithm::containsSorted(settings.normalMapTextures, source.info.uuid);
  const TextureCookFormat cookFormat =
      TextureCooker::resolveFormat(readCompression(source), cookSource, bNormalMap);

  if (cookFormat.format == Format::Unknown || !settings.textureCompressor) {
    outData.assign(source.data.begin(), source.data.end());
    stats.textureBytesAfter += pixels.size();
    ++stats.keptTextures;
    return true;
  }

  Vector<uint8> compressed;
  bool bFromCache = false;
  if (!TextureCooker::cook(*settings.textureCompressor, cookSource, cookFormat.format,
                           settings.textureQuality, settings.textureCacheDirectory, compressed,
                           &bFromCache, settings.cancel)) {
    outError = settings.cancel && settings.cancel->load(std::memory_order_relaxed)
                   ? String("Cancelled.")
                   : "Could not compress the texture " + source.file.toString() + ".";
    return false;
  }

  header.format = cookFormat.format;
  outData.resize(sizeof(header) + compressed.size());
  memcpy(outData.data(), &header, sizeof(header));
  memcpy(outData.data() + sizeof(header), compressed.data(), compressed.size());
  stats.textureBytesAfter += compressed.size();
  if (bFromCache) {
    ++stats.cachedTextures;
  }
  else {
    ++stats.compressedTextures;
  }
  return true;
}

} // namespace chEngineSDK

#endif // USING(CH_EDITOR)
