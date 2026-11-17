/************************************************************************/
/**
 * @file chAudioManager.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Plays audio clips on the sound device.
 */
/************************************************************************/
#include "chAudioManager.h"

#include "chAudioClip.h"
#include "chCamera.h"
#include "chConsoleVariable.h"
#include "chLogger.h"
#include "chMath.h"
#include "chSTDThreading.h"

#include "miniaudio.h"

namespace chEngineSDK {

CH_LOG_DEFINE_CATEGORY_SHARED(AudioSystem, Info);

namespace {
ConsoleVariable<bool> g_cvarAudioEnabled(
    "Audio.Enabled", true,
    "Opens the sound device. Off, sounds still play their logic but nothing is heard. Read at "
    "start up.");
ConsoleVariable<int32> g_cvarMaxVoices(
    "Audio.MaxVoices", 64,
    "Sounds that can play at once; a new one past it stops the oldest. Read at start up.");
ConsoleVariable<float> g_cvarMasterVolume("Audio.MasterVolume", 1.0f,
                                          "Volume of everything, 0 to 1.");
ConsoleVariable<float> g_cvarSfxVolume("Audio.SfxVolume", 1.0f,
                                       "Volume of the sound effects bus, 0 to 1.");
ConsoleVariable<float> g_cvarMusicVolume("Audio.MusicVolume", 1.0f,
                                         "Volume of the music bus, 0 to 1.");

// The mixer may still be inside a pass that read a voice when it stops; two finished passes
// after it, nothing reads the voice's data any more.
constexpr uint64 kMixPassesBeforeReuse = 2;

/**
 * Engine axes (X forward, Y right, Z up) to the mixer's (-Z forward, X right, Y up): only
 * the names of the axes change, so forward, right and up mean the same.
 */
FORCEINLINE void
toMixerSpace(const Vector3& value, float& outX, float& outY, float& outZ) noexcept
{
  outX = value.y;
  outY = value.z;
  outZ = -value.x;
}

NODISCARD ma_attenuation_model
toMixerAttenuation(AudioAttenuation attenuation) noexcept
{
  switch (attenuation) {
  case AudioAttenuation::None: return ma_attenuation_model_none;
  case AudioAttenuation::Linear: return ma_attenuation_model_linear;
  case AudioAttenuation::Exponential: return ma_attenuation_model_exponential;
  case AudioAttenuation::Inverse:
  default: return ma_attenuation_model_inverse;
  }
}

NODISCARD FORCEINLINE float
clampVolume(float volume) noexcept
{
  return Math::isFinite(volume) ? Math::max(volume, 0.0f) : 0.0f;
}

/**
 * One slot of the pool. Its sound stays made while it plays clips of the same channels and
 * rate, so reusing it allocates nothing; streamed clips need a decoder of their own and are
 * made and freed per play.
 */
struct AudioVoiceSlot
{
  enum class State : uint8
  {
    Free,
    Playing,
    // Ended or stopped; kept until the mixer cannot be reading it.
    Retiring
  };

  ma_sound sound;
  ma_audio_buffer_ref bufferRef;
  ma_decoder decoder;
  SPtr<const AudioClip> clip;
  uint64 startOrder = 0;
  uint64 retireMixPass = 0;
  uint32 generation = 0;
  uint32 soundChannels = 0;
  uint32 soundSampleRate = 0;
  AudioBus bus = AudioBus::Sfx;
  State state = State::Free;
  bool bSoundMade = false;
  bool bStreaming = false;
  bool bPaused = false;
  bool bPausedByAll = false;
  bool bStopRequested = false;
};
} // namespace

struct AudioManager::Impl
{
  ma_engine engine;
  ma_sound_group buses[static_cast<uint32>(AudioBus::COUNT)];
  Vector<AudioVoiceSlot> voices;
  // Indices of Playing and Retiring voices.
  Vector<uint32> active;
  Vector<uint32> freeVoices;
  Atomic<uint64> mixPasses{0};
  uint64 nextStartOrder = 0;
  Vector3 lastListenerPosition = Vector3::ZERO;
  bool bListenerPlaced = false;
  float busVolumes[static_cast<uint32>(AudioBus::COUNT)] = {1.0f, 1.0f};
  float masterVolume = 1.0f;
  bool bEngineReady = false;
  bool bBusesReady = false;
  bool bHasDevice = false;
  bool bAllPaused = false;

  static void
  onMixPass(void* userData, float* frames, ma_uint64 frameCount)
  {
    CH_PARAMETER_UNUSED(frames);
    CH_PARAMETER_UNUSED(frameCount);
    static_cast<Impl*>(userData)->mixPasses.fetch_add(1, std::memory_order_release);
  }

  bool
  initialize(const AudioManagerDesc& desc)
  {
    ma_engine_config config = ma_engine_config_init();
    config.listenerCount = 1;
    config.onProcess = &Impl::onMixPass;
    config.pProcessUserData = this;
    if (!desc.bNoDevice) {
      bEngineReady = ma_engine_init(&config, &engine) == MA_SUCCESS;
      bHasDevice = bEngineReady;
      if (!bEngineReady) {
        CH_LOG_WARNING(AudioSystem, "No sound device could be opened; sounds will be silent.");
      }
    }
    if (!bEngineReady) {
      config.noDevice = MA_TRUE;
      config.channels = Math::max(desc.channels, 1u);
      config.sampleRate = Math::max(desc.sampleRate, 8000u);
      bEngineReady = ma_engine_init(&config, &engine) == MA_SUCCESS;
    }
    if (!bEngineReady) {
      CH_LOG_ERROR(AudioSystem, "The audio mixer could not start.");
      return false;
    }

    bBusesReady = true;
    for (ma_sound_group& bus : buses) {
      if (ma_sound_group_init(&engine, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &bus) !=
          MA_SUCCESS) {
        CH_LOG_ERROR(AudioSystem, "The audio buses could not be made.");
        bBusesReady = false;
        return false;
      }
      ma_sound_group_start(&bus);
    }

    const uint32 voiceCount = Math::clamp(desc.maxVoices, 1u, AudioManager::MAX_VOICES);
    voices.resize(voiceCount);
    active.reserve(voiceCount);
    freeVoices.reserve(voiceCount);
    for (uint32 i = voiceCount; i > 0; --i) {
      freeVoices.push_back(i - 1);
    }
    return true;
  }

  void
  destroy()
  {
    for (AudioVoiceSlot& voice : voices) {
      releaseSound(voice);
      voice.clip.reset();
    }
    voices.clear();
    if (bBusesReady) {
      for (ma_sound_group& bus : buses) {
        ma_sound_group_uninit(&bus);
      }
      bBusesReady = false;
    }
    if (bEngineReady) {
      ma_engine_uninit(&engine);
      bEngineReady = false;
    }
  }

  NODISCARD AudioVoiceSlot*
  find(AudioVoice handle)
  {
    if (handle.index >= voices.size()) {
      return nullptr;
    }
    AudioVoiceSlot& voice = voices[handle.index];
    return voice.state == AudioVoiceSlot::State::Playing && voice.generation == handle.generation
               ? &voice
               : nullptr;
  }

  NODISCARD const AudioVoiceSlot*
  find(AudioVoice handle) const
  {
    return const_cast<Impl*>(this)->find(handle);
  }

  void
  releaseSound(AudioVoiceSlot& voice)
  {
    if (voice.bSoundMade) {
      ma_sound_uninit(&voice.sound);
      if (voice.bStreaming) {
        ma_decoder_uninit(&voice.decoder);
      }
      else {
        ma_audio_buffer_ref_uninit(&voice.bufferRef);
      }
    }
    voice.bSoundMade = false;
    voice.bStreaming = false;
    voice.soundChannels = 0;
    voice.soundSampleRate = 0;
  }

  /**
   * Points the voice at the clip, keeping its sound when the format matches.
   */
  NODISCARD bool
  bindClip(AudioVoiceSlot& voice, const AudioClip& clip)
  {
    const AudioClipInfo& info = clip.getInfo();
    const AudioBus bus = clip.getSettings().bus;
    ma_sound_group* group = &buses[static_cast<uint32>(bus)];

    if (clip.isStreamed()) {
      releaseSound(voice);
      const Span<const uint8> data = clip.getEncodedData();
      const ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
      if (ma_decoder_init_memory(data.data(), data.size(), &config, &voice.decoder) !=
          MA_SUCCESS) {
        return false;
      }
      if (ma_sound_init_from_data_source(&engine, &voice.decoder, 0, group, &voice.sound) !=
          MA_SUCCESS) {
        ma_decoder_uninit(&voice.decoder);
        return false;
      }
      voice.bSoundMade = true;
      voice.bStreaming = true;
      voice.bus = bus;
      return true;
    }

    const Span<const float> samples = clip.getSamples();
    if (voice.bSoundMade && !voice.bStreaming && voice.soundChannels == info.channels &&
        voice.soundSampleRate == info.sampleRate) {
      ma_audio_buffer_ref_set_data(&voice.bufferRef, samples.data(), info.frameCount);
      ma_sound_reset_stop_time_and_fade(&voice.sound);
      if (voice.bus != bus) {
        ma_node_attach_output_bus(&voice.sound, 0, group, 0);
        voice.bus = bus;
      }
      return true;
    }

    releaseSound(voice);
    if (ma_audio_buffer_ref_init(ma_format_f32, info.channels, samples.data(), info.frameCount,
                                 &voice.bufferRef) != MA_SUCCESS) {
      return false;
    }
    // The buffer only knows its samples; the mixer converts from this rate.
    voice.bufferRef.sampleRate = info.sampleRate;
    if (ma_sound_init_from_data_source(&engine, &voice.bufferRef, 0, group, &voice.sound) !=
        MA_SUCCESS) {
      ma_audio_buffer_ref_uninit(&voice.bufferRef);
      return false;
    }
    voice.bSoundMade = true;
    voice.soundChannels = info.channels;
    voice.soundSampleRate = info.sampleRate;
    voice.bus = bus;
    return true;
  }

  void
  applySpatialSettings(AudioVoiceSlot& voice, const AudioSpatialSettings& settings)
  {
    ma_sound* sound = &voice.sound;
    ma_sound_set_attenuation_model(sound, toMixerAttenuation(settings.attenuation));
    const float minDistance = Math::max(settings.minDistance, 0.0f);
    ma_sound_set_min_distance(sound, minDistance);
    ma_sound_set_max_distance(sound, Math::max(settings.maxDistance, minDistance));
    ma_sound_set_rolloff(sound, Math::max(settings.rolloff, 0.0f));
    ma_sound_set_doppler_factor(sound, Math::max(settings.dopplerFactor, 0.0f));
    const float inner = Math::clamp(settings.coneInnerAngle, 0.0f, 360.0f);
    const float outer = Math::clamp(settings.coneOuterAngle, inner, 360.0f);
    ma_sound_set_cone(sound, inner * Math::DEG2RAD, outer * Math::DEG2RAD,
                      Math::clamp(settings.coneOuterGain, 0.0f, 1.0f));
  }

  void
  setPosition(AudioVoiceSlot& voice, const Vector3& position, const Vector3& velocity)
  {
    float x, y, z;
    toMixerSpace(position, x, y, z);
    ma_sound_set_position(&voice.sound, x, y, z);
    toMixerSpace(velocity, x, y, z);
    ma_sound_set_velocity(&voice.sound, x, y, z);
  }

  void
  setDirection(AudioVoiceSlot& voice, const Vector3& direction)
  {
    float x, y, z;
    toMixerSpace(direction, x, y, z);
    ma_sound_set_direction(&voice.sound, x, y, z);
  }

  void
  applyVoicePause(AudioVoiceSlot& voice)
  {
    if (voice.bPaused || voice.bPausedByAll) {
      ma_sound_stop(&voice.sound);
    }
    else {
      ma_sound_start(&voice.sound);
    }
  }

  /**
   * Stops a voice now and frees it when the mixer is done with it.
   */
  void
  retire(AudioVoiceSlot& voice)
  {
    if (voice.bSoundMade) {
      ma_sound_stop(&voice.sound);
    }
    voice.state = AudioVoiceSlot::State::Retiring;
    voice.retireMixPass = mixPasses.load(std::memory_order_acquire);
  }

  NODISCARD bool
  canFree(const AudioVoiceSlot& voice) const
  {
    // Without a device nothing mixes on another thread.
    return !bHasDevice || mixPasses.load(std::memory_order_acquire) >=
                              voice.retireMixPass + kMixPassesBeforeReuse;
  }

  void
  freeVoice(uint32 index)
  {
    AudioVoiceSlot& voice = voices[index];
    if (voice.bStreaming) {
      releaseSound(voice);
    }
    voice.clip.reset();
    voice.state = AudioVoiceSlot::State::Free;
    freeVoices.push_back(index);
  }

  void
  removeActive(uint32 index)
  {
    for (SIZE_T i = 0; i < active.size(); ++i) {
      if (active[i] == index) {
        active[i] = active.back();
        active.pop_back();
        return;
      }
    }
  }

  /**
   * Takes a voice out of play at once to give it to another sound. The mixer may be reading
   * it, so its sound is made again from nothing (ma_sound_uninit detaches it safely).
   */
  void
  steal(uint32 index)
  {
    AudioVoiceSlot& voice = voices[index];
    releaseSound(voice);
    voice.clip.reset();
    voice.state = AudioVoiceSlot::State::Free;
    removeActive(index);
  }

  NODISCARD uint32
  takeVoice(const AudioClip* clip, uint32 clipLimit)
  {
    constexpr uint32 kNone = AudioVoice::INVALID_INDEX;
    uint32 oldestOfClip = kNone;
    uint32 clipVoices = 0;
    uint32 oldestPlaying = kNone;
    uint32 retiringReady = kNone;
    for (const uint32 index : active) {
      const AudioVoiceSlot& voice = voices[index];
      if (voice.state == AudioVoiceSlot::State::Retiring) {
        if (retiringReady == kNone && canFree(voice)) {
          retiringReady = index;
        }
        continue;
      }
      if (oldestPlaying == kNone || voice.startOrder < voices[oldestPlaying].startOrder) {
        oldestPlaying = index;
      }
      if (clipLimit > 0 && voice.clip.get() == clip) {
        ++clipVoices;
        if (oldestOfClip == kNone || voice.startOrder < voices[oldestOfClip].startOrder) {
          oldestOfClip = index;
        }
      }
    }

    if (clipLimit > 0 && clipVoices >= clipLimit) {
      steal(oldestOfClip);
      return oldestOfClip;
    }
    if (!freeVoices.empty()) {
      const uint32 index = freeVoices.back();
      freeVoices.pop_back();
      return index;
    }
    if (retiringReady != kNone) {
      removeActive(retiringReady);
      AudioVoiceSlot& voice = voices[retiringReady];
      if (voice.bStreaming) {
        releaseSound(voice);
      }
      voice.clip.reset();
      voice.state = AudioVoiceSlot::State::Free;
      return retiringReady;
    }
    if (oldestPlaying != kNone) {
      steal(oldestPlaying);
      return oldestPlaying;
    }
    // Every voice is retiring and the mixer has not passed them yet.
    const uint32 index = active.front();
    steal(index);
    return index;
  }
};

/*
 */
AudioManager::AudioManager()
  : m_impl(chMakeUnique<Impl>()),
    m_bReadSettings(true)
{
  const AudioManagerDesc desc{
      .bNoDevice = !g_cvarAudioEnabled.get(),
      .maxVoices = static_cast<uint32>(Math::max(g_cvarMaxVoices.get(), 1))};
  if (m_impl->initialize(desc)) {
    CH_LOG_INFO(AudioSystem, "Audio started: {0} Hz, {1} channels, {2} voices{3}.",
                getSampleRate(), getChannels(), getMaxVoices(),
                hasDevice() ? "" : ", no device");
  }
  update();
}

/*
 */
AudioManager::AudioManager(const AudioManagerDesc& desc)
  : m_impl(chMakeUnique<Impl>())
{
  CH_PARAMETER_UNUSED(m_impl->initialize(desc));
}

/*
 */
AudioManager::~AudioManager()
{
  m_impl->destroy();
}

/*
 */
void
AudioManager::update()
{
  Impl& impl = *m_impl;
  if (!impl.bEngineReady) {
    return;
  }

  if (m_bReadSettings) {
    setMasterVolume(g_cvarMasterVolume.get());
    setBusVolume(AudioBus::Sfx, g_cvarSfxVolume.get());
    setBusVolume(AudioBus::Music, g_cvarMusicVolume.get());
  }

  for (SIZE_T i = 0; i < impl.active.size();) {
    const uint32 index = impl.active[i];
    AudioVoiceSlot& voice = impl.voices[index];
    if (voice.state == AudioVoiceSlot::State::Playing) {
      const bool bEnded = ma_sound_at_end(&voice.sound) ||
                          (voice.bStopRequested && !ma_sound_is_playing(&voice.sound));
      if (bEnded) {
        impl.retire(voice);
      }
    }
    if (voice.state == AudioVoiceSlot::State::Retiring && impl.canFree(voice)) {
      impl.active[i] = impl.active.back();
      impl.active.pop_back();
      impl.freeVoice(index);
      continue;
    }
    ++i;
  }
}

/*
 */
AudioVoice
AudioManager::play(const SPtr<const AudioClip>& clip, const AudioPlayDesc& desc)
{
  Impl& impl = *m_impl;
  if (!clip || !impl.bBusesReady) {
    return {};
  }

  const AudioClipSettings& settings = clip->getSettings();
  const uint32 index = impl.takeVoice(clip.get(), settings.maxVoices);
  AudioVoiceSlot& voice = impl.voices[index];
  if (!impl.bindClip(voice, *clip)) {
    CH_LOG_ERROR(AudioSystem, "A voice could not play the clip.");
    voice.state = AudioVoiceSlot::State::Free;
    impl.freeVoices.push_back(index);
    return {};
  }

  ma_sound* sound = &voice.sound;
  ma_sound_set_volume(sound, clampVolume(desc.volume * settings.volume));
  ma_sound_set_pitch(sound, Math::max(desc.pitch, 0.01f));
  ma_sound_set_looping(sound, desc.bLoop ? MA_TRUE : MA_FALSE);
  ma_sound_set_spatialization_enabled(sound, desc.bSpatial ? MA_TRUE : MA_FALSE);
  if (desc.bSpatial) {
    ma_sound_set_positioning(sound, ma_positioning_absolute);
    ma_sound_set_pan(sound, 0.0f);
    impl.applySpatialSettings(voice, desc.spatial);
    impl.setPosition(voice, desc.position, desc.velocity);
    impl.setDirection(voice, desc.direction);
  }
  else {
    ma_sound_set_pan(sound, Math::clamp(desc.pan, -1.0f, 1.0f));
  }
  if (desc.fadeIn > 0.0f) {
    ma_sound_set_fade_in_milliseconds(sound, 0.0f, 1.0f,
                                      static_cast<ma_uint64>(desc.fadeIn * 1000.0f));
  }
  else {
    ma_sound_reset_fade(sound);
  }

  voice.clip = clip;
  voice.state = AudioVoiceSlot::State::Playing;
  voice.startOrder = impl.nextStartOrder++;
  voice.bPaused = false;
  voice.bPausedByAll = false;
  voice.bStopRequested = false;
  ++voice.generation;
  impl.active.push_back(index);
  ma_sound_start(sound);
  return {.index = index, .generation = voice.generation};
}

/*
 */
void
AudioManager::stop(AudioVoice voice, float fadeOutSeconds)
{
  AudioVoiceSlot* slot = m_impl->find(voice);
  if (!slot) {
    return;
  }
  if (fadeOutSeconds > 0.0f && !slot->bPaused && !slot->bPausedByAll) {
    ma_sound_stop_with_fade_in_milliseconds(&slot->sound,
                                            static_cast<ma_uint64>(fadeOutSeconds * 1000.0f));
    slot->bStopRequested = true;
    return;
  }
  m_impl->retire(*slot);
}

/*
 */
void
AudioManager::stopAll()
{
  Impl& impl = *m_impl;
  for (const uint32 index : impl.active) {
    AudioVoiceSlot& voice = impl.voices[index];
    if (voice.state == AudioVoiceSlot::State::Playing) {
      impl.retire(voice);
    }
  }
}

/*
 */
bool
AudioManager::isPlaying(AudioVoice voice) const
{
  return m_impl->find(voice) != nullptr;
}

/*
 */
void
AudioManager::setPaused(AudioVoice voice, bool bPaused)
{
  if (AudioVoiceSlot* slot = m_impl->find(voice)) {
    slot->bPaused = bPaused;
    m_impl->applyVoicePause(*slot);
  }
}

/*
 */
void
AudioManager::setVolume(AudioVoice voice, float volume)
{
  if (AudioVoiceSlot* slot = m_impl->find(voice)) {
    ma_sound_set_volume(&slot->sound, clampVolume(volume * slot->clip->getSettings().volume));
  }
}

/*
 */
void
AudioManager::setPitch(AudioVoice voice, float pitch)
{
  if (AudioVoiceSlot* slot = m_impl->find(voice)) {
    ma_sound_set_pitch(&slot->sound, Math::max(pitch, 0.01f));
  }
}

/*
 */
void
AudioManager::setPan(AudioVoice voice, float pan)
{
  if (AudioVoiceSlot* slot = m_impl->find(voice)) {
    ma_sound_set_pan(&slot->sound, Math::clamp(pan, -1.0f, 1.0f));
  }
}

/*
 */
void
AudioManager::setLooping(AudioVoice voice, bool bLoop)
{
  if (AudioVoiceSlot* slot = m_impl->find(voice)) {
    ma_sound_set_looping(&slot->sound, bLoop ? MA_TRUE : MA_FALSE);
  }
}

/*
 */
void
AudioManager::setPosition(AudioVoice voice, const Vector3& position, const Vector3& velocity)
{
  if (AudioVoiceSlot* slot = m_impl->find(voice)) {
    m_impl->setPosition(*slot, position, velocity);
  }
}

/*
 */
void
AudioManager::setDirection(AudioVoice voice, const Vector3& direction)
{
  if (AudioVoiceSlot* slot = m_impl->find(voice)) {
    m_impl->setDirection(*slot, direction);
  }
}

/*
 */
void
AudioManager::setSpatialSettings(AudioVoice voice, const AudioSpatialSettings& settings)
{
  if (AudioVoiceSlot* slot = m_impl->find(voice)) {
    m_impl->applySpatialSettings(*slot, settings);
  }
}

/*
 */
float
AudioManager::getTime(AudioVoice voice) const
{
  const AudioVoiceSlot* slot = m_impl->find(voice);
  if (!slot) {
    return 0.0f;
  }
  float seconds = 0.0f;
  if (ma_sound_get_cursor_in_seconds(&slot->sound, &seconds) != MA_SUCCESS) {
    return 0.0f;
  }
  return seconds;
}

/*
 */
void
AudioManager::setTime(AudioVoice voice, float seconds)
{
  if (AudioVoiceSlot* slot = m_impl->find(voice)) {
    ma_sound_seek_to_second(&slot->sound, Math::max(seconds, 0.0f));
  }
}

/*
 */
void
AudioManager::setAllPaused(bool bPaused)
{
  Impl& impl = *m_impl;
  if (impl.bAllPaused == bPaused) {
    return;
  }
  impl.bAllPaused = bPaused;
  // Only the voices playing now; sounds started during the pause (menus) play.
  for (const uint32 index : impl.active) {
    AudioVoiceSlot& voice = impl.voices[index];
    if (voice.state == AudioVoiceSlot::State::Playing && voice.bPausedByAll != bPaused) {
      voice.bPausedByAll = bPaused;
      impl.applyVoicePause(voice);
    }
  }
}

/*
 */
bool
AudioManager::isAllPaused() const noexcept
{
  return m_impl->bAllPaused;
}

/*
 */
void
AudioManager::setListener(const Vector3& position,
                          const Vector3& forward,
                          const Vector3& up,
                          const Vector3& velocity)
{
  Impl& impl = *m_impl;
  if (!impl.bEngineReady) {
    return;
  }
  float x, y, z;
  toMixerSpace(position, x, y, z);
  ma_engine_listener_set_position(&impl.engine, 0, x, y, z);
  toMixerSpace(forward, x, y, z);
  ma_engine_listener_set_direction(&impl.engine, 0, x, y, z);
  toMixerSpace(up, x, y, z);
  ma_engine_listener_set_world_up(&impl.engine, 0, x, y, z);
  toMixerSpace(velocity, x, y, z);
  ma_engine_listener_set_velocity(&impl.engine, 0, x, y, z);
}

/*
 */
void
AudioManager::updateListener(const Camera& camera, float deltaTime)
{
  Impl& impl = *m_impl;
  const Vector3 position = camera.getPosition();
  Vector3 velocity = Vector3::ZERO;
  if (impl.bListenerPlaced && deltaTime > 0.0f) {
    const Vector3 moved = position - impl.lastListenerPosition;
    if (moved.sqrMagnitude() < TELEPORT_DISTANCE * TELEPORT_DISTANCE) {
      velocity = moved / deltaTime;
    }
  }
  impl.lastListenerPosition = position;
  impl.bListenerPlaced = true;
  setListener(position, camera.getForwardVector(), camera.getUpVector(), velocity);
}

/*
 */
void
AudioManager::setBusVolume(AudioBus bus, float volume)
{
  Impl& impl = *m_impl;
  const uint32 index = static_cast<uint32>(bus);
  if (index >= static_cast<uint32>(AudioBus::COUNT)) {
    return;
  }
  volume = clampVolume(volume);
  if (impl.busVolumes[index] == volume) {
    return;
  }
  impl.busVolumes[index] = volume;
  if (impl.bBusesReady) {
    ma_sound_group_set_volume(&impl.buses[index], volume);
  }
}

/*
 */
float
AudioManager::getBusVolume(AudioBus bus) const
{
  const uint32 index = static_cast<uint32>(bus);
  return index < static_cast<uint32>(AudioBus::COUNT) ? m_impl->busVolumes[index] : 0.0f;
}

/*
 */
void
AudioManager::setMasterVolume(float volume)
{
  Impl& impl = *m_impl;
  volume = clampVolume(volume);
  if (impl.masterVolume == volume) {
    return;
  }
  impl.masterVolume = volume;
  if (impl.bEngineReady) {
    ma_engine_set_volume(&impl.engine, volume);
  }
}

/*
 */
float
AudioManager::getMasterVolume() const
{
  return m_impl->masterVolume;
}

/*
 */
bool
AudioManager::hasDevice() const noexcept
{
  return m_impl->bHasDevice;
}

/*
 */
uint32
AudioManager::getActiveVoiceCount() const noexcept
{
  uint32 count = 0;
  for (const uint32 index : m_impl->active) {
    count += m_impl->voices[index].state == AudioVoiceSlot::State::Playing ? 1u : 0u;
  }
  return count;
}

/*
 */
uint32
AudioManager::getMaxVoices() const noexcept
{
  return static_cast<uint32>(m_impl->voices.size());
}

/*
 */
uint32
AudioManager::getChannels() const noexcept
{
  return m_impl->bEngineReady ? ma_engine_get_channels(&m_impl->engine) : 0;
}

/*
 */
uint32
AudioManager::getSampleRate() const noexcept
{
  return m_impl->bEngineReady ? ma_engine_get_sample_rate(&m_impl->engine) : 0;
}

/*
 */
uint64
AudioManager::readMix(Span<float> output, uint64 frameCount)
{
  Impl& impl = *m_impl;
  if (!impl.bEngineReady || impl.bHasDevice) {
    return 0;
  }
  const uint32 channels = getChannels();
  frameCount = Math::min<uint64>(frameCount, output.size() / Math::max(channels, 1u));
  ma_uint64 framesRead = 0;
  if (ma_engine_read_pcm_frames(&impl.engine, output.data(), frameCount, &framesRead) !=
      MA_SUCCESS) {
    return 0;
  }
  return framesRead;
}

} // namespace chEngineSDK
