/************************************************************************/
/**
 * @file chAudioTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of audio clips, the audio manager and AudioSourceComponent.
 */
/************************************************************************/
#include "chAssetMeta.h"
#include "chAudioClipAsset.h"
#include "chAudioManager.h"
#include "chAudioSourceComponent.h"
#include "chComponentRegister.h"
#include "chFileStream.h"
#include "chGraphicsTypes.h"
#include "chMath.h"
#include "chScene.h"
#include "chStringUtils.h"
#include "chUUID.h"
#include "chVector3.h"

#include "catch.hpp"
#include "chAudioTestHelpers.h"

using namespace chEngineSDK;

namespace {

class TestAudioClipAsset : public AudioClipAsset
{
 public:
  using AudioClipAsset::AudioClipAsset;
  using AudioClipAsset::deserialize;
  using AudioClipAsset::serialize;
};

/**
 * Mixes frameCount frames (after skipping skipFrames, so gain smoothing settles) and gives
 * the average size of each channel.
 */
void
mixAverage(AudioManager& audio,
           uint32 skipFrames,
           uint32 frameCount,
           float& outLeft,
           float& outRight)
{
  Vector<float> buffer(static_cast<SIZE_T>(Math::max(skipFrames, frameCount)) * 2);
  if (skipFrames > 0) {
    REQUIRE(audio.readMix(buffer, skipFrames) == skipFrames);
  }
  REQUIRE(audio.readMix(buffer, frameCount) == frameCount);
  double left = 0.0;
  double right = 0.0;
  for (uint32 i = 0; i < frameCount; ++i) {
    left += Math::abs(buffer[i * 2]);
    right += Math::abs(buffer[i * 2 + 1]);
  }
  outLeft = static_cast<float>(left / frameCount);
  outRight = static_cast<float>(right / frameCount);
}

} // namespace

TEST_CASE("chCore - Audio clip", "[Audio]")
{
  const Vector<uint8> wav = makeTestWav(2, 44100, 4410, kTestAudioValue);

  SECTION("Info from the header")
  {
    AudioClipInfo info;
    REQUIRE(AudioClip::readInfo(wav, info));
    REQUIRE(info.channels == 2);
    REQUIRE(info.sampleRate == 44100);
    REQUIRE(info.frameCount == 4410);
    REQUIRE(info.getDuration() == Approx(0.1f));
  }

  SECTION("Decompressed clips hold their samples")
  {
    const SPtr<AudioClip> clip = AudioClip::create(wav, {});
    REQUIRE(clip);
    REQUIRE_FALSE(clip->isStreamed());
    REQUIRE(clip->getSamples().size() == 4410 * 2);
    REQUIRE(clip->getSamples()[100] == Approx(0.5f));
    REQUIRE(clip->getEncodedData().size() == wav.size());
  }

  SECTION("Streamed clips keep only the file")
  {
    const SPtr<AudioClip> clip =
        AudioClip::create(wav, {.loadMode = AudioClipLoadMode::Stream, .bus = AudioBus::Music});
    REQUIRE(clip);
    REQUIRE(clip->isStreamed());
    REQUIRE(clip->getSamples().empty());
    REQUIRE(clip->getInfo().frameCount == 4410);
    REQUIRE(clip->getSettings().bus == AudioBus::Music);
  }

  SECTION("Data that is not a sound is refused")
  {
    AudioClipInfo info;
    const Vector<uint8> garbage(256, 7);
    REQUIRE_FALSE(AudioClip::readInfo(garbage, info));
    REQUIRE_FALSE(AudioClip::create(garbage, {}));
    REQUIRE_FALSE(AudioClip::create({}, {}));
  }

  SECTION("Asset round trip")
  {
    AssetMetadata metadata;
    metadata.uuid = chEngineSDK::UUID::createRandom();
    metadata.assetType = AssetTypeTraits<AudioClipAsset>::getTypeId();
    StringUtils::copyToBuffer(metadata.name, "Hit");
    const AudioClipSettings settings{.loadMode = AudioClipLoadMode::Stream,
                                     .bus = AudioBus::Music,
                                     .volume = 0.25f,
                                     .maxVoices = 3};
    TestAudioClipAsset asset(metadata, wav, settings);
    REQUIRE(asset.getClip());
    const SPtr<MemoryDataStream> stream = chMakeShared<MemoryDataStream>();
    REQUIRE(asset.serialize(stream));
    const SIZE_T size = stream->size();

    stream->seek(0);
    TestAudioClipAsset loaded(metadata);
    REQUIRE(loaded.deserialize(stream));
    REQUIRE(loaded.getClip());
    const AudioClipSettings& read = loaded.getClip()->getSettings();
    REQUIRE(read.loadMode == AudioClipLoadMode::Stream);
    REQUIRE(read.bus == AudioBus::Music);
    REQUIRE(read.volume == 0.25f);
    REQUIRE(read.maxVoices == 3);
    REQUIRE(loaded.getClip()->getInfo().frameCount == 4410);

    // A cut file is refused.
    stream->seek(0);
    Vector<uint8> bytes(size / 2);
    REQUIRE(stream->read(bytes.data(), bytes.size()) == bytes.size());
    const SPtr<MemoryDataStream> half = chMakeShared<MemoryDataStream>();
    REQUIRE(half->write(bytes.data(), bytes.size()) == bytes.size());
    half->seek(0);
    TestAudioClipAsset cut(metadata);
    REQUIRE_FALSE(cut.deserialize(half));
  }
}

TEST_CASE("chCore - Audio manager", "[Audio]")
{
  AudioManager audio(AudioManagerDesc{
      .bNoDevice = true, .maxVoices = 4, .channels = 2, .sampleRate = kTestAudioRate});
  REQUIRE_FALSE(audio.hasDevice());
  REQUIRE(audio.getChannels() == 2);
  REQUIRE(audio.getSampleRate() == kTestAudioRate);
  REQUIRE(audio.getMaxVoices() == 4);

  // A tenth of a second, and a long one that is still playing after every mix below.
  const SPtr<const AudioClip> shortClip = AudioClip::create(
      makeTestWav(1, kTestAudioRate, kTestAudioRate / 10, kTestAudioValue), {});
  const SPtr<const AudioClip> longClip = AudioClip::create(
      makeTestWav(1, kTestAudioRate, kTestAudioRate * 2, kTestAudioValue), {});
  REQUIRE(shortClip);
  REQUIRE(longClip);

  float left = 0.0f;
  float right = 0.0f;

  SECTION("Silence without voices")
  {
    REQUIRE_FALSE(audio.play(nullptr).isValid());
    mixAverage(audio, 0, 512, left, right);
    REQUIRE(left == 0.0f);
    REQUIRE(right == 0.0f);
  }

  SECTION("A voice plays to its end and frees itself")
  {
    const AudioVoice voice = audio.play(shortClip);
    REQUIRE(voice.isValid());
    REQUIRE(audio.isPlaying(voice));
    mixAverage(audio, 256, 1024, left, right);
    REQUIRE(left > 0.2f);
    REQUIRE(right == Approx(left).margin(0.01f));

    mixAverage(audio, 0, kTestAudioRate / 10, left, right);
    audio.update();
    REQUIRE_FALSE(audio.isPlaying(voice));
    REQUIRE(audio.getActiveVoiceCount() == 0);
    mixAverage(audio, 0, 512, left, right);
    REQUIRE(left == 0.0f);
  }

  SECTION("Volumes")
  {
    const AudioVoice voice = audio.play(longClip, {.volume = 0.5f});
    mixAverage(audio, 256, 1024, left, right);
    const float half = left;
    REQUIRE(half > 0.05f);

    audio.setVolume(voice, 1.0f);
    mixAverage(audio, 1024, 1024, left, right);
    REQUIRE(left == Approx(half * 2.0f).epsilon(0.05f));

    audio.setBusVolume(AudioBus::Sfx, 0.0f);
    mixAverage(audio, 1024, 1024, left, right);
    REQUIRE(left < 0.001f);

    audio.setBusVolume(AudioBus::Sfx, 1.0f);
    audio.setMasterVolume(0.0f);
    mixAverage(audio, 1024, 1024, left, right);
    REQUIRE(left < 0.001f);
    REQUIRE(audio.isPlaying(voice));
  }

  SECTION("Music bus is apart from the effects bus")
  {
    const SPtr<const AudioClip> music =
        AudioClip::create(makeTestWav(1, kTestAudioRate, kTestAudioRate * 2, kTestAudioValue),
                          {.loadMode = AudioClipLoadMode::Stream, .bus = AudioBus::Music});
    REQUIRE(music);
    const AudioVoice voice = audio.play(music, {.bLoop = true});
    REQUIRE(voice.isValid());
    audio.setBusVolume(AudioBus::Sfx, 0.0f);
    mixAverage(audio, 256, 1024, left, right);
    REQUIRE(left > 0.2f);
    audio.setBusVolume(AudioBus::Music, 0.0f);
    mixAverage(audio, 1024, 1024, left, right);
    REQUIRE(left < 0.001f);
  }

  SECTION("Pan")
  {
    REQUIRE(audio.play(longClip, {.pan = -1.0f}).isValid());
    mixAverage(audio, 256, 1024, left, right);
    REQUIRE(left > 0.2f);
    REQUIRE(right < left * 0.05f);
  }

  SECTION("Spatial voices follow the engine axes")
  {
    // The listener stands at the origin looking along +X with Z up; +Y is its right.
    audio.setListener(Vector3::ZERO, Vector3::FORWARD, Vector3::UP, Vector3::ZERO);
    const AudioVoice voice =
        audio.play(longClip,
                   {.bSpatial = true,
                    .position = Vector3(0.0f, 5.0f, 0.0f),
                    .spatial = {.minDistance = 1.0f, .maxDistance = 100.0f}});
    mixAverage(audio, 2048, 1024, left, right);
    REQUIRE(right > left * 1.5f);
    const float nearVolume = left + right;

    audio.setPosition(voice, Vector3(0.0f, -5.0f, 0.0f), Vector3::ZERO);
    mixAverage(audio, 2048, 1024, left, right);
    REQUIRE(left > right * 1.5f);

    audio.setPosition(voice, Vector3(50.0f, 0.0f, 0.0f), Vector3::ZERO);
    mixAverage(audio, 2048, 1024, left, right);
    REQUIRE(left + right < nearVolume * 0.25f);
  }

  SECTION("A full pool stops the oldest voice")
  {
    AudioVoice voices[5];
    for (AudioVoice& voice : voices) {
      voice = audio.play(longClip);
      REQUIRE(voice.isValid());
    }
    REQUIRE_FALSE(audio.isPlaying(voices[0]));
    for (uint32 i = 1; i < 5; ++i) {
      REQUIRE(audio.isPlaying(voices[i]));
    }
    REQUIRE(audio.getActiveVoiceCount() == 4);
    // The old handle points at a voice that now plays another sound.
    REQUIRE(voices[4].index == voices[0].index);
    REQUIRE(voices[4].generation != voices[0].generation);
  }

  SECTION("A clip limit stops its own oldest voice")
  {
    const SPtr<const AudioClip> limited = AudioClip::create(
        makeTestWav(1, kTestAudioRate, kTestAudioRate * 2, kTestAudioValue), {.maxVoices = 2});
    const AudioVoice other = audio.play(longClip);
    const AudioVoice first = audio.play(limited);
    const AudioVoice second = audio.play(limited);
    const AudioVoice third = audio.play(limited);
    REQUIRE(audio.isPlaying(other));
    REQUIRE_FALSE(audio.isPlaying(first));
    REQUIRE(audio.isPlaying(second));
    REQUIRE(audio.isPlaying(third));
    REQUIRE(audio.getActiveVoiceCount() == 3);
  }

  SECTION("Stop, pause and pause all")
  {
    const AudioVoice voice = audio.play(longClip);
    audio.setPaused(voice, true);
    mixAverage(audio, 256, 1024, left, right);
    REQUIRE(left == 0.0f);
    audio.update();
    REQUIRE(audio.isPlaying(voice));

    audio.setPaused(voice, false);
    audio.setAllPaused(true);
    mixAverage(audio, 256, 1024, left, right);
    REQUIRE(left == 0.0f);
    // Started during the pause, so it plays.
    const AudioVoice menu = audio.play(shortClip);
    mixAverage(audio, 0, 1024, left, right);
    REQUIRE(left > 0.2f);
    audio.stop(menu);

    audio.setAllPaused(false);
    mixAverage(audio, 256, 1024, left, right);
    REQUIRE(left > 0.2f);

    audio.stopAll();
    REQUIRE_FALSE(audio.isPlaying(voice));
    REQUIRE(audio.getActiveVoiceCount() == 0);
    audio.update();
    mixAverage(audio, 0, 1024, left, right);
    // The mixer may already hold one processed frame of the stopped voice.
    REQUIRE(left < 0.001f);
  }

  SECTION("Looping voices keep playing and wrap their time")
  {
    const AudioVoice voice = audio.play(shortClip, {.bLoop = true});
    mixAverage(audio, 0, kTestAudioRate / 4, left, right);
    audio.update();
    REQUIRE(audio.isPlaying(voice));
    REQUIRE(audio.getTime(voice) < 0.1f);
    audio.setLooping(voice, false);
    mixAverage(audio, 0, kTestAudioRate / 5, left, right);
    audio.update();
    REQUIRE_FALSE(audio.isPlaying(voice));
  }
}

TEST_CASE("chCore - Audio source component", "[Audio]")
{
  AudioManager& audio = getTestAudioManager();
  const SPtr<AudioClipAsset> clip = makeTestClipAsset("Hum", kTestAudioRate * 2);
  REQUIRE(clip->getClip());

  Scene scene("Sounds", chEngineSDK::UUID::createRandom());
  const SPtr<GameObject> speaker = scene.createGameObject("Speaker");
  AudioSourceComponent& source = speaker->addComponent<AudioSourceComponent>(clip);
  REQUIRE(source.getClipId() == clip->getUUID());
  REQUIRE(source.isPlayOnStart());

  float left = 0.0f;
  float right = 0.0f;

  SECTION("Play on start waits for a playing scene")
  {
    scene.update(0.016f);
    REQUIRE_FALSE(source.isPlaying());
    scene.setPlaying(true);
    scene.update(0.016f);
    REQUIRE(source.isPlaying());
    mixAverage(audio, 256, 1024, left, right);
    REQUIRE(left > 0.2f);
    // Only once per registration.
    source.stop();
    scene.update(0.016f);
    REQUIRE_FALSE(source.isPlaying());
  }

  SECTION("Settings reach the voice")
  {
    source.play();
    REQUIRE(source.isPlaying());
    source.setVolume(0.5f);
    mixAverage(audio, 1024, 1024, left, right);
    const float half = left;
    source.setVolume(1.0f);
    mixAverage(audio, 1024, 1024, left, right);
    REQUIRE(left == Approx(half * 2.0f).epsilon(0.05f));
    source.setPaused(true);
    REQUIRE(source.isPaused());
    mixAverage(audio, 256, 1024, left, right);
    REQUIRE(left == 0.0f);
    REQUIRE(source.isPlaying());
  }

  SECTION("A spatial source is heard where its object is")
  {
    source.setSpatial(true);
    speaker->getTransform().setLocalPosition(Vector3(0.0f, 5.0f, 0.0f));
    scene.updateTransforms();
    source.play();
    scene.update(0.016f);
    mixAverage(audio, 2048, 1024, left, right);
    REQUIRE(right > left * 1.5f);

    speaker->getTransform().setLocalPosition(Vector3(0.0f, -5.0f, 0.0f));
    scene.updateTransforms();
    scene.update(0.016f);
    mixAverage(audio, 2048, 1024, left, right);
    REQUIRE(left > right * 1.5f);
  }

  SECTION("Leaving the scene stops the sound")
  {
    source.play();
    REQUIRE(source.isPlaying());
    source.setEnabled(false);
    REQUIRE_FALSE(source.isPlaying());
    source.setEnabled(true);
    source.play();
    REQUIRE(source.isPlaying());
    scene.destroyGameObject(*speaker);
    REQUIRE(audio.getActiveVoiceCount() == 0);
  }

  SECTION("A new clip stops the old one")
  {
    source.play();
    source.setClip(makeTestClipAsset("Other", kTestAudioRate));
    REQUIRE_FALSE(source.isPlaying());
  }

  SECTION("Save and read")
  {
    source.setVolume(0.25f);
    source.setPitch(1.5f);
    source.setPan(-0.5f);
    source.setLooping(true);
    source.setPlayOnStart(false);
    source.setSpatial(true);
    source.setSpatialSettings({.attenuation = AudioAttenuation::Linear,
                               .minDistance = 2.0f,
                               .maxDistance = 30.0f,
                               .dopplerFactor = 0.0f,
                               .coneInnerAngle = 90.0f,
                               .coneOuterAngle = 180.0f,
                               .coneOuterGain = 0.25f});
    MemoryDataStream stream;
    source.serialize(stream);
    stream.seek(0);
    AudioSourceComponent loaded;
    REQUIRE(loaded.deserialize(stream));
    // Without an AssetManager the clip is not found, but its id is kept.
    REQUIRE(loaded.getClipId() == clip->getUUID());
    REQUIRE_FALSE(loaded.getClipAsset());
    REQUIRE(loaded.getVolume() == 0.25f);
    REQUIRE(loaded.getPitch() == 1.5f);
    REQUIRE(loaded.getPan() == -0.5f);
    REQUIRE(loaded.isLooping());
    REQUIRE_FALSE(loaded.isPlayOnStart());
    REQUIRE(loaded.isSpatial());
    const AudioSpatialSettings& spatial = loaded.getSpatialSettings();
    REQUIRE(spatial.attenuation == AudioAttenuation::Linear);
    REQUIRE(spatial.minDistance == 2.0f);
    REQUIRE(spatial.maxDistance == 30.0f);
    REQUIRE(spatial.dopplerFactor == 0.0f);
    REQUIRE(spatial.coneInnerAngle == 90.0f);
    REQUIRE(spatial.coneOuterGain == 0.25f);

    Vector<chEngineSDK::UUID> references;
    loaded.collectReferences(references);
    REQUIRE(references.size() == 1);
    REQUIRE(references[0] == clip->getUUID());

    // A cut file is refused.
    MemoryDataStream cut;
    cut.write(&AudioSourceComponent::SERIALIZATION_VERSION, sizeof(uint32));
    cut.seek(0);
    AudioSourceComponent broken;
    REQUIRE_FALSE(broken.deserialize(cut));
  }

  SECTION("Registered for scenes")
  {
    const UniquePtr<Component> made =
        ComponentRegister::create(TypeTraits<AudioSourceComponent>::getTypeId());
    REQUIRE(made);
  }
}
