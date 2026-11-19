/************************************************************************/
/**
 * @file chScriptAudioTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of audio in scripts.
 */
/************************************************************************/
#include "chAlgorithm.h"
#include "chAudioClipAsset.h"
#include "chAudioManager.h"
#include "chAudioSourceComponent.h"
#include "chFileStream.h"
#include "chScene.h"
#include "chScriptAsset.h"
#include "chScriptBinder.h"
#include "chScriptComponent.h"
#include "chScriptManager.h"
#include "chScriptMathTypes.h"
#include "chScriptObjectTypes.h"
#include "chUUID.h"

#include "catch.hpp"
#include "chAudioTestHelpers.h"
#include "chScriptTestHelpers.h"

using namespace chEngineSDK;

TEST_CASE("chCore - Script audio", "[Scripting]")
{
  AudioManager& audio = getTestAudioManager();
  ScriptManager& scripts = getTestScriptManager();
  Scene scene("Sounds", chEngineSDK::UUID::createRandom());
  struct SceneGuard
  {
    ScriptManager& scripts;
    ~SceneGuard() { scripts.setCurrentScene(nullptr); }
  } guard{scripts};
  scripts.setCurrentScene(&scene);

  const SPtr<AudioClipAsset> clip = makeTestClipAsset("Hum", kTestAudioRate * 2);
  const SPtr<GameObject> speaker = scene.createGameObject("Speaker");
  speaker->addComponent<AudioSourceComponent>(clip).setPlayOnStart(false);
  const SPtr<GameObject> silent = scene.createGameObject("Silent");

  SECTION("Bus and master volumes")
  {
    REQUIRE(scripts.runString(R"lua(
      Audio.setBusVolume("Music", 0.5)
      assert(Audio.getBusVolume("music") == 0.5)
      assert(Audio.getBusVolume("Sfx") == 1)
      Audio.setMasterVolume(0.75)
      assert(Audio.getMasterVolume() == 0.75)
      assert(Audio.getVoiceCount() == 0)
      Audio.setBusVolume("Music", 1)
      Audio.setMasterVolume(1)
    )lua"));
    REQUIRE(audio.getMasterVolume() == 1.0f);
    REQUIRE_FALSE(scripts.runString(R"lua(Audio.setBusVolume("Voices", 1))lua"));
    // Without an AssetManager no clip can be named.
    REQUIRE_FALSE(scripts.runString(R"lua(Audio.play("Hum"))lua"));
  }

  SECTION("The AudioSource of a GameObject")
  {
    REQUIRE(scripts.runString(R"lua(
      local source = Scene.find("Speaker"):getAudioSource()
      assert(source ~= nil)
      assert(source == Scene.find("Speaker"):getAudioSource())
      assert(source:getGameObject() == Scene.find("Speaker"))
      assert(not source:isPlaying())
      source:setVolume(0.5)
      source:setLooping(true)
      source:play()
      assert(source:isPlaying())
      assert(source:getVolume() == 0.5)
      assert(source:isLooping())
      source:pause()
      assert(source:isPaused())
      source:resume()
      assert(getmetatable(source:getClip()) == "AudioClip")
      assert(tostring(source:getClip()) == "AudioClip(missing)")
      assert(Audio.getVoiceCount() == 1)
      source:stop()
      assert(not source:isPlaying())
      assert(Scene.find("Silent"):getAudioSource() == nil)
      local added = Scene.find("Silent"):addAudioSource()
      assert(added == Scene.find("Silent"):getAudioSource())
      assert(added:getClip() == nil)
      added:play()
      assert(not added:isPlaying())
    )lua"));
    const AudioSourceComponent* added = silent->getComponent<AudioSourceComponent>();
    REQUIRE(added);
    REQUIRE_FALSE(added->isPlayOnStart());
  }

  SECTION("AudioClip properties are picked in the editor and go into packages")
  {
    ScriptPropertyType type = ScriptPropertyType::Number;
    REQUIRE(ScriptPropertyUtils::fromTypeName("AudioClip", type));
    REQUIRE(type == ScriptPropertyType::AudioClip);

    ScriptPropertyValue value = ScriptPropertyValue::makeDefault(ScriptPropertyType::AudioClip);
    value.object = chEngineSDK::UUID::createRandom();
    MemoryDataStream stream;
    ScriptPropertyUtils::writeValue(stream, value);
    stream.seek(0);
    ScriptPropertyValue loaded;
    REQUIRE(ScriptPropertyUtils::readValue(stream, loaded));
    REQUIRE(loaded.type == ScriptPropertyType::AudioClip);
    REQUIRE(loaded.object == value.object);

    const SPtr<ScriptAsset> player = createTestScriptAsset("SoundUser", R"lua(
      local SoundUser = {}
      SoundUser.properties = { { "hit", type = "AudioClip" } }
      return SoundUser
    )lua");
    const ScriptClassInfo* soundClass = scripts.getClass(*player);
    REQUIRE(soundClass);
    REQUIRE(soundClass->bValid);
    REQUIRE(soundClass->properties[0].defaultValue.type == ScriptPropertyType::AudioClip);

    ScriptComponent component(player);
    component.setOverride("hit", value);
    Vector<chEngineSDK::UUID> references;
    component.collectReferences(references);
    REQUIRE(Algorithm::contains(references, value.object));
  }
}
