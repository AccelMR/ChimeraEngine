/************************************************************************/
/**
 * @file chAudioManager.h
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Plays audio clips on the sound device.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chAudioTypes.h"
#include "chModule.h"

namespace chEngineSDK {

class AudioClip;
class Camera;

struct AudioManagerDesc
{
  // Mixes without a sound device; readMix gives the result (tests, servers).
  bool bNoDevice = false;
  uint32 maxVoices = 64;
  // Only used without a device; with one, the device decides.
  uint32 channels = 2;
  uint32 sampleRate = 48000;
};

/**
 * Exists to own the sound device and a fixed pool of voices, so playing a sound in a frame
 * full of them (a bullet hell's hits) costs no allocation: a voice keeps its mixer state
 * while the next clip it plays has the same channels and rate. When the pool is full, or a
 * clip reached its own voice limit, the oldest voice is stopped. Voices go through a bus
 * (Sfx, Music) and the master volume. Spatial voices are heard from one listener, which the
 * game moves with its camera. Main thread only.
 */
class CH_CORE_EXPORT AudioManager : public Module<AudioManager>
{
 public:
  /**
   * Reads Audio.Enabled, Audio.MaxVoices and, every update, the bus volume CVars. Without
   * a device (disabled, or none could be opened) everything still works, silently.
   */
  AudioManager();

  /**
   * Volumes are only what setBusVolume and setMasterVolume give (tests).
   */
  explicit AudioManager(const AudioManagerDesc& desc);

  ~AudioManager() override;

  /**
   * Once per frame: applies volume changes and frees the voices that ended.
   */
  void
  update();

  /**
   * An invalid handle when the clip is null.
   */
  AudioVoice
  play(const SPtr<const AudioClip>& clip, const AudioPlayDesc& desc = {});

  void
  stop(AudioVoice voice, float fadeOutSeconds = 0.0f);

  void
  stopAll();

  /**
   * True while it plays or is paused; false once it ended, was stopped or reused.
   */
  NODISCARD bool
  isPlaying(AudioVoice voice) const;

  void
  setPaused(AudioVoice voice, bool bPaused);

  void
  setVolume(AudioVoice voice, float volume);

  void
  setPitch(AudioVoice voice, float pitch);

  void
  setPan(AudioVoice voice, float pan);

  void
  setLooping(AudioVoice voice, bool bLoop);

  void
  setPosition(AudioVoice voice, const Vector3& position, const Vector3& velocity);

  void
  setDirection(AudioVoice voice, const Vector3& direction);

  void
  setSpatialSettings(AudioVoice voice, const AudioSpatialSettings& settings);

  /**
   * Seconds since the voice started, wrapped when it loops; 0 for a voice that is not playing.
   */
  NODISCARD float
  getTime(AudioVoice voice) const;

  void
  setTime(AudioVoice voice, float seconds);

  /**
   * Pauses or resumes every voice at once (a paused game).
   */
  void
  setAllPaused(bool bPaused);

  NODISCARD bool
  isAllPaused() const noexcept;

  void
  setListener(const Vector3& position,
              const Vector3& forward,
              const Vector3& up,
              const Vector3& velocity);

  /**
   * Puts the listener at the camera, its velocity from how far the camera moved since the
   * last call (a jump of TELEPORT_DISTANCE or more counts as standing still).
   */
  void
  updateListener(const Camera& camera, float deltaTime);

  void
  setBusVolume(AudioBus bus, float volume);

  NODISCARD float
  getBusVolume(AudioBus bus) const;

  void
  setMasterVolume(float volume);

  NODISCARD float
  getMasterVolume() const;

  NODISCARD bool
  hasDevice() const noexcept;

  NODISCARD uint32
  getActiveVoiceCount() const noexcept;

  NODISCARD uint32
  getMaxVoices() const noexcept;

  NODISCARD uint32
  getChannels() const noexcept;

  NODISCARD uint32
  getSampleRate() const noexcept;

  /**
   * Mixes frameCount frames into output (frames times channels floats). Only without a
   * device; with one, the device pulls the mix itself and this gives 0.
   */
  uint64
  readMix(Span<float> output, uint64 frameCount);

  static constexpr uint32 MAX_VOICES = 1024;
  // World units moved in one frame past which a listener or source is taken to have jumped
  // (a map change), so it gets no doppler shift.
  static constexpr float TELEPORT_DISTANCE = 50.0f;

 private:
  struct Impl;
  UniquePtr<Impl> m_impl;
  bool m_bReadSettings = false;
};

} // namespace chEngineSDK
