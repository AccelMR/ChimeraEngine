/************************************************************************/
/**
 * @file chAssetCooker.h
 * @author AccelMR
 * @date 2026/11/20
 * @brief
 *  Writes the packaged form of each asset file.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#if USING(CH_EDITOR)

#include "chAssetFile.h"
#include "chAssetManifest.h"
#include "chPath.h"
#include "chSTDThreading.h"
#include "chTextureCooker.h"
#include "chUUID.h"

namespace chEngineSDK {

/**
 * An asset file read whole: its start and the data of its type.
 */
struct AssetCookSource
{
  Path file{};
  AssetFileInfo info{};
  Vector<UUID> references{};
  // The editor data text, which never reaches the package; cookers read settings from it.
  String editorData{};
  Span<const uint8> data{};
};

/**
 * What every cooker of one package shares.
 */
struct AssetCookSettings
{
  // Null keeps textures as they are (the plugin is missing).
  const ITextureCompressor* textureCompressor = nullptr;
  TextureCompressionQuality textureQuality = TextureCompressionQuality::Normal;
  // Where compressed textures are kept between packages.
  Path textureCacheDirectory{};
  // Textures the packaged materials read as normal maps, sorted.
  Vector<UUID> normalMapTextures{};
  // Stops a cooker between steps; may be null.
  const Atomic<bool>* cancel = nullptr;
};

/**
 * What the cookers did in one package, for the result line.
 */
struct AssetCookStats
{
  uint32 compressedTextures = 0;
  // Compressed in an earlier package and read back from the cache.
  uint32 cachedTextures = 0;
  // Left as they are: no mips, floats, set to Uncompressed or no compressor.
  uint32 keptTextures = 0;
  uint64 textureBytesBefore = 0;
  uint64 textureBytesAfter = 0;
};

/**
 * Fills outData with what follows the asset file start in the package. False with outError
 * set when the asset cannot be packaged.
 */
using AssetCookFunction = bool (*)(const AssetCookSource& source,
                                   const AssetCookSettings& settings,
                                   AssetCookStats& stats,
                                   Vector<uint8>& outData,
                                   String& outError);

/**
 * Exists so Package Project writes each asset type in the form a game loads best: a type
 * with a cooker (textures, compressed to BC formats) has its data made again, every other
 * type keeps its data byte for byte. The editor data is always left out. Plugins with asset
 * types of their own can register cookers too.
 */
class CH_CORE_EXPORT AssetCooker
{
 public:
  /**
   * Replaces the cooker of the type. Called on the main thread before a package starts.
   */
  static void
  registerCooker(const UUID& assetType, AssetCookFunction cooker);

  static void
  unregisterCooker(const UUID& assetType);

  /**
   * Null when the type keeps its data as it is.
   */
  NODISCARD static AssetCookFunction
  findCooker(const UUID& assetType);

  /**
   * Reads source, cooks it and writes destination without editor data. outEntry, when
   * given, receives the asset's UUID, type and references (not its path).
   */
  NODISCARD static bool
  cookFile(const Path& source,
           const Path& destination,
           const AssetCookSettings& settings,
           AssetCookStats& stats,
           String& outError,
           AssetManifestEntry* outEntry = nullptr);

  /**
   * cookFile into memory: outBytes receives the whole packaged file.
   */
  NODISCARD static bool
  cookToMemory(const Path& source,
               const AssetCookSettings& settings,
               AssetCookStats& stats,
               Vector<uint8>& outBytes,
               String& outError,
               AssetManifestEntry* outEntry = nullptr);

  /**
   * Adds to outTextures every texture the material files among assetFiles use as their
   * normal map, then sorts them and drops repeats. Files that are not materials are skipped
   * after their start.
   */
  static void
  collectNormalMapTextures(Span<const Path> assetFiles, Vector<UUID>& outTextures);

  /**
   * The cooker of texture assets, registered by the engine itself.
   */
  NODISCARD static bool
  cookTexture(const AssetCookSource& source,
              const AssetCookSettings& settings,
              AssetCookStats& stats,
              Vector<uint8>& outData,
              String& outError);
};

} // namespace chEngineSDK

#endif // USING(CH_EDITOR)
