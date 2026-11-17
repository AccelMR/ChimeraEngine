/************************************************************************/
/**
 * @file chScriptSpriteTextTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of sprites, sprite animators and text in scripts.
 */
/************************************************************************/
#include <chrono>

#include "chAlgorithm.h"
#include "chAnimatorComponent.h"
#include "chFileStream.h"
#include "chFloat16.h"
#include "chFont.h"
#include "chFontAsset.h"
#include "chGraphicsTypes.h"
#include "chNumberFormat.h"
#include "chScene.h"
#include "chScriptAsset.h"
#include "chScriptBinder.h"
#include "chScriptComponent.h"
#include "chScriptManager.h"
#include "chScriptMathTypes.h"
#include "chScriptObjectTypes.h"
#include "chSpriteAnimatorComponent.h"
#include "chSpriteAtlas.h"
#include "chSpriteComponent.h"
#include "chSpriteDraw.h"
#include "chTextComponent.h"
#include "chUUID.h"
#include "chVector2.h"
#include "chVector3.h"

#include "catch.hpp"
#include "chTextTestHelpers.h"
#include "chSpriteTestHelpers.h"
#include "chScriptTestHelpers.h"

using namespace chEngineSDK;

CH_LOG_DECLARE_STATIC(ScriptTestLog, All);

TEST_CASE("chCore - Script sprites", "[Scripting]")
{
  ScriptManager& scripts = getTestScriptManager();
  Scene scene("Sprites", chEngineSDK::UUID::createRandom());
  // Reset on any exit, so a failed check leaves no dangling scene for later tests.
  struct SceneGuard
  {
    ScriptManager& scripts;
    ~SceneGuard() { scripts.setCurrentScene(nullptr); }
  } guard{scripts};
  scripts.setCurrentScene(&scene);
  scripts.beginUpdate(0.0f);

  SECTION("A style places sprites in the frame list")
  {
    REQUIRE(scripts.runString(R"lua(
      local style = Sprites.makeStyle{ size = Vector2(2, 1), color = Color(1, 0, 0, 1),
                                       layer = 3, order = -2, blend = "Additive",
                                       pivot = Vector2(0, 0), depth = 4 }
      Sprites.draw(style, 1, 2)
      Sprites.draw(style, 5, 6, 90, 2, Color(0, 1, 0, 0.5))
      local copy = style:copy()
      copy:setSorting(7)
      copy:setSize(Vector2(3, 3))
      assert(style:getSize().x == 2 and copy:getSize().x == 3)
      Sprites.draw(copy, 0, 0)
      Sprites.draw(Sprites.makeStyle(), 0, 0)
    )lua"));
    const Span<const SpriteItem> sprites = scene.getSpriteDraw().getSprites();
    REQUIRE(sprites.size() == 4);
    REQUIRE(sprites[0].instance.center.nearEqual(Vector3(4.0f, 2.0f, 2.5f), 1e-5f));
    REQUIRE(sprites[0].sortingLayer == 3);
    REQUIRE(sprites[0].order == -2);
    REQUIRE(sprites[0].blendMode == SpriteBlendMode::Additive);
    REQUIRE(Float16::toFloat(static_cast<uint16>(sprites[0].instance.colorRG & 0xFFFF)) ==
            1.0f);
    // Turned 90 degrees and twice as big, with the color given to the call.
    REQUIRE(sprites[1].instance.halfAxisU.nearEqual(Vector3(0.0f, 0.0f, 2.0f), 1e-4f));
    REQUIRE(sprites[1].instance.center.nearEqual(Vector3(4.0f, 4.0f, 8.0f), 1e-4f));
    REQUIRE(Float16::toFloat(static_cast<uint16>(sprites[1].instance.colorBA >> 16)) == 0.5f);
    REQUIRE(sprites[2].sortingLayer == 7);
    REQUIRE(sprites[2].order == -2);
    REQUIRE(sprites[2].instance.halfAxisU.y == Approx(1.5f));
    // The default style is a one-unit square around the point.
    REQUIRE(sprites[3].instance.center.nearEqual(Vector3::ZERO));
    REQUIRE(sprites[3].instance.halfAxisU.y == Approx(0.5f));
    REQUIRE(sprites[3].instance.textureAndFlags ==
            (GpuSpriteInstance::NO_TEXTURE | GpuSpriteInstance::POINT_FILTER_BIT));
  }

  SECTION("Sprites drawn in a fixed step go to the fixed list")
  {
    scripts.beginFixedUpdate(1.0f / 60.0f);
    REQUIRE(scripts.runString("Sprites.draw(Sprites.makeStyle(), 0, 0)"));
    scripts.beginUpdate(0.0f);
    REQUIRE(scene.getFixedSpriteDraw().getSprites().size() == 1);
    REQUIRE(scene.getSpriteDraw().getSprites().empty());
  }

  SECTION("Mistakes raise, and without a scene nothing is drawn")
  {
    REQUIRE_FALSE(scripts.runString("Sprites.makeStyle{ colour = Color(1, 1, 1, 1) }"));
    REQUIRE_FALSE(scripts.runString("Sprites.makeStyle{ blend = 'Over' }"));
    REQUIRE_FALSE(scripts.runString("Sprites.makeStyle{ layer = 40000 }"));
    REQUIRE_FALSE(scripts.runString("Sprites.makeStyle(5)"));
    REQUIRE_FALSE(scripts.runString("Sprites.draw(5, 0, 0)"));
    REQUIRE_FALSE(scripts.runString("Sprites.draw(Sprites.makeStyle(), 'x', 0)"));
    REQUIRE(scene.getSpriteDraw().getSprites().empty());

    scripts.setCurrentScene(nullptr);
    REQUIRE(scripts.runString("Sprites.draw(Sprites.makeStyle(), 0, 0)"));
    REQUIRE(scene.getSpriteDraw().getSprites().empty());
  }

  SECTION("Texture properties are picked in the editor and go into packages")
  {
    ScriptPropertyType type = ScriptPropertyType::Number;
    REQUIRE(ScriptPropertyUtils::fromTypeName("Texture", type));
    REQUIRE(type == ScriptPropertyType::Texture);
    REQUIRE(StringView(ScriptPropertyUtils::getTypeName(type)) == "Texture");

    ScriptPropertyValue value = ScriptPropertyValue::makeDefault(ScriptPropertyType::Texture);
    REQUIRE(value.object.isNull());
    value.object = chEngineSDK::UUID::createRandom();
    MemoryDataStream stream;
    ScriptPropertyUtils::writeValue(stream, value);
    stream.seek(0);
    ScriptPropertyValue loaded;
    REQUIRE(ScriptPropertyUtils::readValue(stream, loaded));
    REQUIRE(loaded.type == ScriptPropertyType::Texture);
    REQUIRE(loaded.object == value.object);

    const SPtr<ScriptAsset> painter = createTestScriptAsset("Painter", R"lua(
      local Painter = {}
      Painter.properties = { { "image", type = "Texture" } }
      return Painter
    )lua");
    const ScriptClassInfo* painterClass = scripts.getClass(*painter);
    REQUIRE(painterClass);
    REQUIRE(painterClass->bValid);
    REQUIRE(painterClass->properties.size() == 1);
    REQUIRE(painterClass->properties[0].defaultValue.type == ScriptPropertyType::Texture);

    // Like the other asset types, a texture has no default in the script.
    const SPtr<ScriptAsset> broken = createTestScriptAsset("BrokenPainter", R"lua(
      local BrokenPainter = {}
      BrokenPainter.properties = { { "image", 5, type = "Texture" } }
      return BrokenPainter
    )lua");
    const ScriptClassInfo* brokenClass = scripts.getClass(*broken);
    REQUIRE((brokenClass == nullptr || !brokenClass->bValid));

    ScriptComponent component(painter);
    component.setOverride("image", value);
    Vector<chEngineSDK::UUID> references;
    component.collectReferences(references);
    REQUIRE(Algorithm::contains(references, value.object));
  }

  SECTION("Atlas properties are picked in the editor and go into packages")
  {
    ScriptPropertyType type = ScriptPropertyType::Number;
    REQUIRE(ScriptPropertyUtils::fromTypeName("Atlas", type));
    REQUIRE(type == ScriptPropertyType::SpriteAtlas);

    ScriptPropertyValue value =
        ScriptPropertyValue::makeDefault(ScriptPropertyType::SpriteAtlas);
    value.object = chEngineSDK::UUID::createRandom();
    MemoryDataStream stream;
    ScriptPropertyUtils::writeValue(stream, value);
    stream.seek(0);
    ScriptPropertyValue loaded;
    REQUIRE(ScriptPropertyUtils::readValue(stream, loaded));
    REQUIRE(loaded.type == ScriptPropertyType::SpriteAtlas);
    REQUIRE(loaded.object == value.object);

    const SPtr<ScriptAsset> animated = createTestScriptAsset("AtlasUser", R"lua(
      local AtlasUser = {}
      AtlasUser.properties = { { "frames", type = "Atlas" } }
      function AtlasUser:onStart()
        assert(self.frames == nil or getmetatable(self.frames) == "Atlas")
      end
      return AtlasUser
    )lua");
    const ScriptClassInfo* atlasClass = scripts.getClass(*animated);
    REQUIRE(atlasClass);
    REQUIRE(atlasClass->bValid);
    REQUIRE(atlasClass->properties[0].defaultValue.type == ScriptPropertyType::SpriteAtlas);

    ScriptComponent component(animated);
    component.setOverride("frames", value);
    Vector<chEngineSDK::UUID> references;
    component.collectReferences(references);
    REQUIRE(Algorithm::contains(references, value.object));
  }
}

/*
 * Hidden: run it by name in a Release build,
 *   chCoreTest "chCore - Script sprites benchmark"
 * Bullets moved by a Lua loop over plain number arrays, each drawn with one Sprites.draw.
 */
TEST_CASE("chCore - Script sprites benchmark", "[Scripting][.benchmark]")
{
  constexpr uint32 kFrames = 100;
  ScriptManager& scripts = getTestScriptManager();
  Scene scene("SpriteBench", chEngineSDK::UUID::createRandom());
  struct SceneGuard
  {
    ScriptManager& scripts;
    ~SceneGuard() { scripts.setCurrentScene(nullptr); }
  } guard{scripts};
  scripts.setCurrentScene(&scene);
  REQUIRE(scripts.runString(R"lua(
    BulletCount = 10000
    local px, py, vx, vy = {}, {}, {}, {}
    for i = 1, BulletCount do
      local angle = i * 0.61
      px[i], py[i] = math.cos(angle) * 0.5, math.sin(angle) * 0.5
      vx[i], vy[i] = math.cos(angle) * 2, math.sin(angle) * 2
    end
    local style = Sprites.makeStyle{ size = Vector2(0.1, 0.1), layer = 2, blend = "Additive" }
    local draw = Sprites.draw
    function MoveBullets(dt)
      for i = 1, BulletCount do
        local x, y = px[i] + vx[i] * dt, py[i] + vy[i] * dt
        if x > 9 or x < -9 or y > 5 or y < -5 then x, y = 0, 0 end
        px[i], py[i] = x, y
        draw(style, x, y)
      end
    end
  )lua"));

  const auto runFrame = [&]() {
    scene.getSpriteDraw().clear();
    scripts.beginUpdate(1.0f / 60.0f);
    REQUIRE(scripts.runString("MoveBullets(1 / 60)"));
  };
  runFrame();
  const auto start = std::chrono::steady_clock::now();
  for (uint32 frame = 0; frame < kFrames; ++frame) {
    runFrame();
  }
  const auto end = std::chrono::steady_clock::now();
  const double milliseconds =
      std::chrono::duration<double, std::milli>(end - start).count() / kFrames;
  CH_LOG_INFO(ScriptTestLog, "Lua: 10000 bullets moved and drawn: {:.3f} ms per frame",
              milliseconds);
  REQUIRE(scene.getSpriteDraw().getSprites().size() == 10000);
}

TEST_CASE("chCore - Script sprite animator", "[Scripting]")
{
  ScriptManager& scripts = getTestScriptManager();
  Scene scene("SpriteScripts", chEngineSDK::UUID::createRandom());
  struct SceneGuard
  {
    ScriptManager& scripts;
    ~SceneGuard() { scripts.setCurrentScene(nullptr); }
  } guard{scripts};
  scripts.setCurrentScene(&scene);

  const SPtr<GameObject> hero = scene.createGameObject("Hero");
  SpriteComponent& sprite = hero->addComponent<SpriteComponent>();
  hero->addComponent<SpriteAnimatorComponent>().setAtlas(createTestAtlasAsset());
  scene.createGameObject("Plain");
  scene.updateTransforms();

  REQUIRE(scripts.runString(R"lua(
    local hero = Scene.find("Hero")
    local sprite = hero:getSprite()
    local animator = hero:getSpriteAnimator()
    assert(sprite and animator and hero:getSprite() == sprite)
    assert(Scene.find("Plain"):getSprite() == nil)
    assert(Scene.find("Plain"):getSpriteAnimator() == nil)
    assert(sprite:getGameObject() == hero and animator:getGameObject() == hero)

    animator:play("run")
    assert(animator:getAnimation() == "Run" and animator:isPlaying())
    assert(sprite:getFrame() == 3)
    animator:pause()
    assert(not animator:isPlaying())
    animator:resume()
    assert(animator:isPlaying() and not animator:isFinished())
    animator:setSpeed(2)
    assert(animator:getSpeed() == 2)
    animator:setTime(0.15)
    assert(math.abs(animator:getTime() - 0.15) < 1e-6 and sprite:getFrame() == 4)
    assert(not pcall(animator.play, animator, "Jump"))

    sprite:setFrame("idle_1")
    assert(sprite:getFrame() == 2)
    sprite:setFrame(1)
    assert(sprite:getFrame() == 1)
    assert(not pcall(sprite.setFrame, sprite, 99))
    assert(not pcall(sprite.setFrame, sprite, "nothing"))

    sprite:setColor(Color(1, 0, 0, 1))
    assert(sprite:getColor().r == 1 and sprite:getColor().g == 0)
    sprite:setFlip(true)
    assert(sprite:isFlippedX() and not sprite:isFlippedY())
    sprite:setSorting(4, 2)
    local layer, order = sprite:getSorting()
    assert(layer == 4 and order == 2)
    sprite:setSorting(5)
    layer, order = sprite:getSorting()
    assert(layer == 5 and order == 2)
    sprite:setVisible(false)
    assert(not sprite:isVisible())
    sprite:setVisible(true)
    HeroSprite = sprite
  )lua"));
  REQUIRE(sprite.getSortingLayer() == 5);
  REQUIRE(sprite.isFlippedX());

  // A removed component is an error, never a dangling pointer.
  hero->removeComponent(sprite);
  REQUIRE_FALSE(scripts.runString("HeroSprite:setColor(Color(1, 1, 1, 1))"));
  REQUIRE(scripts.runString("HeroSprite = nil"));
}

TEST_CASE("chCore - Script text", "[Scripting]")
{
  const SPtr<FontAsset> font = makeTestFontAsset();
  const DefaultFontGuard defaultFont(font->getFont());
  ScriptManager& scripts = getTestScriptManager();
  Scene scene("Texts", chEngineSDK::UUID::createRandom());
  struct SceneGuard
  {
    ScriptManager& scripts;
    ~SceneGuard() { scripts.setCurrentScene(nullptr); }
  } guard{scripts};
  scripts.setCurrentScene(&scene);
  scripts.beginUpdate(0.0f);

  SECTION("Short texts and numbers are glyph runs, long ones are laid out")
  {
    REQUIRE(scripts.runString(R"lua(
      local style = Text.makeStyle{ size = 2, color = Color(1, 0, 0, 1), layer = 5, order = 1,
                                    pivot = Vector2(0, 0), monospaceDigits = true,
                                    notation = "Suffix", decimals = 1, depth = 3 }
      Text.draw(style, "Hello", 1, 2)
      Text.draw(style, 1234, 0, 0)
      Text.draw(nil, "Two\nlines", 0, 0)
      Text.draw(Font.getDefault(), "<color=#00ff00>tag</color>", 0, 0)
    )lua"));
    const Span<const GlyphRunItem> runs = scene.getSpriteDraw().getGlyphRuns();
    REQUIRE(runs.size() == 2);
    REQUIRE(runs[0].run.glyphCount == 5);
    REQUIRE(runs[0].run.position.nearEqual(Vector3(3.0f, 1.0f, 2.0f)));
    REQUIRE(runs[0].run.size == Approx(2.0f));
    REQUIRE(runs[0].sortingLayer == 5);
    REQUIRE(runs[0].order == 1);
    REQUIRE(Float16::toFloat(static_cast<uint16>(runs[0].run.colorRG & 0xFFFF)) == 1.0f);
    // 1234 with one decimal and a suffix is "1.2K".
    REQUIRE(runs[1].run.glyphCount == 4);
    // "Two lines" and "tag" go glyph by glyph.
    REQUIRE(scene.getSpriteDraw().getSprites().size() == 8 + 3);
  }

  SECTION("Outline and shadow add runs behind")
  {
    REQUIRE(scripts.runString(R"lua(
      local style = Text.makeStyle{ outlineWidth = 2, shadowColor = Color(0, 0, 0, 0.5) }
      Text.draw(style, "12", 0, 0, 45, 2, Color(0, 0, 1, 1))
    )lua"));
    REQUIRE(scene.getSpriteDraw().getGlyphRuns().size() == 3);
  }

  SECTION("Measure, format and styles")
  {
    REQUIRE(scripts.runString(R"lua(
      local w, h = Text.measure(nil, "Hello")
      assert(w > 0 and math.abs(h - 1) < 1e-4)
      local w2 = Text.measure(Text.makeStyle{ size = 2 }, "Hello")
      assert(math.abs(w2 - 2 * w) < 1e-3)
      assert(Text.formatNumber(1500) == "1.5K")
      assert(Text.formatNumber(123456, "Scientific", 1) == "1.2e5")
      local style = Text.makeStyle{ size = 3 }
      local copy = style:copy()
      copy:setSize(4)
      assert(style:getSize() == 3 and copy:getSize() == 4)
      assert(tostring(Font.getDefault()) == "Font(Default)")
      assert(Font.getDefault():isValid())
    )lua"));
  }

  SECTION("A text component through its handle")
  {
    const SPtr<GameObject> label = scene.createGameObject("Label");
    TextComponent& text = label->addComponent<TextComponent>("Start");
    scene.updateTransforms();
    REQUIRE(scripts.runString(R"lua(
      local object = Scene.find("Label")
      local text = object:getText()
      assert(text == object:getText())
      assert(text:getText() == "Start")
      text:setNumberFormat("Plain")
      text:setText(42)
      assert(text:getText() == "42")
      text:setColor(Color(0, 1, 0, 1))
      text:setSize(3)
      text:setSorting(2, 7)
      text:setOutline(1)
      text:setShadow(Color(0, 0, 0, 1), Vector2(0.1, -0.1))
      assert(text:getSize() == 3)
      local layer, order = text:getSorting()
      assert(layer == 2 and order == 7)
      assert(text:getGameObject() == object)
      assert(text:getFont() == Font.getDefault())
      assert(Scene.find("Label"):getText():isVisible())
    )lua"));
    REQUIRE(text.getText() == "42");
    REQUIRE(text.getSize() == Approx(3.0f));
    REQUIRE(text.getSortingLayer() == 2);
    REQUIRE(text.getShadowColor().a == Approx(1.0f));

    const SPtr<GameObject> plain = scene.createGameObject("Plain");
    REQUIRE(scripts.runString(R"lua(
      local plain = Scene.find("Plain")
      assert(plain:getText() == nil)
      local added = plain:addText("Hi")
      assert(added == plain:getText() and added:getText() == "Hi")
      assert(plain:addText(7) == added and added:getText() == "7")
    )lua"));
    REQUIRE(plain->getComponent<TextComponent>() != nullptr);
  }

  SECTION("Mistakes raise")
  {
    REQUIRE_FALSE(scripts.runString("Text.makeStyle{ sise = 2 }"));
    REQUIRE_FALSE(scripts.runString("Text.makeStyle{ align = 'Middle' }"));
    REQUIRE_FALSE(scripts.runString("Text.makeStyle{ decimals = 12 }"));
    REQUIRE_FALSE(scripts.runString("Text.draw(5, 'x', 0, 0)"));
    REQUIRE_FALSE(scripts.runString("Text.formatNumber(1, 'Roman')"));
  }

  SECTION("Without a scene nothing is drawn")
  {
    scripts.setCurrentScene(nullptr);
    REQUIRE(scripts.runString("Text.draw(nil, 'x', 0, 0)"));
    REQUIRE(scene.getSpriteDraw().getGlyphRuns().empty());
  }
}
