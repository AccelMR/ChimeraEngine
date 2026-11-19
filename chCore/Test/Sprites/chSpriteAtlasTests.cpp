/************************************************************************/
/**
 * @file chSpriteAtlasTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of sprite atlases and the sprite animator.
 */
/************************************************************************/
#include <cstring>

#include "chAnimatorComponent.h"
#include "chAssetMeta.h"
#include "chFileStream.h"
#include "chGraphicsTypes.h"
#include "chScene.h"
#include "chSpriteAnimatorComponent.h"
#include "chSpriteAtlas.h"
#include "chSpriteAtlasAsset.h"
#include "chSpriteComponent.h"
#include "chStringUtils.h"
#include "chUUID.h"
#include "chVector2.h"
#include "chVector3.h"

#include "catch.hpp"
#include "chSpriteTestHelpers.h"
#include "chModelTestHelpers.h"

using namespace chEngineSDK;

TEST_CASE("chCore - Sprite atlas", "[Sprites]")
{
  const SpriteAtlas atlas = createTestAtlas();
  String error;
  REQUIRE(atlas.validate(error));

  SECTION("Names are found ignoring case")
  {
    REQUIRE(atlas.findFrame("IDLE_1") == 1);
    REQUIRE(atlas.findAnimation("run") == 1);
    REQUIRE(atlas.findFrame("jump_0") == SpriteAtlas::NOT_FOUND);
    REQUIRE(atlas.findAnimation("Jump") == SpriteAtlas::NOT_FOUND);
  }

  SECTION("Animations pick the step of a time")
  {
    const SpriteAnimation& run = atlas.getAnimations()[1];
    REQUIRE(run.getDuration() == Approx(0.4f));
    REQUIRE(run.getStepAt(0.0f) == 0);
    REQUIRE(run.getStepAt(0.05f) == 0);
    REQUIRE(run.getStepAt(0.15f) == 1);
    REQUIRE(run.getStepAt(0.25f) == 2);
    // Looping wraps, also backwards.
    REQUIRE(run.getStepAt(0.45f) == 0);
    REQUIRE(run.getStepAt(-0.05f) == 2);
    const SpriteAnimation& die = atlas.getAnimations()[2];
    REQUIRE(die.getStepAt(10.0f) == 1);
    REQUIRE(SpriteAnimation{}.getStepAt(1.0f) == 0);
  }

  SECTION("Broken atlases are refused with their reason")
  {
    SpriteAtlas duplicate = atlas;
    duplicate.getFrames()[1].name = "Idle_0";
    REQUIRE_FALSE(duplicate.validate(error));
    REQUIRE(StringUtils::containsIgnoreCase(error, "idle_0"));

    SpriteAtlas missingFrame = atlas;
    missingFrame.getAnimations()[0].frames[0].frame = 9;
    REQUIRE_FALSE(missingFrame.validate(error));

    SpriteAtlas noDuration = atlas;
    noDuration.getAnimations()[1].frames[2].duration = 0.0f;
    REQUIRE_FALSE(noDuration.validate(error));

    SpriteAtlas noSize = atlas;
    noSize.getFrames()[3].size = Vector2(0.0f, 32.0f);
    REQUIRE_FALSE(noSize.validate(error));
  }

  SECTION("It is saved and read with the same bytes")
  {
    SpriteAtlas withTexture = atlas;
    withTexture.setTextureId(chEngineSDK::UUID::createRandom());
    MemoryDataStream stream;
    withTexture.serialize(stream);
    const Vector<uint8> bytes = readAllBytes(stream);

    SpriteAtlas loaded;
    REQUIRE(loaded.deserialize(stream, error));
    REQUIRE(loaded.getTextureId() == withTexture.getTextureId());
    REQUIRE(loaded.getFrames().size() == 5);
    REQUIRE(loaded.getFrames()[4].name == "run_2");
    REQUIRE(loaded.getFrames()[4].position.x == 128.0f);
    REQUIRE(loaded.getFrames()[0].pivot.y == 0.0f);
    REQUIRE(loaded.getAnimations().size() == 3);
    REQUIRE_FALSE(loaded.getAnimations()[2].bLoop);
    REQUIRE(loaded.getAnimations()[1].frames[2].duration == 0.2f);

    MemoryDataStream again;
    loaded.serialize(again);
    REQUIRE(readAllBytes(again) == bytes);

    MemoryDataStream cut;
    cut.write(bytes.data(), bytes.size() - 1);
    cut.seek(0);
    REQUIRE_FALSE(SpriteAtlas().deserialize(cut, error));

    Vector<uint8> newer = bytes;
    const uint32 newerVersion = SpriteAtlas::SERIALIZATION_VERSION + 1;
    std::memcpy(newer.data(), &newerVersion, sizeof(newerVersion));
    MemoryDataStream newerStream;
    newerStream.write(newer.data(), newer.size());
    newerStream.seek(0);
    REQUIRE_FALSE(SpriteAtlas().deserialize(newerStream, error));

    // Read but refused: an animation step past the frames.
    SpriteAtlas broken = atlas;
    broken.getAnimations()[0].frames[0].frame = 40;
    MemoryDataStream brokenStream;
    broken.serialize(brokenStream);
    brokenStream.seek(0);
    REQUIRE_FALSE(SpriteAtlas().deserialize(brokenStream, error));
  }

#if USING(CH_EDITOR)
  SECTION("Frames from a grid and from cells")
  {
    Vector<SpriteAtlasFrame> frames;
    // 100 x 50 with a margin of 2 and 2 between cells leaves 22 x 22 cells.
    SpriteAtlas::makeGridFrames(100, 50, 4, 2, 2, 2, "hero", frames);
    REQUIRE(frames.size() == 8);
    REQUIRE(frames[0].name == "hero_0");
    REQUIRE(frames[0].position == Vector2(2.0f, 2.0f));
    REQUIRE(frames[0].size == Vector2(22.0f, 22.0f));
    REQUIRE(frames[1].position == Vector2(26.0f, 2.0f));
    REQUIRE(frames[7].name == "hero_7");
    REQUIRE(frames[7].position == Vector2(74.0f, 26.0f));

    SpriteAtlas::makeCellFrames(64, 32, 16, 16, 0, 0, "tile", frames);
    REQUIRE(frames.size() == 8);
    // A cell cut by the edge is left out.
    SpriteAtlas::makeCellFrames(70, 40, 16, 16, 0, 0, "tile", frames);
    REQUIRE(frames.size() == 8);
    SpriteAtlas::makeGridFrames(10, 10, 0, 2, 0, 0, "none", frames);
    REQUIRE(frames.empty());
    SpriteAtlas::makeGridFrames(10, 10, 20, 1, 0, 0, "none", frames);
    REQUIRE(frames.empty());
  }

  SECTION("Frames found around groups of opaque pixels")
  {
    // A: (0,0)-(1,0). B: (4,0). D: (2,1), touching A only by a corner, so its own group.
    // C: (3,3)-(5,3).
    constexpr uint32 kWidth = 6;
    constexpr uint32 kHeight = 4;
    Vector<uint8> pixels(kWidth * kHeight * 4, 0);
    const auto setOpaque = [&pixels](uint32 x, uint32 y) {
      pixels[(y * kWidth + x) * 4 + 3] = 255;
    };
    setOpaque(0, 0);
    setOpaque(1, 0);
    setOpaque(4, 0);
    setOpaque(2, 1);
    setOpaque(3, 3);
    setOpaque(4, 3);
    setOpaque(5, 3);

    Vector<SpriteAtlasFrame> frames;
    SpriteAtlas::detectFrames(pixels, kWidth, kHeight, 0, 0, "blob", frames);
    REQUIRE(frames.size() == 4);
    REQUIRE(frames[0].position == Vector2(0.0f, 0.0f));
    REQUIRE(frames[0].size == Vector2(2.0f, 1.0f));
    REQUIRE(frames[1].position == Vector2(4.0f, 0.0f));
    REQUIRE(frames[2].position == Vector2(2.0f, 1.0f));
    REQUIRE(frames[3].position == Vector2(3.0f, 3.0f));
    REQUIRE(frames[3].size == Vector2(3.0f, 1.0f));
    REQUIRE(frames[3].name == "blob_3");

    // Padding grows each frame but keeps it inside the image.
    SpriteAtlas::detectFrames(pixels, kWidth, kHeight, 0, 1, "blob", frames);
    REQUIRE(frames[0].position == Vector2(0.0f, 0.0f));
    REQUIRE(frames[0].size == Vector2(3.0f, 2.0f));
    REQUIRE(frames[3].size == Vector2(4.0f, 2.0f));

    // Pixels at or under the threshold count as empty.
    SpriteAtlas::detectFrames(pixels, kWidth, kHeight, 255, 0, "blob", frames);
    REQUIRE(frames.empty());
  }
#endif // USING(CH_EDITOR)
}

TEST_CASE("chCore - Sprite animator", "[Sprites]")
{
  Scene scene("Animator", chEngineSDK::UUID::createRandom());
  const SPtr<GameObject> hero = scene.createGameObject("Hero");
  SpriteComponent& sprite = hero->addComponent<SpriteComponent>();
  SpriteAnimatorComponent& animator = hero->addComponent<SpriteAnimatorComponent>();
  const SPtr<SpriteAtlasAsset> atlas = createTestAtlasAsset();
  animator.setAtlas(atlas);
  scene.updateTransforms();

  SECTION("A frame of the atlas sets the sprite's size and pivot")
  {
    sprite.setAtlasFrame(atlas, 0);
    scene.updateTransforms();
    REQUIRE(sprite.getSize().x == Approx(0.32f));
    const SpriteItem* item = nullptr;
    for (const SpriteItem& slot : scene.getSpriteSlots()) {
      item = slot.bInUse ? &slot : item;
    }
    REQUIRE(item);
    // Standing on its bottom center: the quad's center is half its height up.
    REQUIRE(item->instance.center.nearEqual(Vector3(0.0f, 0.0f, 0.16f), 1e-5f));

    sprite.setFrame(3);
    REQUIRE(sprite.getSize().x == Approx(0.64f));
    REQUIRE(item->instance.center.nearEqual(Vector3::ZERO, 1e-5f));

    sprite.setTexture(nullptr);
    REQUIRE(sprite.getAtlas() == nullptr);
    REQUIRE(sprite.getFrame() == SpriteComponent::NO_FRAME);
  }

  SECTION("Animations loop, finish and go backwards")
  {
    REQUIRE(animator.play("run"));
    REQUIRE(animator.getAnimationName() == "Run");
    REQUIRE(sprite.getAtlas() == atlas);
    REQUIRE(sprite.getFrame() == 2);

    scene.update(0.15f);
    REQUIRE(sprite.getFrame() == 3);
    scene.update(0.3f);
    REQUIRE(animator.getTime() == Approx(0.05f));
    REQUIRE(sprite.getFrame() == 2);

    // Playing it again only resumes.
    REQUIRE(animator.play("Run"));
    REQUIRE(animator.getTime() == Approx(0.05f));

    REQUIRE(animator.play("Die"));
    REQUIRE(sprite.getFrame() == 0);
    scene.update(0.6f);
    REQUIRE(animator.isFinished());
    REQUIRE_FALSE(animator.isPlaying());
    REQUIRE(sprite.getFrame() == 1);
    REQUIRE(animator.play("Die"));
    REQUIRE_FALSE(animator.isPlaying());
    REQUIRE(animator.play("Die", true));
    REQUIRE(animator.isPlaying());
    REQUIRE(sprite.getFrame() == 0);

    animator.setSpeed(-1.0f);
    REQUIRE(animator.play("Run", true));
    scene.update(0.05f);
    REQUIRE(animator.getTime() == Approx(0.35f));
    REQUIRE(sprite.getFrame() == 4);

    REQUIRE_FALSE(animator.play("Jump"));
    REQUIRE(animator.getAnimationName() == "Run");
    animator.stop();
    REQUIRE_FALSE(animator.isPlaying());
    REQUIRE(animator.getTime() == 0.0f);
  }

  SECTION("Paused, nothing moves")
  {
    REQUIRE(animator.play("Run"));
    animator.setPlaying(false);
    scene.update(0.15f);
    REQUIRE(sprite.getFrame() == 2);
  }

  SECTION("Atlas and animation are saved, and kept when the atlas is missing")
  {
    REQUIRE(animator.play("Idle"));
    animator.setSpeed(2.0f);
    MemoryDataStream stream;
    animator.serialize(stream);
    stream.seek(0);
    SpriteAnimatorComponent loaded;
    REQUIRE(loaded.deserialize(stream));
    REQUIRE(loaded.getAtlas() == nullptr);
    REQUIRE(loaded.getAtlasId() == atlas->getUUID());
    REQUIRE(loaded.getAnimationName() == "Idle");
    REQUIRE(loaded.getSpeed() == 2.0f);
    Vector<chEngineSDK::UUID> references;
    loaded.collectReferences(references);
    REQUIRE(references == Vector<chEngineSDK::UUID>{atlas->getUUID()});

    MemoryDataStream spriteStream;
    sprite.serialize(spriteStream);
    spriteStream.seek(0);
    SpriteComponent loadedSprite;
    REQUIRE(loadedSprite.deserialize(spriteStream));
    REQUIRE(loadedSprite.getAtlasId() == atlas->getUUID());
    REQUIRE(loadedSprite.getFrame() == 0);
    references.clear();
    loadedSprite.collectReferences(references);
    REQUIRE(references == Vector<chEngineSDK::UUID>{atlas->getUUID()});

    MemoryDataStream cut;
    const Vector<uint8> bytes = readAllBytes(stream);
    cut.write(bytes.data(), bytes.size() - 1);
    cut.seek(0);
    REQUIRE_FALSE(SpriteAnimatorComponent().deserialize(cut));
  }
}
