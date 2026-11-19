/************************************************************************/
/**
 * @file chAudioCodec.h
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Codec that imports sound files as audio clip assets.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chAssetCodec.h"
#include "chAudioClipAsset.h"
#include "chTypeTraits.h"

namespace chEngineSDK {

/**
 * Exists so the editor turns a WAV, Ogg Vorbis, MP3 or FLAC file into an AudioClipAsset,
 * which keeps the file as it is inside the project. Long sounds are set to stream (decoded
 * while they play) and go to the music bus; short ones are decoded when they load.
 */
class AudioCodec : public IAssetCodec
{
 public:
  NODISCARD UUID
  getCodecType() const override
  {
    static const UUID codecType = UUID::createFromName("AudioCodec");
    return codecType;
  }

  NODISCARD Vector<UUID>
  getSupportedAssetTypes() const override
  {
    return {AssetTypeTraits<AudioClipAsset>::getTypeId()};
  }

  NODISCARD const Vector<String>&
  getSupportedExtensions() const override
  {
    return m_extensions;
  }

  NODISCARD UniquePtr<IImportTask>
  createImportTask(const Path& filePath,
                   const String& assetName,
                   const ImportSettings& settings) override;

 private:
  const Vector<String> m_extensions = {"wav", "ogg", "mp3", "flac"};
};
DECLARE_ASSET_TYPE(AudioCodec);

} // namespace chEngineSDK
