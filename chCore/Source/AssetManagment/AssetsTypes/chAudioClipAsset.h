/************************************************************************/
/**
 * @file chAudioClipAsset.h
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Asset that holds a sound file and how it is played.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chAudioClip.h"
#include "chIAsset.h"
#include "chTypeTraits.h"

namespace chEngineSDK {

// Written after the common asset start, before the sound file.
struct AudioClipAssetHeader
{
  static constexpr uint32 VERSION = 1;

  uint32 version = VERSION;
  // AudioClipLoadMode
  uint32 loadMode = 0;
  // AudioBus
  uint32 bus = 0;
  uint32 maxVoices = 0;
  float volume = 1.0f;
  uint32 dataSize = 0;
};
static_assert(sizeof(AudioClipAssetHeader) == 24, "AudioClipAssetHeader must have no padding");

/**
 * Exists to keep a sound file inside the project, so the game plays it without the original
 * file; the AudioClip is made when the asset loads. Voices hold the clip itself, so a sound
 * still playing when its asset unloads finishes.
 */
class CH_CORE_EXPORT AudioClipAsset : public IAsset
{
 public:
  AudioClipAsset() = delete;

  AudioClipAsset(const AssetMetadata& metadata) : IAsset(metadata) {}

  /**
   * For assets made in code (importers, tests).
   */
  AudioClipAsset(const AssetMetadata& metadata,
                 Vector<uint8> encodedData,
                 const AudioClipSettings& settings);

  ~AudioClipAsset() = default;

  /**
   * Null until loaded, and when the file is not a sound.
   */
  NODISCARD FORCEINLINE const SPtr<AudioClip>&
  getClip() const noexcept
  {
    return m_clip;
  }

#if USING(CH_EDITOR)
  /**
   * Makes the clip again with other settings; voices already playing keep the old one.
   */
  void
  setSettings(const AudioClipSettings& settings);
#endif // USING(CH_EDITOR)

 protected:
  bool
  serialize(SPtr<DataStream> stream) override;

  bool
  deserialize(SPtr<DataStream> stream) override;

  void
  clearAssetData() override;

  NODISCARD bool
  hasAsyncRead() const override
  {
    return true;
  }

  bool
  readAsync(Vector<uint8>& fileBytes, SIZE_T dataOffset, String& outError) override;

  bool
  finishAsyncRead() override;

  /**
   * Reads the header and the sound file after it; outClip is null when it fails.
   */
  bool
  readClip(DataStream& stream, SPtr<AudioClip>& outClip) const;

 private:
  SPtr<AudioClip> m_clip;
  // Read by readAsync on the load's worker.
  SPtr<AudioClip> m_pendingClip;
};

DECLARE_ASSET_TYPE(AudioClipAsset);

} // namespace chEngineSDK
