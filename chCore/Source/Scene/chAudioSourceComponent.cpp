/************************************************************************/
/**
 * @file chAudioSourceComponent.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Plays an audio clip from a GameObject.
 */
/************************************************************************/
#include "chAudioSourceComponent.h"

#include "chAssetFile.h"
#include "chAssetManager.h"
#include "chAudioClipAsset.h"
#include "chAudioManager.h"
#include "chGameObject.h"
#include "chLogger.h"
#include "chMath.h"
#include "chScene.h"
#include "chSceneManager.h"

namespace chEngineSDK {

namespace {
constexpr uint8 kLoopBit = 0x1u;
constexpr uint8 kPlayOnStartBit = 0x2u;
constexpr uint8 kSpatialBit = 0x4u;

struct AudioSourceFileData
{
  UUID clip;
  float volume;
  float pitch;
  float pan;
  float minDistance;
  float maxDistance;
  float rolloff;
  float dopplerFactor;
  float coneInnerAngle;
  float coneOuterAngle;
  float coneOuterGain;
  uint8 attenuation;
  uint8 flags;
  uint8 padding[6];
};
static_assert(sizeof(AudioSourceFileData) == 64, "AudioSourceFileData must have no padding");

NODISCARD SPtr<AudioClipAsset>
findClip(const UUID& id)
{
  if (id.isNull() || !AssetManager::isStarted()) {
    return nullptr;
  }
  const SPtr<IAsset> asset = AssetManager::instance().getAsset(id);
  return asset ? asset->as<AudioClipAsset>() : nullptr;
}

NODISCARD FORCEINLINE AudioManager*
getAudio() noexcept
{
  return AudioManager::isStarted() ? &AudioManager::instance() : nullptr;
}

NODISCARD FORCEINLINE Vector3
getRow(const Matrix4& matrix, int32 row) noexcept
{
  const float* values = matrix.getRow(row);
  return Vector3(values[0], values[1], values[2]);
}
} // namespace

/*
 */
AudioSourceComponent::AudioSourceComponent()
{
  setUpdateFlags(ComponentUpdate::Frame);
  // So a sound started by hand in the editor (a preview) still follows its object; play on
  // start waits for a playing scene.
  setUpdatesWhileEditing(true);
}

/*
 */
AudioSourceComponent::AudioSourceComponent(const SPtr<AudioClipAsset>& clip)
  : AudioSourceComponent()
{
  m_clip = clip;
  m_clipId = clip ? clip->getUUID() : UUID::null();
}

/*
 */
AudioSourceComponent::~AudioSourceComponent()
{
  if (AudioManager* audio = getAudio()) {
    audio->stop(m_voice);
  }
}

/*
 */
void
AudioSourceComponent::setClip(const SPtr<AudioClipAsset>& clip)
{
  stop();
  m_clip = clip;
  m_clipId = clip ? clip->getUUID() : UUID::null();
}

/*
 */
void
AudioSourceComponent::setClipId(const UUID& clipId)
{
  stop();
  m_clip = findClip(clipId);
  m_clipId = clipId;
}

/*
 */
void
AudioSourceComponent::play()
{
  AudioManager* audio = getAudio();
  if (!audio || !isRegistered() || !m_clip) {
    return;
  }
  if (!m_clip->isLoaded() && AssetManager::isStarted() &&
      !AssetManager::instance().syncLoadAsset(m_clip)) {
    return;
  }
  audio->stop(m_voice);
  m_lastPosition = getWorldPosition();
  const Matrix4& world = getOwner()->getTransform().getWorldMatrix();
  m_voice = audio->play(m_clip->getClip(),
                        {.volume = m_volume,
                         .pitch = m_pitch,
                         .pan = m_pan,
                         .bLoop = m_bLoop,
                         .bSpatial = m_bSpatial,
                         .position = m_lastPosition,
                         .velocity = Vector3::ZERO,
                         .direction = getRow(world, 0).getNormalized(),
                         .spatial = m_spatial});
  m_bPaused = false;
}

/*
 */
void
AudioSourceComponent::stop(float fadeOutSeconds)
{
  if (AudioManager* audio = getAudio()) {
    audio->stop(m_voice, fadeOutSeconds);
  }
  if (fadeOutSeconds <= 0.0f) {
    m_voice = {};
  }
  m_bPaused = false;
}

/*
 */
void
AudioSourceComponent::setPaused(bool bPaused)
{
  m_bPaused = bPaused;
  if (AudioManager* audio = getAudio()) {
    audio->setPaused(m_voice, bPaused);
  }
}

/*
 */
bool
AudioSourceComponent::isPlaying() const
{
  const AudioManager* audio = getAudio();
  return audio && audio->isPlaying(m_voice);
}

/*
 */
float
AudioSourceComponent::getTime() const
{
  const AudioManager* audio = getAudio();
  return audio ? audio->getTime(m_voice) : 0.0f;
}

/*
 */
void
AudioSourceComponent::setTime(float seconds)
{
  if (AudioManager* audio = getAudio()) {
    audio->setTime(m_voice, seconds);
  }
}

/*
 */
void
AudioSourceComponent::setVolume(float volume)
{
  m_volume = Math::isFinite(volume) ? Math::max(volume, 0.0f) : 0.0f;
  if (AudioManager* audio = getAudio()) {
    audio->setVolume(m_voice, m_volume);
  }
}

/*
 */
void
AudioSourceComponent::setPitch(float pitch)
{
  m_pitch = Math::isFinite(pitch) ? Math::max(pitch, 0.01f) : 1.0f;
  if (AudioManager* audio = getAudio()) {
    audio->setPitch(m_voice, m_pitch);
  }
}

/*
 */
void
AudioSourceComponent::setPan(float pan)
{
  m_pan = Math::isFinite(pan) ? Math::clamp(pan, -1.0f, 1.0f) : 0.0f;
  if (AudioManager* audio = getAudio()) {
    audio->setPan(m_voice, m_pan);
  }
}

/*
 */
void
AudioSourceComponent::setLooping(bool bLoop)
{
  m_bLoop = bLoop;
  if (AudioManager* audio = getAudio()) {
    audio->setLooping(m_voice, bLoop);
  }
}

/*
 */
void
AudioSourceComponent::setSpatialSettings(const AudioSpatialSettings& settings)
{
  m_spatial = settings;
  if (AudioManager* audio = getAudio()) {
    audio->setSpatialSettings(m_voice, settings);
  }
}

/*
 */
void
AudioSourceComponent::update(float deltaTime)
{
  if (!m_bStarted) {
    const Scene* scene = getOwner()->getScene();
    if (scene && scene->isPlaying()) {
      m_bStarted = true;
      if (m_bPlayOnStart) {
        play();
      }
    }
  }
  if (!m_voice.isValid() || !m_bSpatial) {
    return;
  }
  AudioManager* audio = getAudio();
  if (!audio || !audio->isPlaying(m_voice)) {
    m_voice = {};
    return;
  }
  // The world matrix is the one of the last frame (updates run before transforms), so the
  // voice is one frame behind, which nobody hears.
  const Vector3 position = getWorldPosition();
  Vector3 velocity = Vector3::ZERO;
  const Vector3 moved = position - m_lastPosition;
  if (deltaTime > 0.0f &&
      moved.sqrMagnitude() < AudioManager::TELEPORT_DISTANCE * AudioManager::TELEPORT_DISTANCE) {
    velocity = moved / deltaTime;
  }
  m_lastPosition = position;
  audio->setPosition(m_voice, position, velocity);
  const Matrix4& world = getOwner()->getTransform().getWorldMatrix();
  audio->setDirection(m_voice, getRow(world, 0).getNormalized());
}

/*
 */
void
AudioSourceComponent::serialize(DataStream& stream) const
{
  const uint32 version = SERIALIZATION_VERSION;
  const AudioSourceFileData data{
      .clip = m_clipId,
      .volume = m_volume,
      .pitch = m_pitch,
      .pan = m_pan,
      .minDistance = m_spatial.minDistance,
      .maxDistance = m_spatial.maxDistance,
      .rolloff = m_spatial.rolloff,
      .dopplerFactor = m_spatial.dopplerFactor,
      .coneInnerAngle = m_spatial.coneInnerAngle,
      .coneOuterAngle = m_spatial.coneOuterAngle,
      .coneOuterGain = m_spatial.coneOuterGain,
      .attenuation = static_cast<uint8>(m_spatial.attenuation),
      .flags = static_cast<uint8>((m_bLoop ? kLoopBit : 0u) |
                                  (m_bPlayOnStart ? kPlayOnStartBit : 0u) |
                                  (m_bSpatial ? kSpatialBit : 0u)),
      .padding = {0, 0, 0, 0, 0, 0}};
  stream.write(&version, sizeof(version));
  stream.write(&data, sizeof(data));
}

/*
 */
bool
AudioSourceComponent::deserialize(DataStream& stream)
{
  uint32 version = 0;
  AudioSourceFileData data;
  if (!AssetFile::readValue(stream, version) || version != SERIALIZATION_VERSION ||
      !AssetFile::readValue(stream, data) ||
      data.attenuation >= static_cast<uint8>(AudioAttenuation::COUNT) ||
      !Math::isFinite(data.volume) || !Math::isFinite(data.pitch) ||
      !Math::isFinite(data.pan) || !Math::isFinite(data.minDistance) ||
      !Math::isFinite(data.maxDistance)) {
    return false;
  }

  m_volume = Math::max(data.volume, 0.0f);
  m_pitch = Math::max(data.pitch, 0.01f);
  m_pan = Math::clamp(data.pan, -1.0f, 1.0f);
  m_spatial = {.attenuation = static_cast<AudioAttenuation>(data.attenuation),
               .minDistance = data.minDistance,
               .maxDistance = data.maxDistance,
               .rolloff = data.rolloff,
               .dopplerFactor = data.dopplerFactor,
               .coneInnerAngle = data.coneInnerAngle,
               .coneOuterAngle = data.coneOuterAngle,
               .coneOuterGain = data.coneOuterGain};
  m_bLoop = (data.flags & kLoopBit) != 0;
  m_bPlayOnStart = (data.flags & kPlayOnStartBit) != 0;
  m_bSpatial = (data.flags & kSpatialBit) != 0;
  m_clipId = data.clip;
  m_clip = findClip(data.clip);
  if (!m_clip && !data.clip.isNull()) {
    CH_LOG_WARNING(SceneManagerLog, "An audio source lost its clip {0}.", data.clip);
  }
  return true;
}

/*
 */
void
AudioSourceComponent::collectReferences(Vector<UUID>& outReferences) const
{
  if (!m_clipId.isNull()) {
    outReferences.push_back(m_clipId);
  }
}

/*
 */
void
AudioSourceComponent::onRegister()
{
  m_bStarted = false;
  m_lastPosition = getWorldPosition();
}

/*
 */
void
AudioSourceComponent::onUnregister()
{
  stop();
}

/*
 */
Vector3
AudioSourceComponent::getWorldPosition() const
{
  return getRow(getOwner()->getTransform().getWorldMatrix(), 3);
}

} // namespace chEngineSDK
