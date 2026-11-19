/************************************************************************/
/**
 * @file chScriptAudioBindings.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Audio clips, sounds played from scripts and the AudioSource of a GameObject.
 */
/************************************************************************/
#include "chScriptBindings.h"

#include "chAssetManager.h"
#include "chAudioClipAsset.h"
#include "chAudioManager.h"
#include "chAudioSourceComponent.h"
#include "chGameObject.h"
#include "chMath.h"
#include "chPath.h"
#include "chScriptManager.h"
#include "chScriptMathTypes.h"
#include "chScriptObjectTypes.h"
#include "chStringUtils.h"

namespace chEngineSDK {
CH_SCRIPT_HANDLE_TYPE(AudioSourceComponent, "AudioSource");

/**
 * The audio source of a GameObject: a handle kept with the object's like the other parts,
 * which finds the component each time it is used.
 */
template<>
struct ScriptTraits<AudioSourceComponent>
{
  NODISCARD static AudioSourceComponent&
  check(lua_State* state, const int32 index)
  {
    GameObject& gameObject = ScriptPartHandle::checkOwner(
        state, index, ScriptHandleType<AudioSourceComponent>::NAME);
    AudioSourceComponent* source = gameObject.getComponent<AudioSourceComponent>();
    if (!source) {
      luaL_error(state, "GameObject '%s' has no AudioSource", gameObject.getName().c_str());
    }
    return *source;
  }
};

namespace {
constexpr const ANSICHAR* kBusNames[] = {"Sfx", "Music"};
static_assert(sizeof(kBusNames) / sizeof(kBusNames[0]) == static_cast<SIZE_T>(AudioBus::COUNT));
constexpr const ANSICHAR* kAttenuationNames[] = {"None", "Inverse", "Linear", "Exponential"};
static_assert(sizeof(kAttenuationNames) / sizeof(kAttenuationNames[0]) ==
              static_cast<SIZE_T>(AudioAttenuation::COUNT));

NODISCARD FORCEINLINE AudioManager*
getAudio() noexcept
{
  return AudioManager::isStarted() ? &AudioManager::instance() : nullptr;
}

/*
 * The clip asset of an AudioClip value, or null when it no longer exists.
 */
SPtr<AudioClipAsset>
findClipAsset(const UUID& id)
{
  if (id.isNull() || !AssetManager::isStarted()) {
    return nullptr;
  }
  const SPtr<IAsset> asset = AssetManager::instance().getAsset(id);
  return asset ? asset->as<AudioClipAsset>() : nullptr;
}

/*
 * The clip at index, loaded: an AudioClip value or a path or name. Raises when there is
 * none. A path or name walks every asset, so scripts keep the value AudioClip.find gives.
 */
SPtr<AudioClipAsset>
checkClipAsset(lua_State* state, const int32 index, const ANSICHAR* function)
{
  if (!AssetManager::isStarted()) {
    luaL_error(state, "%s: there are no assets to search", function);
  }
  AssetManager& assetManager = AssetManager::instance();
  SPtr<AudioClipAsset> clip;
  if (const auto* value = static_cast<const ScriptAudioClip*>(
          luaL_testudata(state, index, ScriptValueType<ScriptAudioClip>::NAME))) {
    clip = findClipAsset(value->clip);
    if (!clip) {
      luaL_error(state, "%s: the audio clip was deleted", function);
    }
  }
  else {
    const StringView text = ScriptTraits<StringView>::check(state, index);
    const SPtr<IAsset> asset =
        assetManager.findAsset(AssetTypeTraits<AudioClipAsset>::getTypeId(), text);
    if (!asset) {
      luaL_error(state, "%s: there is no audio clip %s", function, String(text).c_str());
    }
    clip = asset->as<AudioClipAsset>();
  }
  if (!clip->isLoaded() && !assetManager.syncLoadAsset(clip)) {
    luaL_error(state, "%s: audio clip %s could not be loaded", function, clip->getName());
  }
  return clip;
}

template<SIZE_T Count>
uint32
checkName(lua_State* state,
          const int32 index,
          const ANSICHAR* function,
          const ANSICHAR* option,
          const ANSICHAR* const (&names)[Count])
{
  const StringView text = ScriptTraits<StringView>::check(state, index);
  for (SIZE_T i = 0; i < Count; ++i) {
    if (StringUtils::equalsIgnoreCase(text, names[i])) {
      return static_cast<uint32>(i);
    }
  }
  return static_cast<uint32>(
      luaL_error(state, "%s: '%s' is not a %s", function, String(text).c_str(), option));
}

NODISCARD float
checkFloat(lua_State* state, const int32 index)
{
  return static_cast<float>(luaL_checknumber(state, index));
}

/*
 * The named fields of the options table at index; a misspelt one raises.
 */
void
readPlayOptions(lua_State* state, const int32 index, const ANSICHAR* function,
                AudioPlayDesc& desc)
{
  luaL_checktype(state, index, LUA_TTABLE);
  lua_pushnil(state);
  while (lua_next(state, index) != 0) {
    const int32 value = lua_gettop(state);
    if (lua_type(state, -2) != LUA_TSTRING) {
      luaL_error(state, "%s: options are named fields", function);
    }
    const StringView key = lua_tostring(state, -2);
    if (key == "volume") {
      desc.volume = checkFloat(state, value);
    }
    else if (key == "pitch") {
      desc.pitch = checkFloat(state, value);
    }
    else if (key == "pan") {
      desc.pan = checkFloat(state, value);
    }
    else if (key == "loop") {
      desc.bLoop = lua_toboolean(state, value) != 0;
    }
    else if (key == "fadeIn") {
      desc.fadeIn = checkFloat(state, value);
    }
    else if (key == "spatial") {
      desc.bSpatial = lua_toboolean(state, value) != 0;
    }
    else if (key == "position") {
      desc.position = ScriptTraits<Vector3>::check(state, value);
      desc.bSpatial = true;
    }
    else if (key == "velocity") {
      desc.velocity = ScriptTraits<Vector3>::check(state, value);
    }
    else if (key == "direction") {
      desc.direction = ScriptTraits<Vector3>::check(state, value);
    }
    else if (key == "attenuation") {
      desc.spatial.attenuation = static_cast<AudioAttenuation>(
          checkName(state, value, function, "attenuation", kAttenuationNames));
    }
    else if (key == "minDistance") {
      desc.spatial.minDistance = checkFloat(state, value);
    }
    else if (key == "maxDistance") {
      desc.spatial.maxDistance = checkFloat(state, value);
    }
    else if (key == "rolloff") {
      desc.spatial.rolloff = checkFloat(state, value);
    }
    else if (key == "doppler") {
      desc.spatial.dopplerFactor = checkFloat(state, value);
    }
    else if (key == "coneInner") {
      desc.spatial.coneInnerAngle = checkFloat(state, value);
    }
    else if (key == "coneOuter") {
      desc.spatial.coneOuterAngle = checkFloat(state, value);
    }
    else if (key == "coneOuterGain") {
      desc.spatial.coneOuterGain = checkFloat(state, value);
    }
    else {
      luaL_error(state, "%s: there is no option '%s'", function, String(key).c_str());
    }
    lua_pop(state, 1);
  }
}

/*
 * The volume, or an options table, at index; nothing there keeps the defaults.
 */
void
readVolumeOrOptions(lua_State* state, const int32 index, const ANSICHAR* function,
                    AudioPlayDesc& desc)
{
  if (lua_isnoneornil(state, index)) {
    return;
  }
  if (lua_type(state, index) == LUA_TNUMBER) {
    desc.volume = static_cast<float>(lua_tonumber(state, index));
    return;
  }
  readPlayOptions(state, index, function, desc);
}

int32
pushVoice(lua_State* state, const AudioVoice voice)
{
  if (!voice.isValid()) {
    lua_pushnil(state);
    return 1;
  }
  ScriptTraits<AudioVoice>::push(state, voice);
  return 1;
}

/*
 * Audio.play(clip, volumeOrOptions?, pitch?): an AudioVoice, nil without audio. Numbers make
 * no table, so a sound per bullet costs no garbage.
 */
int32
playAudio(lua_State* state)
{
  constexpr const ANSICHAR* kFunction = "Audio.play";
  const SPtr<AudioClipAsset> clip = checkClipAsset(state, 1, kFunction);
  AudioPlayDesc desc;
  readVolumeOrOptions(state, 2, kFunction, desc);
  if (!lua_isnoneornil(state, 3)) {
    desc.pitch = checkFloat(state, 3);
  }
  AudioManager* audio = getAudio();
  return pushVoice(state, audio ? audio->play(clip->getClip(), desc) : AudioVoice{});
}

/*
 * Audio.playAt(clip, position, volumeOrOptions?): a spatial sound at a world position.
 */
int32
playAudioAt(lua_State* state)
{
  constexpr const ANSICHAR* kFunction = "Audio.playAt";
  const SPtr<AudioClipAsset> clip = checkClipAsset(state, 1, kFunction);
  AudioPlayDesc desc{.bSpatial = true, .position = ScriptTraits<Vector3>::check(state, 2)};
  readVolumeOrOptions(state, 3, kFunction, desc);
  AudioManager* audio = getAudio();
  return pushVoice(state, audio ? audio->play(clip->getClip(), desc) : AudioVoice{});
}

int32
stopAllAudio(lua_State* state)
{
  CH_PARAMETER_UNUSED(state);
  if (AudioManager* audio = getAudio()) {
    audio->stopAll();
  }
  return 0;
}

int32
setBusVolume(lua_State* state)
{
  const AudioBus bus =
      static_cast<AudioBus>(checkName(state, 1, "Audio.setBusVolume", "bus", kBusNames));
  const float volume = checkFloat(state, 2);
  if (AudioManager* audio = getAudio()) {
    audio->setBusVolume(bus, volume);
  }
  return 0;
}

int32
getBusVolume(lua_State* state)
{
  const AudioBus bus =
      static_cast<AudioBus>(checkName(state, 1, "Audio.getBusVolume", "bus", kBusNames));
  const AudioManager* audio = getAudio();
  lua_pushnumber(state, audio ? audio->getBusVolume(bus) : 0.0f);
  return 1;
}

int32
setMasterVolume(lua_State* state)
{
  const float volume = checkFloat(state, 1);
  if (AudioManager* audio = getAudio()) {
    audio->setMasterVolume(volume);
  }
  return 0;
}

int32
getMasterVolume(lua_State* state)
{
  const AudioManager* audio = getAudio();
  lua_pushnumber(state, audio ? audio->getMasterVolume() : 0.0f);
  return 1;
}

int32
getVoiceCount(lua_State* state)
{
  const AudioManager* audio = getAudio();
  lua_pushinteger(state, audio ? audio->getActiveVoiceCount() : 0);
  return 1;
}

/*
 * AudioClip.find(pathOrName): an AudioClip value, nil when there is none. Walks every asset,
 * so call it once (in onStart).
 */
int32
findClip(lua_State* state)
{
  const StringView text = ScriptTraits<StringView>::check(state, 1);
  if (!AssetManager::isStarted()) {
    return luaL_error(state, "AudioClip.find: there are no assets to search");
  }
  const SPtr<IAsset> asset =
      AssetManager::instance().findAsset(AssetTypeTraits<AudioClipAsset>::getTypeId(), text);
  if (!asset) {
    lua_pushnil(state);
    return 1;
  }
  ScriptTraits<ScriptAudioClip>::push(state, ScriptAudioClip{asset->getUUID()});
  return 1;
}

int32
isClipValid(lua_State* state)
{
  const auto& clip = ScriptTraits<ScriptAudioClip>::check(state, 1);
  lua_pushboolean(state, findClipAsset(clip.clip) ? 1 : 0);
  return 1;
}

int32
getClipName(lua_State* state)
{
  const auto& clip = ScriptTraits<ScriptAudioClip>::check(state, 1);
  const SPtr<AudioClipAsset> asset = findClipAsset(clip.clip);
  if (asset) {
    lua_pushstring(state, asset->getName());
  }
  else {
    lua_pushnil(state);
  }
  return 1;
}

int32
getClipPath(lua_State* state)
{
  const auto& clip = ScriptTraits<ScriptAudioClip>::check(state, 1);
  const SPtr<AudioClipAsset> asset = findClipAsset(clip.clip);
  if (!asset) {
    lua_pushnil(state);
    return 1;
  }
  const String path = Path(asset->getAssetPath()).join(Path(asset->getName())).toString();
  lua_pushlstring(state, path.data(), path.size());
  return 1;
}

/*
 * clip:getDuration(): seconds; loads the clip.
 */
int32
getClipDuration(lua_State* state)
{
  const SPtr<AudioClipAsset> clip = checkClipAsset(state, 1, "AudioClip.getDuration");
  lua_pushnumber(state, clip->getClip() ? clip->getClip()->getInfo().getDuration() : 0.0f);
  return 1;
}

int32
clipToString(lua_State* state)
{
  const auto& clip = ScriptTraits<ScriptAudioClip>::check(state, 1);
  const SPtr<AudioClipAsset> asset = findClipAsset(clip.clip);
  lua_pushfstring(state, "AudioClip(%s)", asset ? asset->getName() : "missing");
  return 1;
}

int32
isVoicePlaying(lua_State* state)
{
  const AudioVoice voice = ScriptTraits<AudioVoice>::check(state, 1);
  const AudioManager* audio = getAudio();
  lua_pushboolean(state, audio && audio->isPlaying(voice) ? 1 : 0);
  return 1;
}

int32
stopVoice(lua_State* state)
{
  const AudioVoice voice = ScriptTraits<AudioVoice>::check(state, 1);
  const float fade = static_cast<float>(luaL_optnumber(state, 2, 0.0));
  if (AudioManager* audio = getAudio()) {
    audio->stop(voice, fade);
  }
  return 0;
}

int32
setVoicePaused(lua_State* state)
{
  const AudioVoice voice = ScriptTraits<AudioVoice>::check(state, 1);
  const bool bPaused = lua_toboolean(state, 2) != 0;
  if (AudioManager* audio = getAudio()) {
    audio->setPaused(voice, bPaused);
  }
  return 0;
}

int32
setVoiceVolume(lua_State* state)
{
  const AudioVoice voice = ScriptTraits<AudioVoice>::check(state, 1);
  const float volume = checkFloat(state, 2);
  if (AudioManager* audio = getAudio()) {
    audio->setVolume(voice, volume);
  }
  return 0;
}

int32
setVoicePitch(lua_State* state)
{
  const AudioVoice voice = ScriptTraits<AudioVoice>::check(state, 1);
  const float pitch = checkFloat(state, 2);
  if (AudioManager* audio = getAudio()) {
    audio->setPitch(voice, pitch);
  }
  return 0;
}

int32
setVoicePan(lua_State* state)
{
  const AudioVoice voice = ScriptTraits<AudioVoice>::check(state, 1);
  const float pan = checkFloat(state, 2);
  if (AudioManager* audio = getAudio()) {
    audio->setPan(voice, pan);
  }
  return 0;
}

int32
setVoiceLooping(lua_State* state)
{
  const AudioVoice voice = ScriptTraits<AudioVoice>::check(state, 1);
  const bool bLoop = lua_toboolean(state, 2) != 0;
  if (AudioManager* audio = getAudio()) {
    audio->setLooping(voice, bLoop);
  }
  return 0;
}

int32
setVoicePosition(lua_State* state)
{
  const AudioVoice voice = ScriptTraits<AudioVoice>::check(state, 1);
  const Vector3 position = ScriptTraits<Vector3>::check(state, 2);
  const Vector3 velocity =
      lua_isnoneornil(state, 3) ? Vector3::ZERO : ScriptTraits<Vector3>::check(state, 3);
  if (AudioManager* audio = getAudio()) {
    audio->setPosition(voice, position, velocity);
  }
  return 0;
}

int32
getVoiceTime(lua_State* state)
{
  const AudioVoice voice = ScriptTraits<AudioVoice>::check(state, 1);
  const AudioManager* audio = getAudio();
  lua_pushnumber(state, audio ? audio->getTime(voice) : 0.0f);
  return 1;
}

int32
setVoiceTime(lua_State* state)
{
  const AudioVoice voice = ScriptTraits<AudioVoice>::check(state, 1);
  const float seconds = checkFloat(state, 2);
  if (AudioManager* audio = getAudio()) {
    audio->setTime(voice, seconds);
  }
  return 0;
}

int32
getSourceGameObject(lua_State* state)
{
  ScriptTraits<GameObject>::push(
      state,
      ScriptPartHandle::checkOwner(state, 1, ScriptHandleType<AudioSourceComponent>::NAME));
  return 1;
}

int32
playSource(lua_State* state)
{
  ScriptTraits<AudioSourceComponent>::check(state, 1).play();
  return 0;
}

int32
stopSource(lua_State* state)
{
  AudioSourceComponent& source = ScriptTraits<AudioSourceComponent>::check(state, 1);
  source.stop(static_cast<float>(luaL_optnumber(state, 2, 0.0)));
  return 0;
}

int32
pauseSource(lua_State* state)
{
  ScriptTraits<AudioSourceComponent>::check(state, 1).setPaused(true);
  return 0;
}

int32
resumeSource(lua_State* state)
{
  ScriptTraits<AudioSourceComponent>::check(state, 1).setPaused(false);
  return 0;
}

int32
isSourcePlaying(lua_State* state)
{
  lua_pushboolean(state, ScriptTraits<AudioSourceComponent>::check(state, 1).isPlaying() ? 1 : 0);
  return 1;
}

int32
isSourcePaused(lua_State* state)
{
  lua_pushboolean(state, ScriptTraits<AudioSourceComponent>::check(state, 1).isPaused() ? 1 : 0);
  return 1;
}

/*
 * source:setClip(clip?): nil leaves it without a clip.
 */
int32
setSourceClip(lua_State* state)
{
  AudioSourceComponent& source = ScriptTraits<AudioSourceComponent>::check(state, 1);
  if (lua_isnoneornil(state, 2)) {
    source.setClip(nullptr);
    return 0;
  }
  source.setClip(checkClipAsset(state, 2, "AudioSource.setClip"));
  return 0;
}

int32
getSourceClip(lua_State* state)
{
  const AudioSourceComponent& source = ScriptTraits<AudioSourceComponent>::check(state, 1);
  if (source.getClipId().isNull()) {
    lua_pushnil(state);
    return 1;
  }
  ScriptTraits<ScriptAudioClip>::push(state, ScriptAudioClip{source.getClipId()});
  return 1;
}

int32
setSourceVolume(lua_State* state)
{
  ScriptTraits<AudioSourceComponent>::check(state, 1).setVolume(checkFloat(state, 2));
  return 0;
}

int32
getSourceVolume(lua_State* state)
{
  lua_pushnumber(state, ScriptTraits<AudioSourceComponent>::check(state, 1).getVolume());
  return 1;
}

int32
setSourcePitch(lua_State* state)
{
  ScriptTraits<AudioSourceComponent>::check(state, 1).setPitch(checkFloat(state, 2));
  return 0;
}

int32
getSourcePitch(lua_State* state)
{
  lua_pushnumber(state, ScriptTraits<AudioSourceComponent>::check(state, 1).getPitch());
  return 1;
}

int32
setSourceLooping(lua_State* state)
{
  ScriptTraits<AudioSourceComponent>::check(state, 1).setLooping(lua_toboolean(state, 2) != 0);
  return 0;
}

int32
isSourceLooping(lua_State* state)
{
  lua_pushboolean(state, ScriptTraits<AudioSourceComponent>::check(state, 1).isLooping() ? 1 : 0);
  return 1;
}

int32
getSourceTime(lua_State* state)
{
  lua_pushnumber(state, ScriptTraits<AudioSourceComponent>::check(state, 1).getTime());
  return 1;
}

int32
setSourceTime(lua_State* state)
{
  ScriptTraits<AudioSourceComponent>::check(state, 1).setTime(checkFloat(state, 2));
  return 0;
}

/*
 * gameObject:getAudioSource(): the object's AudioSource, nil without one.
 */
int32
getObjectAudioSource(lua_State* state)
{
  GameObject& gameObject = ScriptTraits<GameObject>::check(state, 1);
  if (!gameObject.getComponent<AudioSourceComponent>()) {
    lua_pushnil(state);
    return 1;
  }
  ScriptPartHandle::push(state, gameObject, ScriptHandleType<AudioSourceComponent>::NAME);
  return 1;
}

/*
 * gameObject:addAudioSource(clip?): adds an AudioSource that does not play on start (a
 * script decides when) and gives it; an object that has one already gets that one, with
 * the clip set when given.
 */
int32
addObjectAudioSource(lua_State* state)
{
  GameObject& gameObject = ScriptTraits<GameObject>::check(state, 1);
  AudioSourceComponent* source = gameObject.getComponent<AudioSourceComponent>();
  if (!source) {
    source = &gameObject.addComponent<AudioSourceComponent>();
    source->setPlayOnStart(false);
  }
  if (!lua_isnoneornil(state, 2)) {
    source->setClip(checkClipAsset(state, 2, "GameObject.addAudioSource"));
  }
  ScriptPartHandle::push(state, gameObject, ScriptHandleType<AudioSourceComponent>::NAME);
  return 1;
}
} // namespace

/*
 */
void
ScriptBindings::registerAudio(lua_State* state)
{
  ScriptClass<ScriptAudioClip>(state)
      .rawStaticFunction("find", &findClip, "fun(pathOrName: string): AudioClip?")
      .rawMethod("isValid", &isClipValid, "fun(): boolean")
      .rawMethod("getName", &getClipName, "fun(): string?")
      .rawMethod("getPath", &getClipPath, "fun(): string?")
      .rawMethod("getDuration", &getClipDuration, "fun(): number")
      .operators<ScriptOperator::Equal>();
  luaL_getmetatable(state, ScriptValueType<ScriptAudioClip>::NAME);
  lua_pushcfunction(state, &clipToString);
  lua_setfield(state, -2, "__tostring");
  lua_pop(state, 1);

  ScriptClass<AudioVoice>(state)
      .rawMethod("isPlaying", &isVoicePlaying, "fun(): boolean")
      .rawMethod("stop", &stopVoice, "fun(fadeOut?: number)")
      .rawMethod("setPaused", &setVoicePaused, "fun(paused: boolean)")
      .rawMethod("setVolume", &setVoiceVolume, "fun(volume: number)")
      .rawMethod("setPitch", &setVoicePitch, "fun(pitch: number)")
      .rawMethod("setPan", &setVoicePan, "fun(pan: number)")
      .rawMethod("setLooping", &setVoiceLooping, "fun(loop: boolean)")
      .rawMethod("setPosition", &setVoicePosition,
                 "fun(position: Vector3, velocity?: Vector3)")
      .rawMethod("getTime", &getVoiceTime, "fun(): number")
      .rawMethod("setTime", &setVoiceTime, "fun(seconds: number)");

  ScriptClass<AudioSourceComponent>(state)
      .rawMethod("getGameObject", &getSourceGameObject, "fun(): GameObject")
      .rawMethod("play", &playSource, "fun()")
      .rawMethod("stop", &stopSource, "fun(fadeOut?: number)")
      .rawMethod("pause", &pauseSource, "fun()")
      .rawMethod("resume", &resumeSource, "fun()")
      .rawMethod("isPlaying", &isSourcePlaying, "fun(): boolean")
      .rawMethod("isPaused", &isSourcePaused, "fun(): boolean")
      .rawMethod("setClip", &setSourceClip, "fun(clip?: AudioClip|string)")
      .rawMethod("getClip", &getSourceClip, "fun(): AudioClip?")
      .rawMethod("setVolume", &setSourceVolume, "fun(volume: number)")
      .rawMethod("getVolume", &getSourceVolume, "fun(): number")
      .rawMethod("setPitch", &setSourcePitch, "fun(pitch: number)")
      .rawMethod("getPitch", &getSourcePitch, "fun(): number")
      .rawMethod("setLooping", &setSourceLooping, "fun(loop: boolean)")
      .rawMethod("isLooping", &isSourceLooping, "fun(): boolean")
      .rawMethod("getTime", &getSourceTime, "fun(): number")
      .rawMethod("setTime", &setSourceTime, "fun(seconds: number)");

  ScriptClass<GameObject>(state)
      .rawMethod("getAudioSource", &getObjectAudioSource, "fun(): AudioSource?")
      .rawMethod("addAudioSource", &addObjectAudioSource,
                 "fun(clip?: AudioClip|string): AudioSource");

  if (ScriptApiDescription* api = ScriptApiDescription::find(state)) {
    api->addAlias("AudioBus", "\"Sfx\"|\"Music\"");
    ScriptApiType& options = api->addType("AudioPlayOptions", ScriptApiTypeKind::Shape);
    options.description = "How Audio.play and Audio.playAt play a sound; every field is "
                          "optional. A position makes the sound spatial.";
    options.fields = {{"volume?", "number"},
                      {"pitch?", "number"},
                      {"pan?", "number"},
                      {"loop?", "boolean"},
                      {"fadeIn?", "number"},
                      {"spatial?", "boolean"},
                      {"position?", "Vector3"},
                      {"velocity?", "Vector3"},
                      {"direction?", "Vector3"},
                      {"attenuation?", "\"None\"|\"Inverse\"|\"Linear\"|\"Exponential\""},
                      {"minDistance?", "number"},
                      {"maxDistance?", "number"},
                      {"rolloff?", "number"},
                      {"doppler?", "number"},
                      {"coneInner?", "number"},
                      {"coneOuter?", "number"},
                      {"coneOuterGain?", "number"}};
  }

  ScriptTable(state, "Audio",
              "Sounds played without a GameObject, and the volumes of the buses. Spatial "
              "sounds are heard from the camera of the view.")
      .rawFunction("play", &playAudio,
                   "fun(clip: AudioClip|string, volumeOrOptions?: number|AudioPlayOptions, "
                   "pitch?: number): AudioVoice?")
      .rawFunction("playAt", &playAudioAt,
                   "fun(clip: AudioClip|string, position: Vector3, volumeOrOptions?: "
                   "number|AudioPlayOptions): AudioVoice?")
      .rawFunction("stopAll", &stopAllAudio, "fun()")
      .rawFunction("setBusVolume", &setBusVolume, "fun(bus: AudioBus, volume: number)")
      .rawFunction("getBusVolume", &getBusVolume, "fun(bus: AudioBus): number")
      .rawFunction("setMasterVolume", &setMasterVolume, "fun(volume: number)")
      .rawFunction("getMasterVolume", &getMasterVolume, "fun(): number")
      .rawFunction("getVoiceCount", &getVoiceCount, "fun(): integer");
}

} // namespace chEngineSDK
