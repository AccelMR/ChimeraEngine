/************************************************************************/
/**
 * @file chAudioTestHelpers.h
 * @author AccelMR
 * @date 2026/11/17
 * @brief Helpers that the audio and script audio tests share.
 */
/************************************************************************/
#pragma once

#include "chAudioClipAsset.h"
#include "chAudioManager.h"
#include "chStringUtils.h"
#include "chUUID.h"

#include "catch.hpp"

namespace chEngineSDK {

inline void
appendLittleEndian(Vector<uint8>& bytes, uint32 value, uint32 byteCount)
{
  for (uint32 i = 0; i < byteCount; ++i) {
    bytes.push_back(static_cast<uint8>((value >> (i * 8)) & 0xFFu));
  }
}

inline void
appendText(Vector<uint8>& bytes, StringView text)
{
  for (const ANSICHAR c : text) {
    bytes.push_back(static_cast<uint8>(c));
  }
}

// Half of the 16-bit range.
inline constexpr int16 kTestAudioValue = 16384;

/**
 * A 16-bit PCM WAV file whose every sample is value, so the mix is easy to check.
 */
inline Vector<uint8>
makeTestWav(uint32 channels, uint32 sampleRate, uint32 frameCount, int16 value)
{
  const uint32 dataSize = frameCount * channels * 2;
  Vector<uint8> bytes;
  bytes.reserve(44 + dataSize);
  appendText(bytes, "RIFF");
  appendLittleEndian(bytes, 36 + dataSize, 4);
  appendText(bytes, "WAVEfmt ");
  appendLittleEndian(bytes, 16, 4);
  appendLittleEndian(bytes, 1, 2);
  appendLittleEndian(bytes, channels, 2);
  appendLittleEndian(bytes, sampleRate, 4);
  appendLittleEndian(bytes, sampleRate * channels * 2, 4);
  appendLittleEndian(bytes, channels * 2, 2);
  appendLittleEndian(bytes, 16, 2);
  appendText(bytes, "data");
  appendLittleEndian(bytes, dataSize, 4);
  const uint32 sample = static_cast<uint32>(static_cast<uint16>(value));
  for (uint32 i = 0; i < frameCount * channels; ++i) {
    appendLittleEndian(bytes, sample, 2);
  }
  return bytes;
}

inline constexpr uint32 kTestAudioRate = 48000;

/*
 * The component and script tests need the module; it cannot start twice, so it starts once
 * (without a device) and stays.
 */
inline AudioManager&
getTestAudioManager()
{
  if (!AudioManager::isStarted()) {
    AudioManager::startUp(AudioManagerDesc{
        .bNoDevice = true, .maxVoices = 16, .channels = 2, .sampleRate = kTestAudioRate});
  }
  AudioManager& audio = AudioManager::instance();
  audio.stopAll();
  audio.update();
  audio.setListener(Vector3::ZERO, Vector3::FORWARD, Vector3::UP, Vector3::ZERO);
  return audio;
}

inline SPtr<AudioClipAsset>
makeTestClipAsset(const ANSICHAR* name, uint32 frameCount)
{
  AssetMetadata metadata;
  metadata.uuid = chEngineSDK::UUID::createRandom();
  metadata.assetType = AssetTypeTraits<AudioClipAsset>::getTypeId();
  StringUtils::copyToBuffer(metadata.name, name);
  return chMakeShared<AudioClipAsset>(
      metadata, makeTestWav(1, kTestAudioRate, frameCount, kTestAudioValue),
      AudioClipSettings{});
}

} // namespace chEngineSDK
