/************************************************************************/
/**
 * @file chAudioClipAsset.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Asset that holds a sound file and how it is played.
 */
/************************************************************************/
#include "chAudioClipAsset.h"

#include "chAssetFile.h"
#include "chAssetManager.h"
#include "chFileStream.h"
#include "chLogger.h"
#include "chMath.h"

namespace chEngineSDK {

/*
 */
AudioClipAsset::AudioClipAsset(const AssetMetadata& metadata,
                               Vector<uint8> encodedData,
                               const AudioClipSettings& settings)
  : IAsset(metadata),
    m_clip(AudioClip::create(std::move(encodedData), settings))
{}

#if USING(CH_EDITOR)
/*
 */
void
AudioClipAsset::setSettings(const AudioClipSettings& settings)
{
  if (!m_clip) {
    return;
  }
  const Span<const uint8> data = m_clip->getEncodedData();
  SPtr<AudioClip> clip = AudioClip::create(Vector<uint8>(data.begin(), data.end()), settings);
  if (clip) {
    m_clip = std::move(clip);
  }
}
#endif // USING(CH_EDITOR)

/*
 */
bool
AudioClipAsset::serialize(SPtr<DataStream> stream)
{
  if (!stream || !m_clip) {
    return false;
  }
  const Span<const uint8> data = m_clip->getEncodedData();
  const AudioClipSettings& settings = m_clip->getSettings();
  const AudioClipAssetHeader header{.loadMode = static_cast<uint32>(settings.loadMode),
                                    .bus = static_cast<uint32>(settings.bus),
                                    .maxVoices = settings.maxVoices,
                                    .volume = settings.volume,
                                    .dataSize = static_cast<uint32>(data.size())};
  *stream << header;
  return stream->write(data.data(), data.size()) == data.size();
}

/*
 */
bool
AudioClipAsset::readClip(DataStream& stream, SPtr<AudioClip>& outClip) const
{
  AudioClipAssetHeader header;
  if (!AssetFile::readValue(stream, header)) {
    CH_LOG_ERROR(AssetSystem, "Audio clip {0} is cut short.", getName());
    return false;
  }
  if (header.version != AudioClipAssetHeader::VERSION) {
    CH_LOG_ERROR(AssetSystem,
                 "Audio clip {0} has version {1}, this build reads {2}; import it again.",
                 getName(),
                 header.version,
                 AudioClipAssetHeader::VERSION);
    return false;
  }
  if (header.loadMode >= static_cast<uint32>(AudioClipLoadMode::COUNT) ||
      header.bus >= static_cast<uint32>(AudioBus::COUNT) || !Math::isFinite(header.volume) ||
      header.volume < 0.0f) {
    CH_LOG_ERROR(AssetSystem, "Audio clip {0} has broken settings.", getName());
    return false;
  }
  if (header.dataSize == 0 || header.dataSize > AudioClip::MAX_ENCODED_SIZE ||
      header.dataSize > AssetFile::getBytesLeft(stream)) {
    CH_LOG_ERROR(AssetSystem, "Audio clip {0} is cut short.", getName());
    return false;
  }
  Vector<uint8> data(header.dataSize);
  if (!AssetFile::readBytes(stream, data.data(), data.size())) {
    CH_LOG_ERROR(AssetSystem, "Audio clip {0} is cut short.", getName());
    return false;
  }

  const AudioClipSettings settings{.loadMode = static_cast<AudioClipLoadMode>(header.loadMode),
                                   .bus = static_cast<AudioBus>(header.bus),
                                   .volume = header.volume,
                                   .maxVoices = header.maxVoices};
  outClip = AudioClip::create(std::move(data), settings);
  if (!outClip) {
    CH_LOG_ERROR(AssetSystem, "Audio clip {0} does not hold a sound this build reads.",
                 getName());
  }
  return outClip != nullptr;
}

/*
 */
bool
AudioClipAsset::deserialize(SPtr<DataStream> stream)
{
  return stream && readClip(*stream, m_clip);
}

/*
 */
bool
AudioClipAsset::readAsync(Vector<uint8>& fileBytes, SIZE_T dataOffset, String& outError)
{
  // Decompressed clips decode the whole sound here, the slow part of loading them.
  MemoryDataStream stream(fileBytes.data() + dataOffset, fileBytes.size() - dataOffset, false);
  if (!readClip(stream, m_pendingClip)) {
    outError = "the sound could not be read";
    return false;
  }
  return true;
}

/*
 */
bool
AudioClipAsset::finishAsyncRead()
{
  m_clip = std::move(m_pendingClip);
  return m_clip != nullptr;
}

/*
 */
void
AudioClipAsset::clearAssetData()
{
  m_clip.reset();
}

} // namespace chEngineSDK
