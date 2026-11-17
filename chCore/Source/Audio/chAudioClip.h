/************************************************************************/
/**
 * @file chAudioClip.h
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  A sound file kept in memory, ready to be played.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chAudioTypes.h"

namespace chEngineSDK {

/**
 * Exists so voices can play a sound without knowing its file format: it keeps the file's
 * bytes (WAV, Ogg Vorbis, MP3, FLAC) and, for clips that are not streamed, the samples
 * decoded once as 32-bit floats. It never changes once made, so voices share it through an
 * SPtr and keep it alive while they play, even after its asset unloads.
 */
class CH_CORE_EXPORT AudioClip
{
 public:
  /**
   * Reads what the bytes hold and, for Decompress, decodes them. Null when they are not a
   * sound file this build reads, or too big.
   */
  NODISCARD static SPtr<AudioClip>
  create(Vector<uint8> encodedData, const AudioClipSettings& settings);

  /**
   * Fills info from the file's header without decoding it. False when it is not a sound
   * file this build reads.
   */
  NODISCARD static bool
  readInfo(Span<const uint8> encodedData, AudioClipInfo& outInfo);

  AudioClip(const AudioClip&) = delete;
  AudioClip&
  operator=(const AudioClip&) = delete;

  NODISCARD FORCEINLINE const AudioClipInfo&
  getInfo() const noexcept
  {
    return m_info;
  }

  NODISCARD FORCEINLINE const AudioClipSettings&
  getSettings() const noexcept
  {
    return m_settings;
  }

  NODISCARD FORCEINLINE Span<const uint8>
  getEncodedData() const noexcept
  {
    return m_encodedData;
  }

  /**
   * Interleaved by channel; empty for streamed clips.
   */
  NODISCARD FORCEINLINE Span<const float>
  getSamples() const noexcept
  {
    return m_samples;
  }

  NODISCARD FORCEINLINE bool
  isStreamed() const noexcept
  {
    return m_settings.loadMode == AudioClipLoadMode::Stream;
  }

  // Above any real sound file; only a broken asset asks for more.
  static constexpr uint64 MAX_ENCODED_SIZE = 512ull * 1024ull * 1024ull;
  // Decoded samples of one clip; longer sounds must be streamed.
  static constexpr uint64 MAX_DECODED_SIZE = 1024ull * 1024ull * 1024ull;
  static constexpr uint32 MAX_CHANNELS = 8;

 private:
  AudioClip() = default;

  Vector<uint8> m_encodedData;
  Vector<float> m_samples;
  AudioClipInfo m_info;
  AudioClipSettings m_settings;
};

} // namespace chEngineSDK
