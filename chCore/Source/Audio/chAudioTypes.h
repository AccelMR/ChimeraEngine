/************************************************************************/
/**
 * @file chAudioTypes.h
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Settings shared by audio clips, voices and the audio manager.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chLogDeclaration.h"
#include "chVector3.h"

namespace chEngineSDK {

CH_LOG_DECLARE_EXTERN(CH_CORE_EXPORT, AudioSystem);

// Stored in clip assets, so the values are fixed.
enum class AudioClipLoadMode : uint32
{
  // Decoded to samples when the clip loads: short sounds played often.
  Decompress = 0,
  // Kept encoded and decoded while it plays, by each voice: music and long sounds.
  Stream = 1,
  COUNT
};

// Stored in clip assets, so the values are fixed.
enum class AudioBus : uint32
{
  Sfx = 0,
  Music = 1,
  COUNT
};

// How a spatial voice gets quieter with distance. Fixed values (saved by components).
enum class AudioAttenuation : uint32
{
  None = 0,
  Inverse = 1,
  Linear = 2,
  Exponential = 3,
  COUNT
};

struct AudioClipInfo
{
  NODISCARD FORCEINLINE float
  getDuration() const noexcept
  {
    return sampleRate == 0 ? 0.0f
                           : static_cast<float>(static_cast<double>(frameCount) / sampleRate);
  }

  uint32 channels = 0;
  uint32 sampleRate = 0;
  uint64 frameCount = 0;
};

struct AudioClipSettings
{
  AudioClipLoadMode loadMode = AudioClipLoadMode::Decompress;
  AudioBus bus = AudioBus::Sfx;
  // Multiplies the volume every voice of the clip is played with.
  float volume = 1.0f;
  // Voices of this clip playing at once; a new one stops the oldest. 0 means no limit.
  uint32 maxVoices = 0;
};

// How a spatial voice is heard from the listener. Positions are in world units.
struct AudioSpatialSettings
{
  AudioAttenuation attenuation = AudioAttenuation::Inverse;
  // Full volume up to here.
  float minDistance = 1.0f;
  // No quieter past here.
  float maxDistance = 100.0f;
  float rolloff = 1.0f;
  // 0 turns the pitch shift of moving voices off.
  float dopplerFactor = 1.0f;
  // Full angles in degrees around the voice's direction; 360 hears it the same everywhere.
  float coneInnerAngle = 360.0f;
  float coneOuterAngle = 360.0f;
  // Volume outside the outer cone.
  float coneOuterGain = 0.0f;
};

struct AudioPlayDesc
{
  // Multiplied by the clip's own volume.
  float volume = 1.0f;
  // 1 plays at its speed; 2 an octave higher and twice as fast.
  float pitch = 1.0f;
  // -1 left, 1 right; only for voices that are not spatial.
  float pan = 0.0f;
  bool bLoop = false;
  // Placed in the world and heard from the listener; otherwise straight to the speakers.
  bool bSpatial = false;
  Vector3 position = Vector3::ZERO;
  Vector3 velocity = Vector3::ZERO;
  // Where the cone points.
  Vector3 direction = Vector3::FORWARD;
  AudioSpatialSettings spatial{};
  // Seconds of fade in; 0 starts at full volume.
  float fadeIn = 0.0f;
};

/**
 * Exists so a playing sound can be changed or stopped without a pointer into the manager's
 * pool: a voice that ended and was reused for another sound has another generation, so an
 * old handle simply stops working.
 */
struct AudioVoice
{
  NODISCARD FORCEINLINE bool
  isValid() const noexcept
  {
    return index != INVALID_INDEX;
  }

  uint32 index = INVALID_INDEX;
  uint32 generation = 0;

  static constexpr uint32 INVALID_INDEX = 0xFFFFFFFFu;
};

} // namespace chEngineSDK
