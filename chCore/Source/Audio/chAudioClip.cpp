/************************************************************************/
/**
 * @file chAudioClip.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  A sound file kept in memory, ready to be played.
 */
/************************************************************************/
#include "chAudioClip.h"

#include "chLogger.h"

#include "miniaudio.h"

namespace chEngineSDK {

namespace {
/**
 * Opens a decoder that gives 32-bit floats at the file's own channels and rate; the voice
 * converts to the output, so a clip decoded once plays on any device.
 */
NODISCARD bool
openDecoder(Span<const uint8> encodedData, ma_decoder& decoder)
{
  const ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
  return ma_decoder_init_memory(encodedData.data(), encodedData.size(), &config, &decoder) ==
         MA_SUCCESS;
}

NODISCARD bool
readDecoderInfo(ma_decoder& decoder, AudioClipInfo& outInfo)
{
  ma_format format;
  ma_uint32 channels = 0;
  ma_uint32 sampleRate = 0;
  ma_uint64 frameCount = 0;
  if (ma_decoder_get_data_format(&decoder, &format, &channels, &sampleRate, nullptr, 0) !=
          MA_SUCCESS ||
      ma_decoder_get_length_in_pcm_frames(&decoder, &frameCount) != MA_SUCCESS) {
    return false;
  }
  if (channels == 0 || channels > AudioClip::MAX_CHANNELS || sampleRate == 0) {
    return false;
  }
  outInfo = {.channels = channels, .sampleRate = sampleRate, .frameCount = frameCount};
  return true;
}
} // namespace

/*
 */
bool
AudioClip::readInfo(Span<const uint8> encodedData, AudioClipInfo& outInfo)
{
  if (encodedData.empty() || encodedData.size() > MAX_ENCODED_SIZE) {
    return false;
  }
  ma_decoder decoder;
  if (!openDecoder(encodedData, decoder)) {
    return false;
  }
  const bool bRead = readDecoderInfo(decoder, outInfo);
  ma_decoder_uninit(&decoder);
  return bRead;
}

/*
 */
SPtr<AudioClip>
AudioClip::create(Vector<uint8> encodedData, const AudioClipSettings& settings)
{
  if (encodedData.empty() || encodedData.size() > MAX_ENCODED_SIZE) {
    CH_LOG_ERROR(AudioSystem, "A sound of {0} bytes cannot be a clip.", encodedData.size());
    return nullptr;
  }

  ma_decoder decoder;
  if (!openDecoder(encodedData, decoder)) {
    CH_LOG_ERROR(AudioSystem, "The data is not a WAV, Ogg Vorbis, MP3 or FLAC file.");
    return nullptr;
  }

  // The constructor is private, so chMakeShared cannot reach it.
  SPtr<AudioClip> clip(new AudioClip());
  bool bValid = readDecoderInfo(decoder, clip->m_info);
  if (!bValid) {
    CH_LOG_ERROR(AudioSystem, "The sound file has no length, channels or rate this build reads.");
  }

  if (bValid && settings.loadMode == AudioClipLoadMode::Decompress) {
    const uint64 sampleCount = clip->m_info.frameCount * clip->m_info.channels;
    if (sampleCount * sizeof(float) > MAX_DECODED_SIZE) {
      CH_LOG_ERROR(AudioSystem,
                   "A sound of {0:.1f} seconds is too long to decode at load; stream it.",
                   clip->m_info.getDuration());
      bValid = false;
    }
    else {
      clip->m_samples.resize(static_cast<SIZE_T>(sampleCount));
      ma_uint64 framesRead = 0;
      if (ma_decoder_read_pcm_frames(&decoder, clip->m_samples.data(), clip->m_info.frameCount,
                                     &framesRead) != MA_SUCCESS ||
          framesRead == 0) {
        CH_LOG_ERROR(AudioSystem, "The sound file could not be decoded.");
        bValid = false;
      }
      else if (framesRead < clip->m_info.frameCount) {
        // Some MP3 lengths are estimates; the samples decoded are the truth.
        clip->m_info.frameCount = framesRead;
        clip->m_samples.resize(static_cast<SIZE_T>(framesRead * clip->m_info.channels));
      }
    }
  }
  ma_decoder_uninit(&decoder);

  if (!bValid) {
    return nullptr;
  }
  clip->m_encodedData = std::move(encodedData);
  clip->m_settings = settings;
  return clip;
}

} // namespace chEngineSDK
