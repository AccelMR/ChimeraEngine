/************************************************************************/
/**
 * @file chTextTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of text layout, text sprites, glyph runs and TextComponent.
 */
/************************************************************************/
#include <chrono>

#include "chAngle.h"
#include "chAssetMeta.h"
#include "chBox.h"
#include "chCamera.h"
#include "chFileStream.h"
#include "chFont.h"
#include "chFontAsset.h"
#include "chGraphicsTypes.h"
#include "chLinearColor.h"
#include "chLogger.h"
#include "chMath.h"
#include "chNumberFormat.h"
#include "chRenderFrame.h"
#include "chRenderSettings.h"
#include "chScene.h"
#include "chShapeOverlap.h"
#include "chSpriteBatcher.h"
#include "chSpriteDraw.h"
#include "chStringUtils.h"
#include "chTextComponent.h"
#include "chTextLayout.h"
#include "chUUID.h"
#include "chVector2.h"
#include "chVector3.h"

#include "catch.hpp"
#include "chTextTestHelpers.h"
#include "chSpriteTestHelpers.h"

using namespace chEngineSDK;

CH_LOG_DECLARE_STATIC(SpriteTestLog, All);

TEST_CASE("chCore - Text layout", "[Text]")
{
  const SPtr<Font> font = makeTestFont();
  TextLayout layout;
  const float fontHeight = font->getAscent() - font->getDescent();

  SECTION("One line")
  {
    layout.build(*font, "Hello", {.size = 2.0f, .pivot = Vector2(0.0f, 0.0f)});
    REQUIRE(layout.getLineCount() == 1);
    REQUIRE(layout.getGlyphs().size() == 5);
    REQUIRE(layout.getSize().y == Approx(2.0f));
    REQUIRE(layout.getSize().x > 0.0f);
    REQUIRE(layout.getRadius() > 0.0f);
    for (SIZE_T i = 1; i < layout.getGlyphs().size(); ++i) {
      REQUIRE(layout.getGlyphs()[i].position.x > layout.getGlyphs()[i - 1].position.x);
    }
    // 'H' sits on the baseline, above the descent at the bottom of the block.
    REQUIRE(layout.getGlyphs()[0].position.y > 0.0f);

    const Vector2 size = layout.getSize();
    const Vector2 first = layout.getGlyphs()[0].position;
    layout.build(*font, "Hello", {.size = 2.0f, .pivot = Vector2(1.0f, 1.0f)});
    REQUIRE(layout.getGlyphs()[0].position.x == Approx(first.x - size.x));
    REQUIRE(layout.getGlyphs()[0].position.y == Approx(first.y - size.y));
  }

  SECTION("Spaces and line breaks")
  {
    layout.build(*font, "a b", {});
    REQUIRE(layout.getGlyphs().size() == 2);

    layout.build(*font, "A\nA", {.size = 1.0f});
    REQUIRE(layout.getLineCount() == 2);
    REQUIRE(layout.getGlyphs().size() == 2);
    const float lineHeight = font->getLineHeight() / fontHeight;
    REQUIRE(layout.getGlyphs()[0].position.y - layout.getGlyphs()[1].position.y ==
            Approx(lineHeight));
    REQUIRE(layout.getGlyphs()[0].position.x == Approx(layout.getGlyphs()[1].position.x));
    REQUIRE(layout.getSize().y == Approx(lineHeight + 1.0f));
  }

  SECTION("Wrapping")
  {
    layout.build(*font, "mmm", {.pivot = Vector2(0.0f, 1.0f)});
    const float wordWidth = layout.getSize().x;

    layout.build(*font,
                 "mmm mmm mmm",
                 {.wrapWidth = wordWidth * 1.5f, .pivot = Vector2(0.0f, 1.0f)});
    REQUIRE(layout.getLineCount() == 3);
    REQUIRE(layout.getGlyphs().size() == 9);
    REQUIRE(layout.getSize().x == Approx(wordWidth * 1.5f));
    REQUIRE(layout.getGlyphs()[3].position.x == Approx(layout.getGlyphs()[0].position.x));
    REQUIRE(layout.getGlyphs()[6].position.x == Approx(layout.getGlyphs()[0].position.x));
    REQUIRE(layout.getGlyphs()[3].position.y < layout.getGlyphs()[0].position.y);

    // A word longer than the line breaks inside it.
    layout.build(*font, "mmmmmm", {.wrapWidth = wordWidth * 1.2f});
    REQUIRE(layout.getLineCount() == 2);
  }

  SECTION("Alignment")
  {
    layout.build(*font, "mmmmmm\nm", {.align = TextAlign::Right, .pivot = Vector2(0.0f, 1.0f)});
    REQUIRE(layout.getGlyphs().size() == 7);
    REQUIRE(glyphRight(layout.getGlyphs()[6]) ==
            Approx(glyphRight(layout.getGlyphs()[5])).margin(1e-3f));

    layout.build(*font, "mmmmmm\nm", {.align = TextAlign::Center, .pivot = Vector2(0.0f, 1.0f)});
    const Span<const TextGlyph> glyphs = layout.getGlyphs();
    const float longCenter = (glyphs[0].position.x + glyphRight(glyphs[5])) * 0.5f;
    const float shortCenter = (glyphs[6].position.x + glyphRight(glyphs[6])) * 0.5f;
    REQUIRE(shortCenter == Approx(longCenter).margin(0.05f));
  }

  SECTION("Fixed digits")
  {
    layout.build(*font, "111", {.bMonospaceDigits = true});
    const float ones = layout.getSize().x;
    layout.build(*font, "888", {.bMonospaceDigits = true});
    REQUIRE(layout.getSize().x == Approx(ones));
  }

  SECTION("Color tags")
  {
    layout.build(*font, "<color=#ff0000>R</color>G", {});
    REQUIRE(layout.getGlyphs().size() == 2);
    REQUIRE(lowHalf(layout.getGlyphs()[0].colorRG) == Approx(1.0f));
    REQUIRE(highHalf(layout.getGlyphs()[0].colorRG) == Approx(0.0f));
    REQUIRE(highHalf(layout.getGlyphs()[1].colorRG) == Approx(1.0f));

    // sRGB 0x80 is about 0.216 in linear light; the tag's alpha is times the text's.
    layout.build(*font, "<color=#80808080>x", {.color = LinearColor(1.0f, 1.0f, 1.0f, 0.5f)});
    REQUIRE(lowHalf(layout.getGlyphs()[0].colorRG) == Approx(0.216f).margin(0.002f));
    REQUIRE(highHalf(layout.getGlyphs()[0].colorBA) == Approx(0.251f).margin(0.002f));

    layout.build(*font, "<<b", {});
    REQUIRE(layout.getGlyphs().size() == 2);
    layout.build(*font, "<b>", {});
    REQUIRE(layout.getGlyphs().size() == 3);
    layout.build(*font, "<color=#zz0000>", {});
    REQUIRE(layout.getGlyphs().size() == 15);
    layout.build(*font, "<color=#ff0000>R</color>G", {.bRichText = false});
    REQUIRE(layout.getGlyphs().size() == 25);
  }
}

TEST_CASE("chCore - Text sprites", "[Text]")
{
  const SPtr<Font> font = makeTestFont();
  TextLayout layout;
  layout.build(*font, "Hi!", {.size = 1.0f});
  const SIZE_T glyphCount = layout.getGlyphs().size();
  REQUIRE(glyphCount == 3);
  const auto modeOf = [](const SpriteItem& sprite) {
    return (sprite.instance.textureAndFlags >> GpuSpriteInstance::MODE_SHIFT) &
           GpuSpriteInstance::MODE_MASK;
  };
  const auto edgeOf = [](const SpriteItem& sprite) {
    return (sprite.instance.textureAndFlags >> GpuSpriteInstance::EDGE_SHIFT) &
           GpuSpriteInstance::EDGE_MASK;
  };

  SECTION("Fill only")
  {
    Vector<SpriteItem> sprites;
    SpriteDraw::appendTextSprites(*font, layout, {.sortingLayer = 2, .order = 5}, sprites);
    REQUIRE(sprites.size() == glyphCount);
    const uint32 flags = sprites[0].instance.textureAndFlags;
    REQUIRE((flags & GpuSpriteInstance::TEXTURE_INDEX_MASK) == GpuSpriteInstance::NO_TEXTURE);
    REQUIRE(modeOf(sprites[0]) == static_cast<uint32>(SpriteTextureMode::Sdf));
    REQUIRE((flags & GpuSpriteInstance::POINT_FILTER_BIT) == 0);
    REQUIRE(sprites[0].sortingLayer == 2);
    REQUIRE(sprites[0].order == 5);
    REQUIRE(sprites[0].radius > 0.0f);
  }

  SECTION("Shadow and outline go behind the fill")
  {
    Vector<SpriteItem> sprites;
    SpriteDraw::appendTextSprites(*font,
                                  layout,
                                  {.outlineWidth = 2.0f,
                                   .shadowColor = LinearColor(0.0f, 0.0f, 0.0f, 0.5f),
                                   .shadowOffset = Vector2(0.1f, -0.1f)},
                                  sprites);
    REQUIRE(sprites.size() == glyphCount * 3);
    // 2 of 8 pixels of spread move the edge by 32 of 255.
    REQUIRE(edgeOf(sprites[0]) == 32);
    REQUIRE(edgeOf(sprites[glyphCount]) == 32);
    REQUIRE(edgeOf(sprites[glyphCount * 2]) == 0);
    REQUIRE(sprites[0].instance.center.y ==
            Approx(sprites[glyphCount * 2].instance.center.y + 0.1f));
    REQUIRE(sprites[0].instance.center.z ==
            Approx(sprites[glyphCount * 2].instance.center.z - 0.1f));
    REQUIRE(highHalf(sprites[0].instance.colorBA) == Approx(0.5f));
  }

  SECTION("Placement and rotation")
  {
    Vector<SpriteItem> sprites;
    SpriteDraw::appendTextSprites(*font,
                                  layout,
                                  {.position = Vector2(3.0f, 4.0f),
                                   .depth = 2.0f,
                                   .rotation = Radian(Math::HALF_PI),
                                   .scale = 2.0f},
                                  sprites);
    const GpuSpriteInstance& first = sprites[0].instance;
    REQUIRE(first.center.x == Approx(2.0f));
    // Turned a quarter: the glyph's right side points up the screen.
    REQUIRE(first.halfAxisU.y == Approx(0.0f).margin(1e-5f));
    REQUIRE(first.halfAxisU.z == Approx(layout.getGlyphs()[0].size.x));
  }

  SECTION("Pixel fonts use coverage and the point filter")
  {
    const SPtr<Font> pixelFont = makeTestFont({.mode = FontRenderMode::Pixel, .rasterSize = 16});
    layout.build(*pixelFont, "Hi", {});
    Vector<SpriteItem> sprites;
    SpriteDraw::appendTextSprites(*pixelFont, layout, {.outlineWidth = 2.0f}, sprites);
    // Outlines need a distance field.
    REQUIRE(sprites.size() == 2);
    REQUIRE(modeOf(sprites[0]) == static_cast<uint32>(SpriteTextureMode::Coverage));
    REQUIRE((sprites[0].instance.textureAndFlags & GpuSpriteInstance::POINT_FILTER_BIT) != 0);
  }

  SECTION("A text that does not fit is dropped whole")
  {
    SpriteDraw draw;
    const int32 previousMax = g_cvarSpriteDrawMax.get();
    g_cvarSpriteDrawMax.set(4);
    draw.text(*font, layout, {});
    REQUIRE(draw.getSprites().size() == 3);
    draw.text(*font, layout, {});
    REQUIRE(draw.getSprites().size() == 3);
    g_cvarSpriteDrawMax.set(previousMax);
  }
}

TEST_CASE("chCore - Glyph runs", "[Text]")
{
  const SPtr<Font> font = makeTestFont();
  const float height = font->getHeight();
  GlyphRunItem item;

  SECTION("Glyphs and pens")
  {
    REQUIRE(SpriteDraw::makeGlyphRun(*font,
                                     "AV 1.5K",
                                     {.position = Vector2(2.0f, 3.0f),
                                      .depth = 1.0f,
                                      .size = 2.0f,
                                      .pivot = Vector2(0.0f, 0.0f),
                                      .sortingLayer = 4},
                                     item));
    const GpuGlyphRun& run = item.run;
    REQUIRE(run.glyphCount == 7);
    REQUIRE(run.glyphs[0] == font->findGlyph('A'));
    REQUIRE(run.glyphs[6] == font->findGlyph('K'));
    REQUIRE(run.pens[0] == 0);
    for (uint32 i = 1; i < run.glyphCount; ++i) {
      REQUIRE(run.pens[i] > run.pens[i - 1]);
    }
    // Kerning pulls V toward A.
    const float aAdvance = font->getGlyph(font->findGlyph('A')).advance / height;
    REQUIRE(static_cast<float>(run.pens[1]) / GpuGlyphRun::PEN_SCALE < aAdvance);
    REQUIRE(run.glyphs[7] == 0);
    REQUIRE(run.position.x == Approx(1.0f));
    REQUIRE(run.position.y == Approx(2.0f));
    REQUIRE(run.position.z == Approx(3.0f));
    REQUIRE(run.size == Approx(2.0f));
    // Pivot at the bottom left: the first pen is at the left, the baseline above the descent.
    REQUIRE(run.offset[0] == Approx(0.0f));
    REQUIRE(run.offset[1] == Approx(-font->getDescent() / height));
    REQUIRE((run.textureAndFlags & GpuSpriteInstance::TEXTURE_INDEX_MASK) ==
            GpuSpriteInstance::NO_TEXTURE);
    REQUIRE(((run.textureAndFlags >> GpuSpriteInstance::MODE_SHIFT) &
             GpuSpriteInstance::MODE_MASK) == static_cast<uint32>(SpriteTextureMode::Sdf));
    REQUIRE(lowHalf(run.rotation) == Approx(1.0f));
    REQUIRE(highHalf(run.rotation) == Approx(0.0f));
    REQUIRE(item.radius > 0.0f);
    REQUIRE(item.sortingLayer == 4);
  }

  SECTION("Centered pivot and fixed digits")
  {
    REQUIRE(SpriteDraw::makeGlyphRun(*font, "1181", {.bMonospaceDigits = true}, item));
    const float digit = font->getDigitAdvance() / height;
    const float step = (static_cast<float>(item.run.pens[3]) - item.run.pens[2]) /
                       GpuGlyphRun::PEN_SCALE;
    // 8 is centered in its cell, 1 too, so from 8 to 1 is a whole cell minus the halves.
    const float eight = font->getGlyph(font->findGlyph('8')).advance / height;
    const float one = font->getGlyph(font->findGlyph('1')).advance / height;
    REQUIRE(step == Approx(digit - (digit - eight) * 0.5f + (digit - one) * 0.5f).margin(2e-3f));
    REQUIRE(item.run.offset[0] == Approx(-2.0f * digit).margin(1e-4f));
    REQUIRE(item.run.offset[1] == Approx(-font->getDescent() / height - 0.5f));
  }

  SECTION("Limits")
  {
    REQUIRE_FALSE(SpriteDraw::makeGlyphRun(*font, "", {}, item));
    REQUIRE_FALSE(SpriteDraw::makeGlyphRun(*font, "\n", {}, item));
    REQUIRE(SpriteDraw::makeGlyphRun(*font, "A\nB", {}, item));
    REQUIRE(item.run.glyphCount == 2);
    REQUIRE(SpriteDraw::makeGlyphRun(*font, "0123456789abcdefghij", {}, item));
    REQUIRE(item.run.glyphCount == GpuGlyphRun::MAX_GLYPHS);
    REQUIRE(item.run.glyphs[15] == font->findGlyph('f'));
  }

  SECTION("Outline and shadow runs go first")
  {
    SpriteDraw draw;
    draw.glyphRun(*font,
                  "12",
                  {.outlineWidth = 2.0f,
                   .shadowColor = LinearColor(0.0f, 0.0f, 0.0f, 0.5f),
                   .shadowOffset = Vector2(0.1f, -0.2f)});
    const Span<const GlyphRunItem> runs = draw.getGlyphRuns();
    REQUIRE(runs.size() == 3);
    const auto edgeOf = [](const GlyphRunItem& run) {
      return (run.run.textureAndFlags >> GpuSpriteInstance::EDGE_SHIFT) &
             GpuSpriteInstance::EDGE_MASK;
    };
    REQUIRE(edgeOf(runs[0]) == 32);
    REQUIRE(edgeOf(runs[1]) == 32);
    REQUIRE(edgeOf(runs[2]) == 0);
    REQUIRE(runs[0].run.position.y == Approx(runs[2].run.position.y + 0.1f));
    REQUIRE(runs[0].run.position.z == Approx(runs[2].run.position.z - 0.2f));
    REQUIRE(highHalf(runs[0].run.colorBA) == Approx(0.5f));
    draw.clear();
    REQUIRE(draw.getGlyphRuns().empty());
  }

  SECTION("They sort with the sprites")
  {
    const Camera camera = createSpriteTestCamera();

    const auto spriteAt = [](int16 layer) {
      SpriteItem sprite;
      sprite.instance.center = Vector3::ZERO;
      sprite.radius = 1.0f;
      sprite.sortingLayer = layer;
      sprite.bInUse = true;
      return sprite;
    };
    const Vector<SpriteItem> sprites{spriteAt(0), spriteAt(2), spriteAt(1)};
    REQUIRE(SpriteDraw::makeGlyphRun(*font, "x", {.sortingLayer = 1}, item));
    Vector<GlyphRunItem> runs{item, item};
    runs[1].sortingLayer = 3;
    const Vector<GlyphRunBatch> batches{
        {.bufferIndex = 9, .firstRun = 5, .runCount = 7, .sortingLayer = 1},
        {.bufferIndex = 9, .firstRun = 0, .runCount = 0, .sortingLayer = 1}};

    SpriteBatcher batcher;
    batcher.build(sprites, camera.getFrustum(), camera.getForwardVector(), runs, batches);
    const Span<const SpriteRun> order = batcher.getRuns();
    // The sprites of layers 0 and 1 share a draw; then layer 1's glyph run and batch, the
    // sprite of layer 2 and the run of layer 3. The empty batch is dropped.
    REQUIRE(order.size() == 5);
    REQUIRE(order[0].kind == SpriteRunKind::Sprites);
    REQUIRE(order[0].instanceCount == 2);
    REQUIRE(order[1].kind == SpriteRunKind::GlyphRuns);
    REQUIRE(order[2].kind == SpriteRunKind::GlyphRunBatch);
    REQUIRE(order[2].bufferIndex == 9);
    REQUIRE(order[2].firstInstance == 5);
    REQUIRE(order[2].instanceCount == 7);
    REQUIRE(order[3].kind == SpriteRunKind::Sprites);
    REQUIRE(order[3].firstInstance == 2);
    REQUIRE(order[4].kind == SpriteRunKind::GlyphRuns);
    REQUIRE(order[4].firstInstance == 1);
    REQUIRE(batcher.getInstances().size() == 3);
    REQUIRE(batcher.getGlyphRuns().size() == 2);

    // A run the camera does not see is dropped.
    runs[1].run.position = Vector3(0.0f, 500.0f, 0.0f);
    batcher.build(sprites, camera.getFrustum(), camera.getForwardVector(), runs, {});
    REQUIRE(batcher.getGlyphRuns().size() == 1);
  }
}

TEST_CASE("chCore - Glyph run benchmark", "[Text][.]")
{
  const SPtr<Font> font = makeTestFont();
  constexpr uint32 kNumbers = 100000;
  SpriteDraw draw;
  SpriteBatcher batcher;
  Camera camera = createSpriteTestCamera();
  camera.setOrthographicSize(100.0f);

  const int32 previousMax = g_cvarSpriteDrawMax.get();
  g_cvarSpriteDrawMax.set(static_cast<int32>(kNumbers * 2));
  ANSICHAR buffer[NumberFormat::MAX_CHARS];
  double total = 0.0;
  constexpr uint32 kFrames = 20;
  for (uint32 frame = 0; frame < kFrames; ++frame) {
    const auto start = std::chrono::steady_clock::now();
    draw.clear();
    for (uint32 i = 0; i < kNumbers; ++i) {
      const double value = 1234.5 * (i + 1) * (frame + 1);
      const float x = static_cast<float>(i % 400) * 0.45f - 90.0f;
      const float y = static_cast<float>(i / 400) * 0.7f - 85.0f;
      draw.glyphRun(*font,
                    NumberFormat::format(buffer, value),
                    {.position = Vector2(x, y), .size = 0.5f, .bMonospaceDigits = true});
    }
    batcher.build({}, camera.getFrustum(), camera.getForwardVector(), draw.getGlyphRuns(), {});
    const auto end = std::chrono::steady_clock::now();
    if (frame > 1) {
      total += std::chrono::duration<double, std::milli>(end - start).count();
    }
  }
  g_cvarSpriteDrawMax.set(previousMax);
  CH_LOG_INFO(SpriteTestLog,
              "{0} numbers formatted, built as glyph runs and sorted: {1:.3f} ms per frame, "
              "{2} runs drawn",
              kNumbers,
              total / (kFrames - 2),
              batcher.getGlyphRuns().size());
  REQUIRE(batcher.getGlyphRuns().size() == kNumbers);
}

TEST_CASE("chCore - Text component", "[Text]")
{
  const SPtr<FontAsset> font = makeTestFontAsset();
  Scene scene("Text", chEngineSDK::UUID::createRandom());
  const SPtr<GameObject> gameObject = scene.createGameObject("Label");
  gameObject->getTransform().setLocalPosition(Vector3(1.0f, 2.0f, 3.0f));
  TextComponent& text = gameObject->addComponent<TextComponent>("Score 12", font);
  scene.updateTransforms();

  const auto getItem = [&scene]() -> const TextItem& {
    for (const TextItem& item : scene.getTextSlots()) {
      if (item.bInUse) {
        return item;
      }
    }
    FAIL("no text in the scene");
    return scene.getTextSlots()[0];
  };

  SECTION("A short text is one glyph run at its object")
  {
    REQUIRE(text.isDrawnAsGlyphRuns());
    const TextItem& item = getItem();
    REQUIRE(item.owner == gameObject.get());
    REQUIRE(item.runCount == 1);
    REQUIRE(item.sprites.empty());
    REQUIRE(item.runs[0].run.glyphCount == 8);
    REQUIRE(item.runs[0].run.position.nearEqual(Vector3(1.0f, 2.0f, 3.0f)));
    REQUIRE(text.getTextSize().x > 0.0f);
    REQUIRE(text.getTextSize().y == Approx(1.0f));

    AABox bounds;
    REQUIRE(text.getWorldBounds(bounds));
    REQUIRE(ShapeOverlap::pointBox(Vector3(1.0f, 2.0f, 3.0f), bounds));
  }

  SECTION("It follows the object after the transforms are rebuilt")
  {
    gameObject->getTransform().setLocalPosition(Vector3(0.0f, -4.0f, 5.0f));
    gameObject->getTransform().setLocalScale(Vector3(2.0f, 2.0f, 2.0f));
    scene.updateTransforms();
    REQUIRE(getItem().runs[0].run.position.nearEqual(Vector3(0.0f, -4.0f, 5.0f)));
    REQUIRE(getItem().runs[0].run.size == Approx(2.0f));
  }

  SECTION("Long, broken or tagged texts are laid out glyph by glyph")
  {
    text.setText("First line\nSecond line");
    text.update(0.0f);
    REQUIRE_FALSE(text.isDrawnAsGlyphRuns());
    REQUIRE(getItem().runCount == 0);
    // 19 glyphs with an image: the two spaces and the line break have none.
    REQUIRE(getItem().sprites.size() == 19);
    REQUIRE(text.getTextSize().y > 1.5f);

    text.setText("<color=#ff0000>red</color>");
    text.update(0.0f);
    REQUIRE_FALSE(text.isDrawnAsGlyphRuns());
    REQUIRE(getItem().sprites.size() == 3);

    text.setText("This one has more than sixteen characters");
    text.update(0.0f);
    REQUIRE_FALSE(text.isDrawnAsGlyphRuns());

    // Short again: back to a run.
    text.setText("OK");
    text.update(0.0f);
    REQUIRE(text.isDrawnAsGlyphRuns());
    REQUIRE(getItem().sprites.empty());
  }

  SECTION("A run wider than the wrap width is laid out")
  {
    text.setText("WWWWWWWWWWWW");
    text.setWrapWidth(1.0f);
    text.update(0.0f);
    REQUIRE_FALSE(text.isDrawnAsGlyphRuns());
    REQUIRE(getItem().sprites.size() == 12);
  }

  SECTION("Outline and shadow")
  {
    text.setOutline(2.0f, LinearColor::BLACK);
    text.setShadow(LinearColor(0.0f, 0.0f, 0.0f, 0.5f), Vector2(0.1f, -0.1f));
    text.update(0.0f);
    REQUIRE(getItem().runCount == 3);
    text.setText("Two\nlines");
    text.update(0.0f);
    // A sprite per glyph and pass.
    REQUIRE(getItem().sprites.size() == 8 * 3);
  }

  SECTION("Numbers")
  {
    text.setNumberFormat({.notation = NumberNotation::Suffix, .decimals = 1});
    text.setNumber(1234.0);
    REQUIRE(text.getText() == "1.2K");
    text.update(0.0f);
    const uint16 firstGlyph = getItem().runs[0].run.glyphs[0];
    text.setNumber(1240.0);
    REQUIRE(text.getText() == "1.2K");
    text.setNumber(5.5e6);
    REQUIRE(text.getText() == "5.5M");
    text.update(0.0f);
    REQUIRE(getItem().runs[0].run.glyphs[0] != firstGlyph);
  }

  SECTION("Disabled, it leaves the scene")
  {
    text.setEnabled(false);
    for (const TextItem& item : scene.getTextSlots()) {
      REQUIRE_FALSE(item.bInUse);
    }
    text.setEnabled(true);
    REQUIRE(getItem().runCount == 1);
  }

  SECTION("The frame copies the runs and sprites")
  {
    const SPtr<GameObject> second = scene.createGameObject("Paragraph");
    second->addComponent<TextComponent>("A\nB", font);
    scene.updateTransforms();
    RenderFrame frame;
    Camera camera = createSpriteTestCamera();
    frame.extract(scene, camera, LinearColor::BLACK);
    REQUIRE(frame.getGlyphRuns().size() == 1);
    REQUIRE(frame.getSprites().size() == 2);
  }

  SECTION("Saving keeps every setting and the font reference")
  {
    text.setSize(0.5f);
    text.setColor(LinearColor(1.0f, 0.5f, 0.25f, 1.0f));
    text.setAlign(TextAlign::Right);
    text.setWrapWidth(7.0f);
    text.setPivot(Vector2(1.0f, 0.0f));
    text.setMonospaceDigits(true);
    text.setRichText(false);
    text.setSorting(4, -3);
    text.setBlendMode(SpriteBlendMode::Additive);
    text.setOutline(1.5f, LinearColor(0.0f, 0.0f, 1.0f, 1.0f));
    text.setNumberFormat({.notation = NumberNotation::Scientific, .decimals = 3});

    MemoryDataStream stream;
    text.serialize(stream);
    stream.seek(0);
    TextComponent loaded;
    REQUIRE(loaded.deserialize(stream));
    REQUIRE(loaded.getText() == "Score 12");
    // No AssetManager in tests, so the font is not found, but its id is kept.
    REQUIRE(loaded.getFontId() == font->getUUID());
    REQUIRE(loaded.getSize() == Approx(0.5f));
    REQUIRE(loaded.getColor().g == Approx(0.5f));
    REQUIRE(loaded.getAlign() == TextAlign::Right);
    REQUIRE(loaded.getWrapWidth() == Approx(7.0f));
    REQUIRE(loaded.getPivot().x == Approx(1.0f));
    REQUIRE(loaded.isMonospaceDigits());
    REQUIRE_FALSE(loaded.isRichText());
    REQUIRE(loaded.getSortingLayer() == 4);
    REQUIRE(loaded.getOrder() == -3);
    REQUIRE(loaded.getBlendMode() == SpriteBlendMode::Additive);
    REQUIRE(loaded.getOutlineWidth() == Approx(1.5f));
    REQUIRE(loaded.getNumberFormat().notation == NumberNotation::Scientific);
    REQUIRE(loaded.getNumberFormat().decimals == 3);

    Vector<chEngineSDK::UUID> references;
    loaded.collectReferences(references);
    REQUIRE(references.size() == 1);

    // Cut short, it is refused.
    Vector<uint8> bytes(stream.size() - 4);
    stream.seek(0);
    REQUIRE(stream.read(bytes.data(), bytes.size()) == bytes.size());
    MemoryDataStream cut;
    REQUIRE(cut.write(bytes.data(), bytes.size()) == bytes.size());
    cut.seek(0);
    TextComponent broken;
    REQUIRE_FALSE(broken.deserialize(cut));
  }
}
