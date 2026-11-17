/************************************************************************/
/**
 * @file chSpriteTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of sprites, the sprite batcher and sprites drawn without GameObjects.
 */
/************************************************************************/
#include <chrono>
#include <cstring>

#include "chAngle.h"
#include "chBox.h"
#include "chCamera.h"
#include "chComponentRegister.h"
#include "chEnginePaths.h"
#include "chFileStream.h"
#include "chFloat16.h"
#include "chGraphicsTypes.h"
#include "chLinearColor.h"
#include "chLogger.h"
#include "chMath.h"
#include "chQuaternion.h"
#include "chRay.h"
#include "chRenderFrame.h"
#include "chRenderSettings.h"
#include "chRotator.h"
#include "chScene.h"
#include "chSpatialHash2D.h"
#include "chSpriteBatcher.h"
#include "chSpriteComponent.h"
#include "chSpriteDraw.h"
#include "chSpriteShader.h"
#include "chUUID.h"
#include "chVector2.h"
#include "chVector3.h"

#include "catch.hpp"
#include "chSpriteTestHelpers.h"
#include "chModelTestHelpers.h"

using namespace chEngineSDK;

CH_LOG_DECLARE_STATIC(SpriteTestLog, All);

namespace {

NODISCARD SpriteItem
makeTestSprite(const Vector3& center,
               int16 sortingLayer = 0,
               int16 order = 0,
               SpriteBlendMode blendMode = SpriteBlendMode::Alpha)
{
  SpriteItem sprite;
  sprite.instance.center = center;
  sprite.instance.halfAxisU = Vector3(0.0f, 0.5f, 0.0f);
  sprite.instance.halfAxisV = Vector3(0.0f, 0.0f, 0.5f);
  sprite.radius = 1.0f;
  sprite.sortingLayer = sortingLayer;
  sprite.order = order;
  sprite.blendMode = blendMode;
  sprite.bInUse = true;
  return sprite;
}
} // namespace

TEST_CASE("chCore - Sprite component", "[Sprites]")
{
  Scene scene("Test", chEngineSDK::UUID::createRandom());
  const SPtr<GameObject> gameObject = scene.createGameObject("Sprite");
  gameObject->getTransform().setLocalPosition(Vector3(0.0f, 2.0f, 3.0f));
  SpriteComponent& sprite = gameObject->addComponent<SpriteComponent>();
  scene.updateTransforms();
  REQUIRE(scene.getSpriteCount() == 1);

  const auto getItem = [&scene]() -> const SpriteItem& {
    for (const SpriteItem& item : scene.getSpriteSlots()) {
      if (item.bInUse) {
        return item;
      }
    }
    FAIL("no sprite in the scene");
    return scene.getSpriteSlots()[0];
  };

  SECTION("Without a texture it is one unit square, centered on its pivot")
  {
    const SpriteItem& item = getItem();
    REQUIRE(item.owner == gameObject.get());
    REQUIRE(item.instance.center.nearEqual(Vector3(0.0f, 2.0f, 3.0f)));
    REQUIRE(item.instance.halfAxisU.nearEqual(Vector3(0.0f, 0.5f, 0.0f)));
    REQUIRE(item.instance.halfAxisV.nearEqual(Vector3(0.0f, 0.0f, 0.5f)));
    REQUIRE(item.radius == Approx(1.0f));
    // Point filtering is the default, for pixel art.
    REQUIRE(item.instance.textureAndFlags ==
            (GpuSpriteInstance::NO_TEXTURE | GpuSpriteInstance::POINT_FILTER_BIT));
    REQUIRE(item.instance.uvRect[0] == 0.0f);
    REQUIRE(item.instance.uvRect[2] == 1.0f);

    sprite.setFilter(SpriteFilter::Linear);
    REQUIRE(getItem().instance.textureAndFlags == GpuSpriteInstance::NO_TEXTURE);
  }

  SECTION("Size comes from the source pixels, the pixels per unit and the scale")
  {
    sprite.setSourceRect(Vector2::ZERO, Vector2(200.0f, 100.0f));
    REQUIRE(sprite.getSize().x == Approx(2.0f));
    REQUIRE(sprite.getSize().y == Approx(1.0f));
    sprite.setPixelsPerUnit(50.0f);
    REQUIRE(sprite.getSize().x == Approx(4.0f));
    sprite.setPixelsPerUnit(0.0f);
    REQUIRE(sprite.getPixelsPerUnit() == SpriteComponent::MIN_PIXELS_PER_UNIT);
    sprite.setPixelsPerUnit(100.0f);

    // The bottom left corner stands at the object.
    sprite.setPivot(Vector2(0.0f, 0.0f));
    REQUIRE(getItem().instance.center.nearEqual(Vector3(0.0f, 3.0f, 3.5f)));

    gameObject->getTransform().setLocalScale(Vector3(1.0f, 3.0f, 2.0f));
    scene.updateTransforms();
    const SpriteItem& item = getItem();
    REQUIRE(item.instance.halfAxisU.nearEqual(Vector3(0.0f, 3.0f, 0.0f)));
    REQUIRE(item.instance.halfAxisV.nearEqual(Vector3(0.0f, 0.0f, 1.0f)));
    REQUIRE(item.instance.center.nearEqual(Vector3(0.0f, 5.0f, 4.0f)));

    AABox bounds(Vector3::ZERO, Vector3::ZERO);
    REQUIRE(sprite.getWorldBounds(bounds));
    REQUIRE(bounds.minPoint.nearEqual(Vector3(0.0f, 2.0f, 3.0f)));
    REQUIRE(bounds.maxPoint.nearEqual(Vector3(0.0f, 8.0f, 5.0f)));
  }

  SECTION("It turns with its object")
  {
    // Positive roll turns right down, so -90 turns the right side up.
    gameObject->getTransform().setLocalRotation(Quaternion(Rotator(0.0f, 0.0f, -90.0f)));
    scene.updateTransforms();
    const SpriteItem& item = getItem();
    REQUIRE(item.instance.halfAxisU.nearEqual(Vector3(0.0f, 0.0f, 0.5f), 1e-5f));
    REQUIRE(item.instance.halfAxisV.nearEqual(Vector3(0.0f, -0.5f, 0.0f), 1e-5f));
  }

  SECTION("Color, flip, sorting and blend reach the item")
  {
    sprite.setColor(LinearColor(1.0f, 0.5f, 0.25f, 0.75f));
    sprite.setFlip(true, true);
    sprite.setSorting(-2, 7);
    sprite.setBlendMode(SpriteBlendMode::Additive);
    const SpriteItem& item = getItem();
    REQUIRE(Float16::toFloat(static_cast<uint16>(item.instance.colorRG & 0xFFFF)) == 1.0f);
    REQUIRE(Float16::toFloat(static_cast<uint16>(item.instance.colorRG >> 16)) == 0.5f);
    REQUIRE(Float16::toFloat(static_cast<uint16>(item.instance.colorBA & 0xFFFF)) == 0.25f);
    REQUIRE(Float16::toFloat(static_cast<uint16>(item.instance.colorBA >> 16)) == 0.75f);
    // Flipped rectangles start at their other edge and run backwards.
    REQUIRE(item.instance.uvRect[0] == 1.0f);
    REQUIRE(item.instance.uvRect[1] == 1.0f);
    REQUIRE(item.instance.uvRect[2] == -1.0f);
    REQUIRE(item.instance.uvRect[3] == -1.0f);
    REQUIRE(item.sortingLayer == -2);
    REQUIRE(item.order == 7);
    REQUIRE(item.blendMode == SpriteBlendMode::Additive);
  }

  SECTION("A ray picks it inside its quad only")
  {
    SceneRayHit hit;
    REQUIRE(scene.raycast(Ray(Vector3(-10.0f, 2.2f, 3.4f), Vector3::FORWARD), hit));
    REQUIRE(hit.gameObject == gameObject.get());
    REQUIRE(hit.item == nullptr);
    REQUIRE(hit.distance == Approx(10.0f));
    REQUIRE(hit.normal.nearEqual(-Vector3::FORWARD));

    REQUIRE_FALSE(scene.raycast(Ray(Vector3(-10.0f, 2.6f, 3.0f), Vector3::FORWARD), hit));
    REQUIRE_FALSE(
        scene.raycast(Ray(Vector3(-10.0f, 2.0f, 3.0f), Vector3::FORWARD), hit, 5.0f));
    REQUIRE_FALSE(scene.raycast(Ray(Vector3(-10.0f, 2.0f, 3.0f), Vector3::FORWARD), hit,
                                Math::MAX_FLOAT, gameObject.get()));
    // Seen from behind it is still there.
    REQUIRE(scene.raycast(Ray(Vector3(10.0f, 2.0f, 3.0f), -Vector3::FORWARD), hit));
    REQUIRE(hit.normal.nearEqual(Vector3::FORWARD));
  }

  SECTION("The render frame copies the sprites in use")
  {
    const SPtr<GameObject> other = scene.createGameObject("Other");
    SpriteComponent& otherSprite = other->addComponent<SpriteComponent>();
    otherSprite.setEnabled(false);
    REQUIRE(scene.getSpriteCount() == 1);
    otherSprite.setEnabled(true);
    sprite.setEnabled(false);
    scene.updateTransforms();

    RenderFrame frame;
    frame.extract(scene, createSpriteTestCamera(), LinearColor::BLACK);
    REQUIRE(frame.getSprites().size() == 1);
    REQUIRE(frame.getSprites()[0].owner == other.get());
  }

  SECTION("Destroying the object removes the sprite")
  {
    scene.destroyGameObject(*gameObject);
    REQUIRE(scene.getSpriteCount() == 0);
  }

  SECTION("Values are saved, the texture id too when it is missing")
  {
    const chEngineSDK::UUID textureId = chEngineSDK::UUID::createRandom();
    sprite.setSourceRect(Vector2(16.0f, 32.0f), Vector2(64.0f, 48.0f));
    sprite.setPixelsPerUnit(32.0f);
    sprite.setPivot(Vector2(0.25f, 1.0f));
    sprite.setColor(LinearColor(0.1f, 0.2f, 0.3f, 0.4f));
    sprite.setFlip(true, false);
    sprite.setSorting(3, -4);
    sprite.setBlendMode(SpriteBlendMode::Additive);
    sprite.setFilter(SpriteFilter::Linear);

    MemoryDataStream stream;
    sprite.serialize(stream);
    Vector<uint8> bytes = readAllBytes(stream);
    // The texture id is the first thing after the version; a random one names no asset.
    std::memcpy(bytes.data() + sizeof(uint32), &textureId, sizeof(textureId));

    MemoryDataStream withTexture;
    withTexture.write(bytes.data(), bytes.size());
    withTexture.seek(0);
    SpriteComponent loaded;
    REQUIRE(loaded.deserialize(withTexture));
    REQUIRE(loaded.getTexture() == nullptr);
    REQUIRE(loaded.getTextureId() == textureId);
    REQUIRE(loaded.getSourcePosition().x == 16.0f);
    REQUIRE(loaded.getSourcePosition().y == 32.0f);
    REQUIRE(loaded.getSourceSize().x == 64.0f);
    REQUIRE(loaded.getSourceSize().y == 48.0f);
    REQUIRE(loaded.getPixelsPerUnit() == 32.0f);
    REQUIRE(loaded.getPivot().x == 0.25f);
    REQUIRE(loaded.getPivot().y == 1.0f);
    REQUIRE(loaded.getColor() == LinearColor(0.1f, 0.2f, 0.3f, 0.4f));
    REQUIRE(loaded.isFlippedX());
    REQUIRE_FALSE(loaded.isFlippedY());
    REQUIRE(loaded.getSortingLayer() == 3);
    REQUIRE(loaded.getOrder() == -4);
    REQUIRE(loaded.getBlendMode() == SpriteBlendMode::Additive);
    REQUIRE(loaded.getFilter() == SpriteFilter::Linear);

    Vector<chEngineSDK::UUID> references;
    loaded.collectReferences(references);
    REQUIRE(references.size() == 1);
    REQUIRE(references[0] == textureId);

    SECTION("A cut file, an unknown blend mode and another version are refused")
    {
      MemoryDataStream cut;
      cut.write(bytes.data(), bytes.size() - 1);
      cut.seek(0);
      SpriteComponent fromCut;
      REQUIRE_FALSE(fromCut.deserialize(cut));

      // The blend mode follows the version, the texture and atlas ids, the frame, 11 floats
      // and two int16.
      Vector<uint8> badBlend = bytes;
      badBlend[sizeof(uint32) + 32 + sizeof(uint32) + 11 * sizeof(float) +
               2 * sizeof(int16)] = 9;
      MemoryDataStream badBlendStream;
      badBlendStream.write(badBlend.data(), badBlend.size());
      badBlendStream.seek(0);
      SpriteComponent fromBadBlend;
      REQUIRE_FALSE(fromBadBlend.deserialize(badBlendStream));

      Vector<uint8> newer = bytes;
      const uint32 newerVersion = SpriteComponent::SERIALIZATION_VERSION + 1;
      std::memcpy(newer.data(), &newerVersion, sizeof(newerVersion));
      MemoryDataStream newerStream;
      newerStream.write(newer.data(), newer.size());
      newerStream.seek(0);
      SpriteComponent fromNewer;
      REQUIRE_FALSE(fromNewer.deserialize(newerStream));
    }
  }

  SECTION("It is a registered component")
  {
    UniquePtr<Component> made =
        ComponentRegister::create(TypeTraits<SpriteComponent>::getTypeId());
    REQUIRE(made != nullptr);
    REQUIRE(StringView(ComponentRegister::getTypeName(
                TypeTraits<SpriteComponent>::getTypeId())) != "Unknown");
  }
}

TEST_CASE("chCore - Sprite batcher", "[Sprites]")
{
  const Camera camera = createSpriteTestCamera();
  SpriteBatcher batcher;

  SECTION("Sort keys go by layer, then order, then the farther depth first")
  {
    REQUIRE(SpriteBatcher::makeSortKey(-5, 100, 0.0f) <
            SpriteBatcher::makeSortKey(0, -100, 0.0f));
    REQUIRE(SpriteBatcher::makeSortKey(0, -1, 0.0f) < SpriteBatcher::makeSortKey(0, 1, 0.0f));
    REQUIRE(SpriteBatcher::makeSortKey(0, 0, 3.0f) < SpriteBatcher::makeSortKey(0, 0, 2.0f));
    REQUIRE(SpriteBatcher::makeSortKey(0, 0, 0.5f) < SpriteBatcher::makeSortKey(0, 0, -0.5f));
    REQUIRE(SpriteBatcher::makeSortKey(0, 0, -2.0f) < SpriteBatcher::makeSortKey(0, 0, -3.0f));
    REQUIRE(SpriteBatcher::makeSortKey(32767, 0, 0.0f) >
            SpriteBatcher::makeSortKey(-32768, 32767, -1e30f));
  }

  SECTION("Sprites outside the view are left out")
  {
    const Vector<SpriteItem> sprites = {makeTestSprite(Vector3(0.0f, 0.0f, 0.0f)),
                                        makeTestSprite(Vector3(0.0f, 50.0f, 0.0f)),
                                        makeTestSprite(Vector3(0.0f, 0.0f, -7.0f)),
                                        makeTestSprite(Vector3(-20.0f, 0.0f, 0.0f))};
    batcher.build(sprites, camera.getFrustum(), camera.getForwardVector());
    REQUIRE(batcher.getInstances().size() == 1);
    REQUIRE(batcher.getRuns().size() == 1);
    REQUIRE(batcher.getRuns()[0].instanceCount == 1);
  }

  SECTION("Instances come out sorted and equal keys keep their order")
  {
    Vector<SpriteItem> sprites;
    sprites.push_back(makeTestSprite(Vector3(0.0f, 1.0f, 0.0f), 1, 0));
    sprites.push_back(makeTestSprite(Vector3(0.0f, 2.0f, 0.0f), 0, 5));
    sprites.push_back(makeTestSprite(Vector3(0.0f, 3.0f, 0.0f), 0, -5));
    sprites.push_back(makeTestSprite(Vector3(0.0f, 4.0f, 0.0f), 1, 0));
    // Same layer and order, farther along the camera's forward axis: drawn first.
    sprites.push_back(makeTestSprite(Vector3(5.0f, -1.0f, 0.0f), 1, 0));
    batcher.build(sprites, camera.getFrustum(), camera.getForwardVector());

    const Span<const GpuSpriteInstance> instances = batcher.getInstances();
    REQUIRE(instances.size() == 5);
    REQUIRE(instances[0].center.y == 3.0f);
    REQUIRE(instances[1].center.y == 2.0f);
    REQUIRE(instances[2].center.y == -1.0f);
    REQUIRE(instances[3].center.y == 1.0f);
    REQUIRE(instances[4].center.y == 4.0f);
  }

  SECTION("Sorting by the up axis puts the higher sprites first")
  {
    const Vector<SpriteItem> sprites = {makeTestSprite(Vector3(0.0f, 0.0f, -2.0f)),
                                        makeTestSprite(Vector3(0.0f, 0.0f, 3.0f)),
                                        makeTestSprite(Vector3(0.0f, 0.0f, 0.5f))};
    batcher.build(sprites, camera.getFrustum(), camera.getUpVector());
    const Span<const GpuSpriteInstance> instances = batcher.getInstances();
    REQUIRE(instances[0].center.z == 3.0f);
    REQUIRE(instances[1].center.z == 0.5f);
    REQUIRE(instances[2].center.z == -2.0f);
  }

  SECTION("A run ends where the blend mode changes, never where the texture does")
  {
    Vector<SpriteItem> sprites;
    for (int16 order = 0; order < 6; ++order) {
      const SpriteBlendMode blend =
          order == 2 || order == 3 ? SpriteBlendMode::Additive : SpriteBlendMode::Alpha;
      SpriteItem sprite = makeTestSprite(Vector3::ZERO, 0, order, blend);
      sprite.instance.textureAndFlags = static_cast<uint32>(order);
      sprites.push_back(sprite);
    }
    batcher.build(sprites, camera.getFrustum(), camera.getForwardVector());
    const Span<const SpriteRun> runs = batcher.getRuns();
    REQUIRE(runs.size() == 3);
    REQUIRE(runs[0].blendMode == SpriteBlendMode::Alpha);
    REQUIRE(runs[0].firstInstance == 0);
    REQUIRE(runs[0].instanceCount == 2);
    REQUIRE(runs[1].blendMode == SpriteBlendMode::Additive);
    REQUIRE(runs[1].firstInstance == 2);
    REQUIRE(runs[1].instanceCount == 2);
    REQUIRE(runs[2].firstInstance == 4);
    REQUIRE(runs[2].instanceCount == 2);
    REQUIRE(batcher.getInstances()[5].textureAndFlags == 5);
  }

  SECTION("Building again replaces the last frame")
  {
    const Vector<SpriteItem> many(100, makeTestSprite(Vector3::ZERO));
    batcher.build(many, camera.getFrustum(), camera.getForwardVector());
    REQUIRE(batcher.getInstances().size() == 100);
    batcher.build({}, camera.getFrustum(), camera.getForwardVector());
    REQUIRE(batcher.getInstances().empty());
    REQUIRE(batcher.getRuns().empty());
  }
}

/*
 * Hidden: run it by name in a Release build,
 *   chCoreTest "chCore - Sprite batch benchmark"
 * Culls, sorts and lays out 100000 sprites spread over four layers, the last one additive
 * (glows), like a busy 2D frame.
 */
TEST_CASE("chCore - Sprite batch benchmark", "[Sprites][.benchmark]")
{
  constexpr uint32 kSpriteCount = 100000;
  constexpr uint32 kFrames = 100;
  const Camera camera = createSpriteTestCamera();

  Vector<SpriteItem> sprites;
  sprites.reserve(kSpriteCount);
  for (uint32 i = 0; i < kSpriteCount; ++i) {
    const float angle = static_cast<float>(i) * 0.37f;
    const float distance = static_cast<float>(i % 997) * 0.01f;
    SpriteItem sprite =
        makeTestSprite(Vector3(0.0f, Math::cos(Radian(angle)) * distance * 0.8f,
                               Math::sin(Radian(angle)) * distance * 0.45f),
                       static_cast<int16>(i % 4), static_cast<int16>(i % 7),
                       i % 4 == 3 ? SpriteBlendMode::Additive : SpriteBlendMode::Alpha);
    sprite.radius = 0.1f;
    sprites.push_back(sprite);
  }

  SpriteBatcher batcher;
  for (const bool bYSort : {false, true}) {
    const Vector3 axis = bYSort ? camera.getUpVector() : camera.getForwardVector();
    batcher.build(sprites, camera.getFrustum(), axis);
    const auto start = std::chrono::steady_clock::now();
    for (uint32 frame = 0; frame < kFrames; ++frame) {
      batcher.build(sprites, camera.getFrustum(), axis);
    }
    const auto end = std::chrono::steady_clock::now();
    const double milliseconds =
        std::chrono::duration<double, std::milli>(end - start).count() / kFrames;
    CH_LOG_INFO(SpriteTestLog, "{} sprites ({}): {:.3f} ms per frame, {} instances, {} runs",
                kSpriteCount, bYSort ? "y sort" : "depth sort", milliseconds,
                static_cast<uint32>(batcher.getInstances().size()),
                static_cast<uint32>(batcher.getRuns().size()));
    REQUIRE(batcher.getInstances().size() == kSpriteCount);
  }
}

TEST_CASE("chCore - Sprite draw", "[Sprites]")
{
  SpriteDraw spriteDraw;

  SECTION("A quad turns counterclockwise and stands on its pivot")
  {
    spriteDraw.quad({.position = Vector2(1.0f, 2.0f),
                     .depth = 4.0f,
                     .size = Vector2(4.0f, 2.0f),
                     .rotation = Radian(Degree(90.0f)),
                     .pivot = Vector2(0.0f, 0.0f),
                     .color = LinearColor(0.0f, 1.0f, 0.0f, 0.5f),
                     .sortingLayer = 3,
                     .order = -1,
                     .blendMode = SpriteBlendMode::Additive});
    REQUIRE(spriteDraw.getSprites().size() == 1);
    const SpriteItem& sprite = spriteDraw.getSprites()[0];
    REQUIRE(sprite.bInUse);
    REQUIRE(sprite.owner == nullptr);
    // The right side now points up and the top points left.
    REQUIRE(sprite.instance.halfAxisU.nearEqual(Vector3(0.0f, 0.0f, 2.0f), 1e-5f));
    REQUIRE(sprite.instance.halfAxisV.nearEqual(Vector3(0.0f, -1.0f, 0.0f), 1e-5f));
    REQUIRE(sprite.instance.center.nearEqual(Vector3(4.0f, 0.0f, 4.0f), 1e-5f));
    REQUIRE(sprite.radius == Approx(3.0f));
    REQUIRE(sprite.sortingLayer == 3);
    REQUIRE(sprite.order == -1);
    REQUIRE(sprite.blendMode == SpriteBlendMode::Additive);
    REQUIRE(Float16::toFloat(static_cast<uint16>(sprite.instance.colorRG >> 16)) == 1.0f);
    REQUIRE(Float16::toFloat(static_cast<uint16>(sprite.instance.colorBA >> 16)) == 0.5f);
    REQUIRE(sprite.instance.textureAndFlags == GpuSpriteInstance::NO_TEXTURE);
  }

  SECTION("Instances are taken as they are")
  {
    GpuSpriteInstance instance{};
    instance.center = Vector3(0.0f, 7.0f, 0.0f);
    spriteDraw.instance(instance, 2.0f, 1, 2, SpriteBlendMode::Alpha);
    REQUIRE(spriteDraw.getSprites()[0].instance.center.y == 7.0f);
    REQUIRE(spriteDraw.getSprites()[0].radius == 2.0f);
    spriteDraw.clear();
    REQUIRE(spriteDraw.getSprites().empty());
  }

  SECTION("Above the limit the rest are dropped")
  {
    const int32 limit = g_cvarSpriteDrawMax.get();
    REQUIRE(g_cvarSpriteDrawMax.set(3));
    for (uint32 i = 0; i < 5; ++i) {
      spriteDraw.quad({});
    }
    REQUIRE(spriteDraw.getSprites().size() == 3);
    REQUIRE(g_cvarSpriteDrawMax.set(limit));
  }

  SECTION("Texture parts and flips")
  {
    float uvRect[4];
    SpriteDraw::makeUvRect(nullptr, Vector2(4.0f, 4.0f), Vector2(8.0f, 8.0f), false, true,
                           uvRect);
    // Without a texture size the whole image is shown, flipped as asked.
    REQUIRE(uvRect[0] == 0.0f);
    REQUIRE(uvRect[1] == 1.0f);
    REQUIRE(uvRect[2] == 1.0f);
    REQUIRE(uvRect[3] == -1.0f);
    REQUIRE(SpriteDraw::makeTextureFlags(nullptr, SpriteFilter::Point) ==
            (GpuSpriteInstance::NO_TEXTURE | GpuSpriteInstance::POINT_FILTER_BIT));
  }

  SECTION("Each scene list is cleared by its own update, and the frame has them all")
  {
    Scene scene("Sprites", chEngineSDK::UUID::createRandom());
    const SPtr<GameObject> gameObject = scene.createGameObject("Sprite");
    gameObject->addComponent<SpriteComponent>();
    scene.updateTransforms();
    scene.getSpriteDraw().quad({.position = Vector2(1.0f, 0.0f)});
    scene.getFixedSpriteDraw().quad({.position = Vector2(2.0f, 0.0f)});

    RenderFrame frame;
    frame.extract(scene, createSpriteTestCamera(), LinearColor::BLACK);
    REQUIRE(frame.getSprites().size() == 3);
    REQUIRE(frame.getSprites()[0].owner == gameObject.get());
    REQUIRE(frame.getSprites()[1].instance.center.y == 1.0f);
    REQUIRE(frame.getSprites()[2].instance.center.y == 2.0f);

    scene.update(0.016f);
    REQUIRE(scene.getSpriteDraw().getSprites().empty());
    REQUIRE(scene.getFixedSpriteDraw().getSprites().size() == 1);
    scene.fixedUpdate(1.0f / 60.0f);
    REQUIRE(scene.getFixedSpriteDraw().getSprites().empty());
  }
}

/*
 * Hidden: run it by name in a Release build,
 *   chCoreTest "chCore - Bullet benchmark"
 * A bullet hell frame done the way a C++ system (or plugin) would: 20000 bullets move, go
 * into a spatial hash, 200 targets ask what touches them, and every bullet is drawn with
 * SpriteDraw; then the sprites are sorted as the renderer does.
 */
TEST_CASE("chCore - Bullet benchmark", "[Sprites][.benchmark]")
{
  constexpr uint32 kBulletCount = 20000;
  constexpr uint32 kTargetCount = 200;
  constexpr uint32 kFrames = 100;
  constexpr float kDeltaTime = 1.0f / 60.0f;
  const Camera camera = createSpriteTestCamera();

  Vector<Vector2> positions(kBulletCount);
  Vector<Vector2> velocities(kBulletCount);
  for (uint32 i = 0; i < kBulletCount; ++i) {
    const float angle = static_cast<float>(i) * 0.61f;
    positions[i] = Vector2(Math::cos(Radian(angle)) * 0.5f, Math::sin(Radian(angle)) * 0.5f);
    velocities[i] = Vector2(Math::cos(Radian(angle)) * 2.0f, Math::sin(Radian(angle)) * 2.0f);
  }
  Vector<Vector2> targets(kTargetCount);
  for (uint32 i = 0; i < kTargetCount; ++i) {
    targets[i] = Vector2(static_cast<float>(i % 20) * 0.8f - 8.0f,
                         static_cast<float>(i / 20) * 0.9f - 4.5f);
  }

  SpriteDraw spriteDraw;
  SpatialHash2D hash(0.5f);
  Vector<uint32> touching;
  SpriteBatcher batcher;
  uint64 hits = 0;
  const auto runFrame = [&]() {
    spriteDraw.clear();
    hash.clear();
    for (uint32 i = 0; i < kBulletCount; ++i) {
      Vector2& position = positions[i];
      position += velocities[i] * kDeltaTime;
      // Bullets that leave the screen come back in at the center.
      if (Math::abs(position.x) > 9.0f || Math::abs(position.y) > 5.0f) {
        position = Vector2::ZERO;
      }
      hash.addCircle(position, 0.05f, i);
      spriteDraw.quad({.position = position,
                       .size = Vector2(0.1f, 0.1f),
                       .sortingLayer = 2,
                       .blendMode = SpriteBlendMode::Additive});
    }
    hash.build();
    for (const Vector2& target : targets) {
      hash.queryCircle(target, 0.3f, touching);
      hits += touching.size();
    }
    batcher.build(spriteDraw.getSprites(), camera.getFrustum(), camera.getForwardVector());
  };

  runFrame();
  const auto start = std::chrono::steady_clock::now();
  for (uint32 frame = 0; frame < kFrames; ++frame) {
    runFrame();
  }
  const auto end = std::chrono::steady_clock::now();
  const double milliseconds =
      std::chrono::duration<double, std::milli>(end - start).count() / kFrames;
  CH_LOG_INFO(SpriteTestLog,
              "{} bullets, {} targets: {:.3f} ms per frame (move, hash, queries, draw, sort), "
              "{} draws, {} hits",
              kBulletCount, kTargetCount, milliseconds,
              static_cast<uint32>(batcher.getRuns().size()), hits);
  REQUIRE(batcher.getInstances().size() == kBulletCount);
}

TEST_CASE("chCore - Custom sprite batches", "[Sprites]")
{
  struct Parameters
  {
    uint32 bufferIndex;
    float time;
  };

  SECTION("Parameters are copied as bytes")
  {
    CustomSpriteBatch batch;
    batch.setParameters(Parameters{.bufferIndex = 0x01020304u, .time = 2.5f});
    uint32 bufferIndex = 0;
    float time = 0.0f;
    std::memcpy(&bufferIndex, batch.parameters.data(), sizeof(bufferIndex));
    std::memcpy(&time, batch.parameters.data() + 4, sizeof(time));
    REQUIRE(bufferIndex == 0x01020304u);
    REQUIRE(time == 2.5f);
    REQUIRE(batch.parameters[8] == 0);
  }

  SECTION("SpriteDraw keeps only batches that draw something")
  {
    SpriteShader shader(Path("Nowhere"), "none");
    SpriteDraw draw;
    draw.customBatch({.shader = &shader, .instanceCount = 3});
    draw.customBatch({.shader = &shader, .instanceCount = 0});
    draw.customBatch({.shader = nullptr, .instanceCount = 3});
    draw.customBatch({.shader = &shader, .vertexCount = 0, .instanceCount = 3});
    REQUIRE(draw.getCustomBatches().size() == 1);
    draw.clear();
    REQUIRE(draw.getCustomBatches().empty());
  }

  SECTION("They sort with the sprites, after batches of equal keys")
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
    const Vector<SpriteItem> sprites{spriteAt(0), spriteAt(2)};
    const Vector<GlyphRunBatch> batches{{.bufferIndex = 4, .runCount = 2, .sortingLayer = 1}};
    SpriteShader shader(Path("Nowhere"), "none");
    const Vector<CustomSpriteBatch> customBatches{
        {.shader = &shader, .vertexCount = 12, .instanceCount = 5, .sortingLayer = 1},
        {.shader = &shader,
         .instanceCount = 7,
         .sortingLayer = 3,
         .blendMode = SpriteBlendMode::Additive}};

    SpriteBatcher batcher;
    batcher.build(
        sprites, camera.getFrustum(), camera.getForwardVector(), {}, batches, customBatches);
    const Span<const SpriteRun> order = batcher.getRuns();
    REQUIRE(order.size() == 5);
    REQUIRE(order[0].kind == SpriteRunKind::Sprites);
    REQUIRE(order[1].kind == SpriteRunKind::GlyphRunBatch);
    REQUIRE(order[2].kind == SpriteRunKind::Custom);
    REQUIRE(order[2].bufferIndex == 0);
    REQUIRE(order[2].instanceCount == 5);
    REQUIRE(order[3].kind == SpriteRunKind::Sprites);
    REQUIRE(order[4].kind == SpriteRunKind::Custom);
    REQUIRE(order[4].bufferIndex == 1);
    REQUIRE(order[4].blendMode == SpriteBlendMode::Additive);
  }

  SECTION("Plugin shaders live next to the plugins")
  {
    const Path directory = EnginePaths::getEnginePluginShaderDirectory("chSomething");
    REQUIRE(directory ==
            EnginePaths::getEnginePluginDirectory().join(Path("Shaders/chSomething")));
  }
}
