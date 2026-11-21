/************************************************************************/
/**
 * @file chAssetManifest.h
 * @author AccelMR
 * @date 2026/11/20
 * @brief
 *  The list of every asset of one content layer, so a game knows them without opening them.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"
#include "chPath.h"
#include "chUUID.h"

namespace chEngineSDK {

/**
 * One asset file of a layer, with what the asset manager reads from its start.
 */
struct AssetManifestEntry
{
  // Relative to the layer's root with '/' separators, extension included
  // ("Maps/Main.chAss", "Scripts/Player.lua").
  String path{};
  UUID uuid = UUID::null();
  UUID assetType = UUID::null();
  Vector<UUID> references{};
};

/**
 * Exists so a packaged game starts without opening every asset file to learn its identity:
 * Package Project writes one next to the assets of each content layer (the game, later a DLC
 * or a mod), and the asset manager registers the entries instead of scanning. Layers mounted
 * over others replace assets by UUID.
 */
class CH_CORE_EXPORT AssetManifest
{
 public:
  /**
   * False with outError set when the file is missing, cut, of another version or broken.
   */
  NODISCARD bool
  read(const Path& file, String& outError);

  /**
   * Like read, from bytes already in memory (an archive's); sourceName names them in errors.
   */
  NODISCARD bool
  readFromMemory(Span<const uint8> bytes, StringView sourceName, String& outError);

#if USING(CH_EDITOR)
  NODISCARD bool
  write(const Path& file) const;

  void
  writeToMemory(Vector<uint8>& outBytes) const;
#endif // USING(CH_EDITOR)

  // Written by Package Project: the game's name and the engine version that packaged it.
  String gameName{};
  String engineVersion{};
  Vector<AssetManifestEntry> entries{};

  static constexpr StringView FILE_NAME = "AssetManifest.chman";
  // "CHAM" as the bytes appear in the file.
  static constexpr uint32 MAGIC = 0x4D414843;
  static constexpr uint32 VERSION = 1;
  // Above any real layer; only a broken file asks for more.
  static constexpr uint32 MAX_ENTRIES = 1u << 20;
  static constexpr uint32 MAX_TEXT_LENGTH = 4096;
};

} // namespace chEngineSDK
