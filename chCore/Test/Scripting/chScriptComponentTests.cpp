/************************************************************************/
/**
 * @file chScriptComponentTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of script assets, ScriptComponent, hot reload and properties.
 */
/************************************************************************/
#include "chAssetMeta.h"
#include "chComponentRegister.h"
#include "chFileStream.h"
#include "chGameObjectSerializer.h"
#include "chGraphicsTypes.h"
#include "chPrefabAsset.h"
#include "chScene.h"
#include "chScriptAsset.h"
#include "chScriptBinder.h"
#include "chScriptComponent.h"
#include "chScriptManager.h"
#include "chScriptMathTypes.h"
#include "chScriptObjectTypes.h"
#include "chStringUtils.h"
#include "chUUID.h"
#include "chVector3.h"

#include "catch.hpp"
#include "chScriptTestHelpers.h"

using namespace chEngineSDK;

TEST_CASE("chCore - Script UUID line", "[Scripting]")
{
  const chEngineSDK::UUID uuid = chEngineSDK::UUID::createRandom();
  chEngineSDK::UUID read = chEngineSDK::UUID::null();
  REQUIRE_FALSE(ScriptAsset::readUUIDLine("local A = {}\n", read));
  REQUIRE(read.isNull());

  const String added = ScriptAsset::withUUIDLine("local A = {}\nreturn A\n", uuid);
  REQUIRE(added == "-- chimera-uuid: " + uuid.toString() + "\nlocal A = {}\nreturn A\n");
  REQUIRE(ScriptAsset::readUUIDLine(added, read));
  REQUIRE(read == uuid);

  // An old UUID line is replaced, and the file keeps its line endings.
  const chEngineSDK::UUID other = chEngineSDK::UUID::createRandom();
  const String crlf = ScriptAsset::withUUIDLine("local A = {}\r\nreturn A\r\n", uuid);
  const String replaced = ScriptAsset::withUUIDLine(crlf, other);
  REQUIRE(replaced == "-- chimera-uuid: " + other.toString() + "\r\nlocal A = {}\r\nreturn A\r\n");
  REQUIRE(ScriptAsset::readUUIDLine(replaced, read));
  REQUIRE(read == other);

  REQUIRE(ScriptAsset::withUUIDLine("", uuid) == "-- chimera-uuid: " + uuid.toString() + "\n");
  REQUIRE_FALSE(ScriptAsset::readUUIDLine("-- chimera-uuid: not-a-uuid\n", read));
}

TEST_CASE("chCore - Script component", "[Scripting]")
{
  ScriptManager& scripts = getTestScriptManager();
  REQUIRE(scripts.runString("Events = { selves = {}, starts = 0, destroys = 0 }"));
  const SPtr<ScriptAsset> mover = createTestScriptAsset("Mover", R"lua(
    local Mover = { speed = 2 }
    function Mover:onStart()
      self.distance = 0
      Events.starts = Events.starts + 1
      Events.selves[self] = true
    end
    function Mover:update(dt)
      self.distance = self.distance + self.speed * dt
      Events.distance = self.distance
      Events.updates = (Events.updates or 0) + 1
    end
    function Mover:fixedUpdate(dt)
      Events.fixed = (Events.fixed or 0) + 1
    end
    function Mover:onDestroy()
      Events.destroys = Events.destroys + 1
    end
    return Mover
  )lua");

  Scene scene("Scripts", chEngineSDK::UUID::createRandom());
  const SPtr<GameObject> runner = scene.createGameObject("Runner");
  ScriptComponent& component = runner->addComponent<ScriptComponent>(mover);
  REQUIRE(component.getScriptId() == mover->getUUID());

  SECTION("Nothing runs while the scene does not play")
  {
    scene.update(0.5f);
    scene.fixedUpdate(0.5f);
    REQUIRE_FALSE(component.isRunning());
    REQUIRE(scripts.runString("assert(Events.starts == 0 and Events.updates == nil)"));
  }

  SECTION("onStart, update, fixedUpdate, onDestroy")
  {
    scene.setPlaying(true);
    scene.update(0.5f);
    scene.update(0.25f);
    scene.fixedUpdate(0.1f);
    REQUIRE(component.isRunning());
    REQUIRE(scripts.runString(R"lua(
      assert(Events.starts == 1 and Events.updates == 2 and Events.fixed == 1)
      assert(math.abs(Events.distance - 1.5) < 1e-6)
    )lua"));

    component.setEnabled(false);
    REQUIRE_FALSE(component.isRunning());
    REQUIRE(scripts.runString("assert(Events.destroys == 1)"));

    // Registered again it starts over with a new instance.
    component.setEnabled(true);
    scene.update(0.1f);
    REQUIRE(scripts.runString("assert(Events.starts == 2)"));
  }

  SECTION("Each component has its own self")
  {
    const SPtr<GameObject> other = scene.createGameObject("Other");
    other->addComponent<ScriptComponent>(mover);
    scene.setPlaying(true);
    scene.update(0.5f);
    REQUIRE(scripts.runString(R"lua(
      local count = 0
      for self in pairs(Events.selves) do
        count = count + 1
        assert(self.distance == 1 and self.speed == 2)
      end
      assert(count == 2)
    )lua"));
  }

  SECTION("Destroying the object runs onDestroy")
  {
    scene.setPlaying(true);
    scene.update(0.1f);
    scene.destroyGameObject(*runner);
    REQUIRE(scripts.runString("assert(Events.destroys == 1)"));
  }

  SECTION("An error stops only its component")
  {
    const SPtr<ScriptAsset> broken = createTestScriptAsset("Broken", R"lua(
      local Broken = {}
      function Broken:update(dt)
        error("broken on purpose")
      end
      return Broken
    )lua");
    const SPtr<GameObject> other = scene.createGameObject("Other");
    ScriptComponent& brokenComponent = other->addComponent<ScriptComponent>(broken);
    scene.setPlaying(true);
    scene.update(0.1f);
    scene.update(0.1f);
    REQUIRE(brokenComponent.hasFailed());
    REQUIRE_FALSE(brokenComponent.isRunning());
    REQUIRE(component.isRunning());
    REQUIRE(scripts.runString("assert(Events.updates == 2)"));
  }

  SECTION("A script that returns no class never runs")
  {
    const SPtr<ScriptAsset> noClass = createTestScriptAsset("NoClass", "return 5");
    component.setScript(noClass);
    scene.setPlaying(true);
    scene.update(0.1f);
    REQUIRE(component.hasFailed());
  }

  SECTION("Saved with its script, kept when the asset is missing")
  {
    MemoryDataStream stream;
    component.serialize(stream);
    stream.seek(0);
    ScriptComponent read;
    REQUIRE(read.deserialize(stream));
    REQUIRE(read.getScriptId() == mover->getUUID());
    REQUIRE(read.getScript() == nullptr);

    Vector<chEngineSDK::UUID> references;
    read.collectReferences(references);
    REQUIRE(references.size() == 1);
    REQUIRE(references[0] == mover->getUUID());
    REQUIRE(ComponentRegister::create(TypeTraits<ScriptComponent>::getTypeId()) != nullptr);
  }
}

#if USING(CH_EDITOR)
TEST_CASE("chCore - Script hot reload", "[Scripting]")
{
  ScriptManager& scripts = getTestScriptManager();
  REQUIRE(scripts.runString("Reload = { starts = 0, destroys = 0 }"));
  const SPtr<ScriptAsset> counter = createTestScriptAsset("Counter", R"lua(
    local Counter = { step = 1 }
    function Counter:onStart()
      self.count = 0
      Reload.starts = Reload.starts + 1
    end
    function Counter:update(dt)
      self.count = self.count + self.step
      Reload.count = self.count
    end
    function Counter:onDestroy()
      Reload.destroys = Reload.destroys + 1
    end
    return Counter
  )lua");

  Scene scene("Reload", chEngineSDK::UUID::createRandom());
  scene.setPlaying(true);
  const SPtr<GameObject> object = scene.createGameObject("Object");
  ScriptComponent& component = object->addComponent<ScriptComponent>(counter);
  scene.update(0.1f);
  scene.update(0.1f);
  REQUIRE(scripts.runString("assert(Reload.count == 2)"));

  SECTION("A running instance keeps self and calls the new functions")
  {
    counter->setSource(R"lua(
      local Counter = { step = 10 }
      function Counter:update(dt)
        self.count = self.count + self.step
        Reload.count = self.count
      end
      return Counter
    )lua");
    REQUIRE(scripts.updateClass(*counter));
    scene.update(0.1f);
    REQUIRE(component.isRunning());
    // The count it had goes on, with the step of the new class; onStart did not run again.
    REQUIRE(scripts.runString("assert(Reload.count == 12 and Reload.starts == 1)"));
  }

  SECTION("New text that fails keeps the previous functions")
  {
    counter->setSource("local Counter = {");
    REQUIRE_FALSE(scripts.updateClass(*counter));
    counter->setSource("return 5");
    REQUIRE_FALSE(scripts.updateClass(*counter));
    scene.update(0.1f);
    REQUIRE(component.isRunning());
    REQUIRE(scripts.runString("assert(Reload.count == 3)"));
  }

  SECTION("A component stopped by an error tries the new text")
  {
    counter->setSource(R"lua(
      local Counter = {}
      function Counter:update(dt) error("broken on purpose") end
      return Counter
    )lua");
    REQUIRE(scripts.updateClass(*counter));
    scene.update(0.1f);
    REQUIRE(component.hasFailed());
    scene.update(0.1f);
    REQUIRE(component.hasFailed());

    counter->setSource(R"lua(
      local Counter = {}
      function Counter:onStart() Reload.starts = Reload.starts + 1 end
      function Counter:update(dt) Reload.fixed = true end
      return Counter
    )lua");
    REQUIRE(scripts.updateClass(*counter));
    scene.update(0.1f);
    REQUIRE(component.isRunning());
    REQUIRE(scripts.runString("assert(Reload.fixed and Reload.starts == 2)"));
  }

  SECTION("Restart runs onDestroy and a new onStart")
  {
    REQUIRE(component.restart());
    REQUIRE(scripts.runString("assert(Reload.destroys == 1 and Reload.starts == 2)"));
    scene.update(0.1f);
    REQUIRE(scripts.runString("assert(Reload.count == 1)"));
  }

  scene.destroyGameObject(*object);
  scripts.forgetClass(counter->getUUID());
}
#endif

TEST_CASE("chCore - Script properties", "[Scripting]")
{
  ScriptManager& scripts = getTestScriptManager();
  REQUIRE(scripts.runString("Props = { starts = 0 }"));
  const SPtr<ScriptAsset> tuner = createTestScriptAsset("Tuner", R"lua(
    local Tuner = {}
    Tuner.properties = {
      { "speed", 5 },
      { "count", 3, type = "integer", min = 0, max = 10, tooltip = "How many" },
      { "label", "hello" },
      { "enabled", true },
      { "offset", Vector3(1, 2, 3) },
      { "tint", type = "Color" },
      { "target", type = "GameObject" },
    }
    function Tuner:onStart()
      Props.starts = Props.starts + 1
      Props[self.gameObject:getName()] = {
        speed = self.speed, count = self.count, label = self.label, enabled = self.enabled,
        offsetX = self.offset.x, tintR = self.tint.r, target = self.target,
        countIsInteger = math.type(self.count) == "integer",
      }
      -- In place: must reach neither the class nor other instances.
      self.offset.x = 100
    end
    return Tuner
  )lua");

  const auto makeNumber = [](const double number) {
    ScriptPropertyValue value = ScriptPropertyValue::makeDefault(ScriptPropertyType::Number);
    value.number = number;
    return value;
  };
  const auto makeObject = [](const chEngineSDK::UUID& id) {
    ScriptPropertyValue value =
        ScriptPropertyValue::makeDefault(ScriptPropertyType::GameObject);
    value.object = id;
    return value;
  };

  SECTION("The list is read in order with its types and defaults")
  {
    const ScriptClassInfo* scriptClass = scripts.getClass(*tuner);
    REQUIRE(scriptClass);
    REQUIRE(scriptClass->bValid);
    const Vector<ScriptProperty>& properties = scriptClass->properties;
    REQUIRE(properties.size() == 7);
    REQUIRE(properties[0].name == "speed");
    REQUIRE(properties[0].defaultValue.type == ScriptPropertyType::Number);
    REQUIRE(properties[0].defaultValue.number == 5.0);
    REQUIRE(properties[1].defaultValue.type == ScriptPropertyType::Integer);
    REQUIRE(properties[1].defaultValue.integer == 3);
    REQUIRE(properties[2].defaultValue.text == "hello");
    REQUIRE(properties[3].defaultValue.boolean);
    REQUIRE(properties[4].defaultValue.type == ScriptPropertyType::Vector3);
    REQUIRE(properties[4].defaultValue.components[2] == 3.0f);
    // A color without a default is white.
    REQUIRE(properties[5].defaultValue.type == ScriptPropertyType::Color);
    REQUIRE(properties[5].defaultValue.components[0] == 1.0f);
    REQUIRE(properties[6].defaultValue.type == ScriptPropertyType::GameObject);
    REQUIRE(properties[6].defaultValue.object.isNull());
#if USING(CH_EDITOR)
    REQUIRE(properties[1].minimum == 0.0);
    REQUIRE(properties[1].maximum == 10.0);
    REQUIRE(properties[1].tooltip == "How many");
#endif
  }

  SECTION("A list written wrong stops the script")
  {
    constexpr const ANSICHAR* kBroken[] = {
        "return { properties = 5 }",
        "return { properties = { 5 } }",
        "return { properties = { { 5, 1 } } }",
        "return { properties = { { \"a\", 1, mni = 0 } } }",
        "return { properties = { { \"a\", 1 }, { \"a\", 2 } } }",
        "return { properties = { { \"update\", 1 } } }",
        "return { properties = { { \"a\" } } }",
        "return { properties = { { \"a\", {} } } }",
        "return { properties = { { \"a\", \"text\", type = \"number\" } } }",
        "return { properties = { { \"a\", 2.5, type = \"integer\" } } }",
        "return { properties = { { \"a\", 1, type = \"Vector9\" } } }",
        "return { properties = { { \"a\", 1, type = \"GameObject\" } } }",
        "return { properties = { { \"a\", 1, type = \"Prefab\" } } }",
        "return { properties = { { \"a\", 1, min = \"low\" } } }",
    };
    for (const ANSICHAR* source : kBroken) {
      const SPtr<ScriptAsset> broken = createTestScriptAsset("Broken", source);
      const ScriptClassInfo* scriptClass = scripts.getClass(*broken);
      INFO(source);
      REQUIRE(scriptClass);
      REQUIRE_FALSE(scriptClass->bValid);
      scripts.forgetClass(broken->getUUID());
    }
  }

  Scene scene("Properties", chEngineSDK::UUID::createRandom());
  scene.setPlaying(true);
  const SPtr<GameObject> first = scene.createGameObject("First");
  const SPtr<GameObject> second = scene.createGameObject("Second");
  const SPtr<GameObject> enemy = scene.createGameObject("Enemy");
  ScriptComponent& firstScript = first->addComponent<ScriptComponent>(tuner);
  ScriptComponent& secondScript = second->addComponent<ScriptComponent>(tuner);

  SECTION("self holds the overrides or the defaults before onStart")
  {
    firstScript.setOverride("speed", makeNumber(9.0));
    firstScript.setOverride("target", makeObject(enemy->getUUID()));
    // Another type than the script lists: ignored.
    ScriptPropertyValue wrongType = ScriptPropertyValue::makeDefault(ScriptPropertyType::String);
    wrongType.text = "fast";
    secondScript.setOverride("count", wrongType);
    scene.update(0.1f);
    REQUIRE(scripts.runString(R"lua(
      assert(Props.starts == 2)
      local first, second = Props.First, Props.Second
      assert(first.speed == 9 and second.speed == 5)
      assert(first.count == 3 and second.count == 3 and first.countIsInteger)
      assert(first.label == "hello" and first.enabled == true)
      assert(first.offsetX == 1 and second.offsetX == 1)
      assert(first.tintR == 1)
      assert(first.target ~= nil and first.target:getName() == "Enemy")
      assert(second.target == nil)
    )lua"));
  }

#if USING(CH_EDITOR)
  SECTION("Changes while running reach self")
  {
    scene.update(0.1f);
    REQUIRE(firstScript.isRunning());
    firstScript.setOverride("speed", makeNumber(42.0));
    ScriptPropertyValue live;
    REQUIRE(scripts.getInstanceProperty(firstScript.getInstance(), "speed",
                                        ScriptPropertyType::Number, live));
    REQUIRE(live.number == 42.0);
    REQUIRE(firstScript.removeOverride("speed"));
    REQUIRE(scripts.getInstanceProperty(firstScript.getInstance(), "speed",
                                        ScriptPropertyType::Number, live));
    REQUIRE(live.number == 5.0);
    REQUIRE_FALSE(firstScript.removeOverride("speed"));
    // The vector the script changed in place is read back as it is now.
    REQUIRE(scripts.getInstanceProperty(firstScript.getInstance(), "offset",
                                        ScriptPropertyType::Vector3, live));
    REQUIRE(live.components[0] == 100.0f);
    REQUIRE_FALSE(scripts.getInstanceProperty(firstScript.getInstance(), "offset",
                                              ScriptPropertyType::Number, live));
  }
#endif

  SECTION("Overrides are saved, also those the script no longer lists")
  {
    firstScript.setOverride("speed", makeNumber(7.5));
    firstScript.setOverride("gone", makeObject(enemy->getUUID()));
    ScriptPropertyValue color = ScriptPropertyValue::makeDefault(ScriptPropertyType::Color);
    color.components[1] = 0.25f;
    firstScript.setOverride("tint", color);

    MemoryDataStream stream;
    firstScript.serialize(stream);
    MemoryDataStream reader(stream.getStartPtr(), stream.size(), false);
    ScriptComponent loaded;
    REQUIRE(loaded.deserialize(reader));
    REQUIRE(loaded.getScriptId() == tuner->getUUID());
    REQUIRE(loaded.getOverrides().size() == 3);
    REQUIRE(*loaded.findOverride("speed") == makeNumber(7.5));
    REQUIRE(*loaded.findOverride("gone") == makeObject(enemy->getUUID()));
    REQUIRE(*loaded.findOverride("tint") == color);

    // Cut anywhere inside the overrides, the data is refused.
    MemoryDataStream cut(stream.getStartPtr(), stream.size() - 3, false);
    ScriptComponent broken;
    REQUIRE_FALSE(broken.deserialize(cut));
  }

  SECTION("Copies point at the copies of the objects copied with them")
  {
    // Holder > Weapon, Holder's script points at Weapon and at Enemy (not copied).
    const SPtr<GameObject> holder = scene.createGameObject("Holder");
    const SPtr<GameObject> weapon = scene.createGameObject("Weapon", holder.get());
    ScriptComponent& holderScript = holder->addComponent<ScriptComponent>(tuner);
    holderScript.setOverride("target", makeObject(weapon->getUUID()));
    holderScript.setOverride("other", makeObject(enemy->getUUID()));

    const SPtr<GameObject> copy = GameObjectSerializer::duplicate(*holder);
    REQUIRE(copy);
    REQUIRE(copy->getChildren().size() == 1);
    const SPtr<GameObject>& weaponCopy = copy->getChildren()[0];
    REQUIRE(weaponCopy->getUUID() != weapon->getUUID());
    const ScriptComponent* copyScript = copy->getComponent<ScriptComponent>();
    REQUIRE(copyScript);
    REQUIRE(copyScript->findOverride("target")->object == weaponCopy->getUUID());
    REQUIRE(copyScript->findOverride("other")->object == enemy->getUUID());
    // The source keeps its own.
    REQUIRE(holderScript.findOverride("target")->object == weapon->getUUID());

    // Instances of a prefab point inside themselves, also at their root.
    AssetMetadata metadata;
    metadata.uuid = chEngineSDK::UUID::createRandom();
    metadata.assetType = AssetTypeTraits<PrefabAsset>::getTypeId();
    StringUtils::copyToBuffer(metadata.name, "PF_Holder");
    PrefabAsset prefab(metadata);
    holderScript.setOverride("self", makeObject(holder->getUUID()));
    REQUIRE(prefab.setContent(*holder));
    const SPtr<GameObject> instance = GameObjectSerializer::instantiate(prefab, scene, nullptr);
    REQUIRE(instance->getChildren().size() == 1);
    const ScriptComponent* instanceScript = instance->getComponent<ScriptComponent>();
    REQUIRE(instanceScript);
    REQUIRE(instanceScript->findOverride("target")->object ==
            instance->getChildren()[0]->getUUID());
    REQUIRE(instanceScript->findOverride("self")->object == instance->getUUID());
    REQUIRE(instanceScript->findOverride("other")->object == enemy->getUUID());
  }

#if USING(CH_EDITOR)
  SECTION("A property added to a running script reaches its instances")
  {
    scene.update(0.1f);
    tuner->setSource(R"lua(
      local Tuner = {}
      Tuner.properties = { { "speed", 5 }, { "jump", 2.5 } }
      function Tuner:update(dt)
        Props[self.gameObject:getName() .. "Jump"] = self.jump
        Props[self.gameObject:getName() .. "Speed"] = self.speed
      end
      return Tuner
    )lua");
    firstScript.setOverride("speed", makeNumber(8.0));
    REQUIRE(scripts.updateClass(*tuner));
    scene.update(0.1f);
    // The new property arrives; the one self already held keeps its value.
    REQUIRE(scripts.runString(R"lua(
      assert(Props.FirstJump == 2.5 and Props.FirstSpeed == 8)
      assert(Props.SecondJump == 2.5 and Props.SecondSpeed == 5)
    )lua"));
  }
#endif

  scene.destroyGameObject(*first);
  scene.destroyGameObject(*second);
  scripts.forgetClass(tuner->getUUID());
}
