/************************************************************************/
/**
 * @file chAssetManifest.cpp
 * @author AccelMR
 * @date 2026/11/20
 * @brief
 *  The list of every asset of one content layer, so a game knows them without opening them.
 */
/************************************************************************/
#include "chAssetManifest.h"

#include "chAssetFile.h"
#include "chFileStream.h"
#include "chFileSystem.h"

namespace chEngineSDK {

namespace {
struct ManifestHeader
{
  uint32 magic = AssetManifest::MAGIC;
  uint32 version = AssetManifest::VERSION;
  uint32 entryCount = 0;
};
static_assert(sizeof(ManifestHeader) == 12, "ManifestHeader must have no padding");

// The smallest entry: an empty path length, two UUIDs and an empty reference count.
constexpr SIZE_T kMinEntrySize = sizeof(uint32) + 2 * sizeof(UUID) + sizeof(uint32);

bool
readUUID(DataStream& stream, UUID& outUUID)
{
  return AssetFile::readValue(stream, outUUID);
}
} // namespace

/*
 */
bool
AssetManifest::read(const Path& file, String& outError)
{
  gameName.clear();
  engineVersion.clear();
  entries.clear();

  const Vector<uint8> bytes = FileSystem::fastRead(file);
  if (bytes.empty()) {
    outError = "Could not read " + file.toString() + ".";
    return false;
  }
  MemoryDataStream stream(const_cast<uint8*>(bytes.data()), bytes.size(), false);

  ManifestHeader header;
  if (!AssetFile::readValue(stream, header) || header.magic != MAGIC) {
    outError = file.toString() + " is not an asset manifest.";
    return false;
  }
  if (header.version != VERSION) {
    outError = file.toString() + " is a manifest of another version; package the game again.";
    return false;
  }
  if (header.entryCount > MAX_ENTRIES ||
      header.entryCount > AssetFile::getBytesLeft(stream) / kMinEntrySize ||
      !AssetFile::readString(stream, gameName, MAX_TEXT_LENGTH) ||
      !AssetFile::readString(stream, engineVersion, MAX_TEXT_LENGTH)) {
    outError = file.toString() + " is cut or broken.";
    return false;
  }

  entries.resize(header.entryCount);
  for (AssetManifestEntry& entry : entries) {
    uint32 referenceCount = 0;
    if (!AssetFile::readString(stream, entry.path, MAX_TEXT_LENGTH) || entry.path.empty() ||
        !readUUID(stream, entry.uuid) || !readUUID(stream, entry.assetType) ||
        !AssetFile::readValue(stream, referenceCount) ||
        referenceCount > AssetFile::MAX_REFERENCES ||
        referenceCount > AssetFile::getBytesLeft(stream) / sizeof(UUID)) {
      outError = file.toString() + " is cut or broken.";
      entries.clear();
      return false;
    }
    entry.references.resize(referenceCount);
    if (referenceCount > 0 &&
        !AssetFile::readBytes(stream, entry.references.data(),
                              referenceCount * sizeof(UUID))) {
      outError = file.toString() + " is cut or broken.";
      entries.clear();
      return false;
    }
  }
  return true;
}

#if USING(CH_EDITOR)
/*
 */
bool
AssetManifest::write(const Path& file) const
{
  MemoryDataStream stream;
  const ManifestHeader header{.entryCount = static_cast<uint32>(entries.size())};
  stream.write(&header, sizeof(header));
  AssetFile::writeString(stream, gameName);
  AssetFile::writeString(stream, engineVersion);
  for (const AssetManifestEntry& entry : entries) {
    AssetFile::writeString(stream, entry.path);
    stream.write(&entry.uuid, sizeof(UUID));
    stream.write(&entry.assetType, sizeof(UUID));
    const uint32 referenceCount = static_cast<uint32>(entry.references.size());
    stream.write(&referenceCount, sizeof(referenceCount));
    if (referenceCount > 0) {
      stream.write(entry.references.data(), referenceCount * sizeof(UUID));
    }
  }

  const SPtr<DataStream> output = FileSystem::createAndOpenFile(file);
  if (!output) {
    return false;
  }
  const SIZE_T size = stream.size();
  const bool bWritten = output->write(stream.getStartPtr(), size) == size;
  output->close();
  return bWritten;
}
#endif // USING(CH_EDITOR)

} // namespace chEngineSDK
