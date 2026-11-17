/************************************************************************/
/**
 * @file chSpriteMaterialTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of sprite materials and how sprites carry them to the renderer.
 */
/************************************************************************/
#include <cstring>

#include "chCamera.h"
#include "chFileStream.h"
#include "chGameObject.h"
#include "chScene.h"
#include "chShaderParameters.h"
#include "chSpriteBatcher.h"
#include "chSpriteComponent.h"
#include "chSpriteDraw.h"
#include "chSpriteMaterialAsset.h"

#include "catch.hpp"
#include "chPostProcessTestHelpers.h"
#include "chSpriteTestHelpers.h"

using namespace chEngineSDK;

namespace {

class TestSpriteMaterialAsset : public SpriteMaterialAsset
{
 public:
  using SpriteMaterialAsset::SpriteMaterialAsset;
  using SpriteMaterialAsset::collectReferences;
  using SpriteMaterialAsset::deserialize;
  using SpriteMaterialAsset::serialize;
};

NODISCARD SPtr<ShaderAsset>
makeSpriteShader(const ANSICHAR* name)
{
  return makeTestPostProcessShader(name, PostProcessStage::AfterTonemap,
                                   ShaderAssetKind::Sprite);
}

NODISCARD SPtr<TestSpriteMaterialAsset>
makeMaterial(const ANSICHAR* name)
{
  AssetMetadata metadata;
  metadata.uuid = chEngineSDK::UUID::createRandom();
  metadata.assetType = AssetTypeTraits<SpriteMaterialAsset>::getTypeId();
  StringUtils::copyToBuffer(metadata.name, name);
  return chMakeShared<TestSpriteMaterialAsset>(metadata);
}

NODISCARD SpriteItem
makeSprite(float y, const SpriteMaterialAsset* material)
{
  SpriteItem sprite;
  sprite.instance.center = Vector3(0.0f, y, 0.0f);
  sprite.instance.halfAxisU = Vector3(0.0f, 0.5f, 0.0f);
  sprite.instance.halfAxisV = Vector3(0.0f, 0.0f, 0.5f);
  sprite.radius = 1.0f;
  sprite.material = material;
  sprite.bInUse = true;
  return sprite;
}

NODISCARD float
readFloat(Span<const uint8> bytes, uint32 offset)
{
  float value = 0.0f;
  std::memcpy(&value, bytes.data() + offset, sizeof(value));
  return value;
}

} // namespace

TEST_CASE("chCore - Sprite material", "[Sprites]")
{
  const SPtr<ShaderAsset> shader = makeSpriteShader("SM_Flash");
  const SPtr<TestSpriteMaterialAsset> material = makeMaterial("M_Flash");

  SECTION("Only sprite shaders are taken")
  {
    REQUIRE_FALSE(material->canDraw());
    REQUIRE(material->setShader(shader));
    REQUIRE(material->getShaderId() == shader->getUUID());
    REQUIRE(material->canDraw());
    // Made in memory, so not marked loaded: the kind is checked once it is.
    REQUIRE(material->setShader(makeTestPostProcessShader("PP_Other")));
    REQUIRE_FALSE(material->canDraw());
    REQUIRE(material->setShader(nullptr));
    REQUIRE(material->getShaderId().isNull());
  }

  SECTION("Values build the bytes the shader reads")
  {
    REQUIRE(material->setShader(shader));
    REQUIRE(readFloat(material->getParameterData(), 0) == 1.0f);
    REQUIRE(material->setValue("intensity", Vector4(0.25f, 0.0f, 0.0f, 0.0f)));
    REQUIRE_FALSE(material->setValue("missing", Vector4::ZERO));
    REQUIRE(readFloat(material->getParameterData(), 0) == 0.25f);
    REQUIRE(material->getParameterData().size() == shader->getParameterBufferSize());

    Vector4 value;
    REQUIRE(material->getValue("tint", value));
    REQUIRE(value == Vector4(1.0f, 1.0f, 1.0f, 1.0f));
    material->resetValue("intensity");
    REQUIRE_FALSE(material->findOverride("intensity"));
    REQUIRE(readFloat(material->getParameterData(), 0) == 1.0f);

#if USING(CH_EDITOR)
    // A new compile moves the parameter; the bytes follow it.
    REQUIRE(material->setValue("intensity", Vector4(0.5f, 0.0f, 0.0f, 0.0f)));
    REQUIRE(readFloat(material->getParameterData(), 0) == 0.5f);
    ShaderAssetData data = shader->getData();
    std::swap(data.parameters[0], data.parameters[1]);
    ShaderParameters::computeLayout(data.parameters);
    shader->setData(std::move(data));
    REQUIRE(readFloat(material->getParameterData(), 16) == 0.5f);
#endif // USING(CH_EDITOR)
  }

  SECTION("Saved and read back, with the shader kept by id")
  {
    REQUIRE(material->setShader(shader));
    REQUIRE(material->setValue("tint", Vector4(1.0f, 0.0f, 0.0f, 1.0f)));
    const SPtr<MemoryDataStream> stream = chMakeShared<MemoryDataStream>();
    REQUIRE(material->serialize(stream));
    const SIZE_T size = stream->tell();

    stream->seek(0);
    const SPtr<TestSpriteMaterialAsset> read = makeMaterial("M_Read");
    REQUIRE(read->deserialize(stream));
    // Without an asset manager the shader is not found, but its id stays.
    REQUIRE_FALSE(read->getShader());
    REQUIRE(read->getShaderId() == shader->getUUID());
    REQUIRE(read->getOverrides().size() == 1);
    REQUIRE(read->getOverrides()[0].value == Vector4(1.0f, 0.0f, 0.0f, 1.0f));
    Vector<chEngineSDK::UUID> references;
    read->collectReferences(references);
    REQUIRE(references.size() == 1);

    for (const SIZE_T cut : {SIZE_T(4), size - 1}) {
      stream->seek(0);
      Vector<uint8> bytes(cut);
      REQUIRE(stream->read(bytes.data(), bytes.size()) == bytes.size());
      const SPtr<MemoryDataStream> part = chMakeShared<MemoryDataStream>();
      REQUIRE(part->write(bytes.data(), bytes.size()) == bytes.size());
      part->seek(0);
      REQUIRE_FALSE(makeMaterial("M_Cut")->deserialize(part));
    }
  }
}

TEST_CASE("chCore - Sprite material batching", "[Sprites]")
{
  const Camera camera = createSpriteTestCamera();
  const SPtr<TestSpriteMaterialAsset> flash = makeMaterial("M_Flash");
  const SPtr<TestSpriteMaterialAsset> dissolve = makeMaterial("M_Dissolve");
  SpriteBatcher batcher;

  SECTION("Sprites next to each other with one material draw together")
  {
    // Equal keys keep their order: plain, flash, flash, dissolve, plain.
    const Vector<SpriteItem> sprites = {makeSprite(0.0f, nullptr), makeSprite(0.1f, flash.get()),
                                        makeSprite(0.2f, flash.get()),
                                        makeSprite(0.3f, dissolve.get()),
                                        makeSprite(0.4f, nullptr)};
    batcher.build(sprites, camera.getFrustum(), camera.getForwardVector());
    const Span<const SpriteRun> runs = batcher.getRuns();
    REQUIRE(runs.size() == 4);
    REQUIRE(runs[0].material == nullptr);
    REQUIRE(runs[1].material == flash.get());
    REQUIRE(runs[1].instanceCount == 2);
    REQUIRE(runs[2].material == dissolve.get());
    REQUIRE(runs[3].material == nullptr);
  }

  SECTION("Sprite components and drawn sprites carry their material")
  {
    Scene scene("Sprites", chEngineSDK::UUID::createRandom());
    const SPtr<GameObject> owner = scene.createGameObject("Enemy");
    SpriteComponent& sprite = owner->addComponent<SpriteComponent>();
    sprite.setMaterial(flash);
    REQUIRE(sprite.getMaterialId() == flash->getUUID());
    const SpriteItem* item = nullptr;
    for (const SpriteItem& slot : scene.getSpriteSlots()) {
      if (slot.bInUse) {
        item = &slot;
      }
    }
    REQUIRE(item);
    REQUIRE(item->material == flash.get());

    // The material id is saved; without an asset manager the asset is not found again.
    MemoryDataStream stream;
    sprite.serialize(stream);
    stream.seek(0);
    SpriteComponent read;
    REQUIRE(read.deserialize(stream));
    REQUIRE(read.getMaterialId() == flash->getUUID());
    REQUIRE_FALSE(read.getMaterial());
    Vector<chEngineSDK::UUID> references;
    read.collectReferences(references);
    REQUIRE(references.size() == 1);

    sprite.setMaterial(nullptr);
    REQUIRE(item->material == nullptr);

    SpriteDraw draw;
    draw.quad({.material = dissolve.get()});
    draw.instance(GpuSpriteInstance{}, 1.0f, 0, 0, SpriteBlendMode::Alpha, flash.get());
    REQUIRE(draw.getSprites().size() == 2);
    REQUIRE(draw.getSprites()[0].material == dissolve.get());
    REQUIRE(draw.getSprites()[1].material == flash.get());
  }
}

#if USING(CH_EDITOR)
TEST_CASE("chCore - Shader kinds", "[Graphics]")
{
  ShaderParameters::SourceInfo info;
  String error;
  REQUIRE(ShaderParameters::parseSource("// CH_SHADER Sprite\n// CH_PARAM float amount 1\n",
                                        info, error));
  REQUIRE(info.kind == ShaderAssetKind::Sprite);
  REQUIRE(ShaderParameters::parseSource("// CH_PARAM float amount 1\n", info, error));
  REQUIRE(info.kind == ShaderAssetKind::PostProcess);

  const StringView broken[] = {
      "// CH_SHADER Mesh\n",
      "// CH_SHADER Sprite\n// CH_SHADER Sprite\n",
      "// CH_SHADER Sprite\n// CH_STAGE BeforeTonemap\n",
  };
  for (const StringView source : broken) {
    CAPTURE(source);
    REQUIRE_FALSE(ShaderParameters::parseSource(source, info, error));
  }
  REQUIRE(ShaderParameters::getKindName(ShaderAssetKind::Sprite) == "Sprite");
}
#endif // USING(CH_EDITOR)
