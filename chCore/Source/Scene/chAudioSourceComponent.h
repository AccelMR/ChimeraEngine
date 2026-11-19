/************************************************************************/
/**
 * @file chAudioSourceComponent.h
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Plays an audio clip from a GameObject.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chAudioTypes.h"
#include "chComponent.h"
#include "chTypeTraits.h"
#include "chUUID.h"

namespace chEngineSDK {

class AudioClipAsset;

/**
 * Exists so a sound belongs to an object of the scene: it is saved with it, can start when
 * the scene plays, and a spatial one is heard from where the object is, moving with it
 * (with doppler from its speed). It plays through the AudioManager's voices; the sound
 * stops when the component leaves the scene (destroyed, disabled, scene closed, Play
 * stopped). Settings changed while it plays reach the voice, except Spatial, which waits
 * for the next play.
 */
class CH_CORE_EXPORT AudioSourceComponent : public Component
{
 public:
  AudioSourceComponent();

  explicit AudioSourceComponent(const SPtr<AudioClipAsset>& clip);

  ~AudioSourceComponent() override;

  /**
   * A new clip stops the one playing.
   */
  void
  setClip(const SPtr<AudioClipAsset>& clip);

  /**
   * Looks the clip up by UUID; kept even when there is no such asset, so saving keeps it.
   */
  void
  setClipId(const UUID& clipId);

  NODISCARD FORCEINLINE const UUID&
  getClipId() const noexcept
  {
    return m_clipId;
  }

  NODISCARD FORCEINLINE const SPtr<AudioClipAsset>&
  getClipAsset() const noexcept
  {
    return m_clip;
  }

  /**
   * Plays the clip from its start (again when it already plays). Only while registered;
   * does nothing without a clip or an AudioManager.
   */
  void
  play();

  void
  stop(float fadeOutSeconds = 0.0f);

  void
  setPaused(bool bPaused);

  NODISCARD FORCEINLINE bool
  isPaused() const noexcept
  {
    return m_bPaused;
  }

  /**
   * True while its sound plays or is paused.
   */
  NODISCARD bool
  isPlaying() const;

  NODISCARD float
  getTime() const;

  void
  setTime(float seconds);

  void
  setVolume(float volume);

  NODISCARD FORCEINLINE float
  getVolume() const noexcept
  {
    return m_volume;
  }

  void
  setPitch(float pitch);

  NODISCARD FORCEINLINE float
  getPitch() const noexcept
  {
    return m_pitch;
  }

  /**
   * -1 left, 1 right; only heard when not spatial.
   */
  void
  setPan(float pan);

  NODISCARD FORCEINLINE float
  getPan() const noexcept
  {
    return m_pan;
  }

  void
  setLooping(bool bLoop);

  NODISCARD FORCEINLINE bool
  isLooping() const noexcept
  {
    return m_bLoop;
  }

  /**
   * Starts the clip the first time the scene updates while playing.
   */
  FORCEINLINE void
  setPlayOnStart(bool bPlayOnStart) noexcept
  {
    m_bPlayOnStart = bPlayOnStart;
  }

  NODISCARD FORCEINLINE bool
  isPlayOnStart() const noexcept
  {
    return m_bPlayOnStart;
  }

  FORCEINLINE void
  setSpatial(bool bSpatial) noexcept
  {
    m_bSpatial = bSpatial;
  }

  NODISCARD FORCEINLINE bool
  isSpatial() const noexcept
  {
    return m_bSpatial;
  }

  void
  setSpatialSettings(const AudioSpatialSettings& settings);

  NODISCARD FORCEINLINE const AudioSpatialSettings&
  getSpatialSettings() const noexcept
  {
    return m_spatial;
  }

  NODISCARD FORCEINLINE AudioVoice
  getVoice() const noexcept
  {
    return m_voice;
  }

  void
  update(float deltaTime) override;

  void
  serialize(DataStream& stream) const override;

  NODISCARD bool
  deserialize(DataStream& stream) override;

  void
  collectReferences(Vector<UUID>& outReferences) const override;

  static constexpr uint32 SERIALIZATION_VERSION = 1;

 protected:
  void
  onRegister() override;

  void
  onUnregister() override;

 private:
  NODISCARD Vector3
  getWorldPosition() const;

  SPtr<AudioClipAsset> m_clip;
  UUID m_clipId = UUID::null();
  AudioSpatialSettings m_spatial{};
  AudioVoice m_voice;
  Vector3 m_lastPosition = Vector3::ZERO;
  float m_volume = 1.0f;
  float m_pitch = 1.0f;
  float m_pan = 0.0f;
  bool m_bLoop = false;
  bool m_bPlayOnStart = true;
  bool m_bSpatial = false;
  bool m_bPaused = false;
  // Set once play on start was tried in this registration.
  bool m_bStarted = false;
};

DECLARE_TYPE_TRAITS(AudioSourceComponent)

} // namespace chEngineSDK
