/************************************************************************/
/**
 * @file chAudioCodec.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Codec that imports sound files as audio clip assets.
 */
/************************************************************************/
#include "chAudioCodec.h"

#include "chConsoleVariable.h"
#include "chFileSystem.h"
#include "chLogger.h"

namespace chEngineSDK {

CH_LOG_DECLARE_STATIC(AudioCodecLog, All);

namespace {
ConsoleVariable<float> g_cvarStreamAboveSeconds(
    "Audio.ImportStreamSeconds", 10.0f,
    "Imported sounds longer than this many seconds stream (music bus); shorter ones are "
    "decoded when they load (effects bus).");

/**
 * Exists so the file is read and checked on the import's worker thread, and only the asset
 * is made on the main thread.
 */
class AudioImportTask final : public IImportTask
{
 public:
  AudioImportTask(const Path& filePath, const String& assetName, const ImportSettings& settings)
    : m_filePath(filePath),
      m_assetName(assetName),
      m_settings(settings)
  {}

  NODISCARD bool
  readSource(ImportProgress& progress) override
  {
    progress.set("Reading the sound", -1.0f);
    m_data = FileSystem::fastRead(m_filePath);
    if (m_data.empty()) {
      CH_LOG_ERROR(AudioCodecLog, "The sound {0} could not be read.", m_filePath);
      return false;
    }
    if (!AudioClip::readInfo(m_data, m_info)) {
      CH_LOG_ERROR(AudioCodecLog, "{0} is not a WAV, Ogg Vorbis, MP3 or FLAC file.",
                   m_filePath);
      return false;
    }
    return !progress.isCancelled();
  }

  SPtr<IAsset>
  createAssets(const Path& assetFolder) override
  {
    const bool bStream = m_info.getDuration() > g_cvarStreamAboveSeconds.get();
    const AudioClipSettings clipSettings{
        .loadMode = bStream ? AudioClipLoadMode::Stream : AudioClipLoadMode::Decompress,
        .bus = bStream ? AudioBus::Music : AudioBus::Sfx};
    const AssetMetadata metadata =
        IAssetCodec::makeMetadata<AudioClipAsset>(m_assetName, assetFolder);
    const SPtr<AudioClipAsset> asset =
        chMakeShared<AudioClipAsset>(metadata, std::move(m_data), clipSettings);
    if (!asset->getClip() ||
        !IAssetCodec::saveAndRegister(asset, FileSystem::absolutePath(m_filePath), StringView(),
                                      m_settings)) {
      CH_LOG_ERROR(AudioCodecLog, "Failed to save audio clip asset: {0}", m_assetName);
      return nullptr;
    }
    CH_LOG_INFO(AudioCodecLog, "Imported sound {0} ({1:.2f} s, {2} channels, {3} Hz{4}) from {5}",
                asset->getName(), m_info.getDuration(), m_info.channels, m_info.sampleRate,
                bStream ? ", streamed" : "", m_filePath);
    return asset;
  }

 private:
  Path m_filePath;
  String m_assetName;
  ImportSettings m_settings;
  Vector<uint8> m_data;
  AudioClipInfo m_info;
};
} // namespace

/*
 */
UniquePtr<IImportTask>
AudioCodec::createImportTask(const Path& filePath,
                             const String& assetName,
                             const ImportSettings& settings)
{
  return chMakeUnique<AudioImportTask>(filePath, assetName, settings);
}

} // namespace chEngineSDK
