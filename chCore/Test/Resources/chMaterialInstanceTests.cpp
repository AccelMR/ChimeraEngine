/************************************************************************/
/**
 * @file chMaterialInstanceTests.cpp
 * @author AccelMR
 * @date 2026/11/27
 * @brief Tests of named material values, material instances and dynamic materials.
 */
/************************************************************************/
#include <limits>

#include "chAssetFile.h"
#include "chDynamicMaterial.h"
#include "chFileStream.h"
#include "chGameObject.h"
#include "chLinearColor.h"
#include "chMaterial.h"
#include "chMaterialAsset.h"
#include "chMaterialInstanceAsset.h"
#include "chMaterialOverrides.h"
#include "chScene.h"
#include "chShapeComponent.h"
#include "chStringUtils.h"
#include "chUUID.h"
#include "chVector4.h"

#include "catch.hpp"
#include "chModelTestHelpers.h"

using namespace chEngineSDK;

namespace {
// Opens the protected asset functions to the tests.
class TestInstanceAsset : public MaterialInstanceAsset
{
 public:
  using MaterialInstanceAsset::clearAssetData;
  using MaterialInstanceAsset::collectReferences;
  using MaterialInstanceAsset::deserialize;
  using MaterialInstanceAsset::MaterialInstanceAsset;
  using MaterialInstanceAsset::serialize;
};

NODISCARD SPtr<TestInstanceAsset>
createTestInstance(StringView name, const SPtr<MaterialInterfaceAsset>& parent = nullptr)
{
  AssetMetadata metadata;
  metadata.uuid = chEngineSDK::UUID::createRandom();
  metadata.assetType = AssetTypeTraits<MaterialInstanceAsset>::getTypeId();
  StringUtils::copyToBuffer(metadata.name, name);
  SPtr<TestInstanceAsset> instance = chMakeShared<TestInstanceAsset>(metadata);
  if (parent) {
    REQUIRE(instance->setParent(parent));
  }
  return instance;
}

NODISCARD float
getFloat(const Material& material, StringView name)
{
  Vector4 value;
  REQUIRE(material.getValue(name, value));
  return value.x;
}

NODISCARD const Material*
getShapeRenderMaterial(const Scene& scene, const GameObject& owner)
{
  for (const RenderItem& item : scene.getRenderItemSlots()) {
    if (item.mesh && item.owner == &owner) {
      return item.material;
    }
  }
  FAIL("The object has no render item.");
  return nullptr;
}
} // namespace

TEST_CASE("chCore - Material named values", "[Resources]")
{
  Material material;
  REQUIRE(Material::getParameters().size() == 9);
  for (const MaterialParameterInfo& info : Material::getParameters()) {
    REQUIRE(Material::findParameter(info.name) != Material::NOT_FOUND);
    Vector4 value;
    REQUIRE(material.getValue(info.name, value));
  }

  REQUIRE(material.setValue("baseColor", Vector4(0.25f, 0.5f, 0.75f, 1.0f)));
  REQUIRE(material.getBaseColorFactor() == LinearColor(0.25f, 0.5f, 0.75f, 1.0f));
  REQUIRE(material.setValue("roughness", Vector4(0.125f, 0.0f, 0.0f, 0.0f)));
  REQUIRE(material.getRoughnessFactor() == 0.125f);
  REQUIRE(material.setValue("alphaMode", Vector4(1.0f, 0.0f, 0.0f, 0.0f)));
  REQUIRE(material.getAlphaMode() == MaterialAlphaMode::Mask);
  REQUIRE(material.setValue("twoSided", Vector4(1.0f, 0.0f, 0.0f, 0.0f)));
  REQUIRE(material.isTwoSided());
  REQUIRE(getFloat(material, "alphaMode") == 1.0f);

  SECTION("Bad names and values are refused")
  {
    REQUIRE_FALSE(material.setValue("BaseColor", Vector4::ZERO));
    REQUIRE_FALSE(material.setValue("missing", Vector4::ZERO));
    REQUIRE_FALSE(material.setValue("metallic", Vector4(std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 0.0f)));
    REQUIRE_FALSE(material.setValue("alphaMode", Vector4(2.0f, 0.0f, 0.0f, 0.0f)));
    REQUIRE_FALSE(material.setValue("alphaMode", Vector4(0.5f, 0.0f, 0.0f, 0.0f)));
    REQUIRE_FALSE(material.setValue("alphaMode", Vector4(-1.0f, 0.0f, 0.0f, 0.0f)));
    REQUIRE(material.getAlphaMode() == MaterialAlphaMode::Mask);
    Vector4 value;
    REQUIRE_FALSE(material.getValue("missing", value));
  }

  SECTION("Texture names")
  {
    for (uint32 i = 0; i < static_cast<uint32>(MaterialTexture::COUNT); ++i) {
      const MaterialTexture slot = static_cast<MaterialTexture>(i);
      REQUIRE(Material::findTexture(Material::getTextureName(slot)) == slot);
    }
    REQUIRE(Material::findTexture("missing") == MaterialTexture::COUNT);
  }

  SECTION("Copying and resetting keep nothing of the old values")
  {
    const chEngineSDK::UUID textureId = chEngineSDK::UUID::createRandom();
    material.setTexture(MaterialTexture::Normal, textureId, nullptr);
    Material copy;
    copy.copyValuesFrom(material);
    REQUIRE(copy.getRoughnessFactor() == 0.125f);
    REQUIRE(copy.isTwoSided());
    REQUIRE(copy.getTextureId(MaterialTexture::Normal) == textureId);

    copy.resetValues();
    REQUIRE(copy.getRoughnessFactor() == 0.5f);
    REQUIRE_FALSE(copy.isTwoSided());
    REQUIRE(copy.getAlphaMode() == MaterialAlphaMode::Opaque);
    REQUIRE(copy.getTextureId(MaterialTexture::Normal).isNull());
  }
}

TEST_CASE("chCore - Material instance", "[Resources]")
{
  const SPtr<MaterialAsset> parent = createTestMaterialAsset("M_Parent");
  parent->getMaterial().setBaseColorFactor(LinearColor(1.0f, 0.0f, 0.0f, 1.0f));
  parent->getMaterial().setRoughnessFactor(0.8f);
  parent->notifyMaterialChanged();

  const SPtr<TestInstanceAsset> instance = createTestInstance("MI_Child", parent);
  REQUIRE(instance->getParentId() == parent->getUUID());
  REQUIRE(instance->getParentMaterial() == parent);
  REQUIRE(instance->getMaterial().getBaseColorFactor().r == 1.0f);
  REQUIRE(instance->getMaterial().getRoughnessFactor() == 0.8f);

  REQUIRE(instance->setValue("roughness", Vector4(0.2f, 0.0f, 0.0f, 0.0f)));
  REQUIRE_FALSE(instance->setValue("missing", Vector4::ZERO));
  REQUIRE(instance->getMaterial().getRoughnessFactor() == 0.2f);
  REQUIRE(parent->getMaterial().getRoughnessFactor() == 0.8f);

  SECTION("Parent edits reach what the instance leaves")
  {
    parent->getMaterial().setMetallicFactor(1.0f);
    parent->getMaterial().setRoughnessFactor(0.9f);
    parent->notifyMaterialChanged();
    REQUIRE(instance->getMaterial().getMetallicFactor() == 1.0f);
    REQUIRE(instance->getMaterial().getRoughnessFactor() == 0.2f);

    REQUIRE(instance->resetValue("roughness"));
    REQUIRE_FALSE(instance->resetValue("roughness"));
    REQUIRE(instance->getMaterial().getRoughnessFactor() == 0.9f);
  }

  SECTION("A chain of instances follows the top")
  {
    const SPtr<TestInstanceAsset> grandChild = createTestInstance("MI_GrandChild", instance);
    REQUIRE(grandChild->setValue("metallic", Vector4(0.5f, 0.0f, 0.0f, 0.0f)));
    REQUIRE(grandChild->getMaterial().getRoughnessFactor() == 0.2f);

    parent->getMaterial().setBaseColorFactor(LinearColor(0.0f, 1.0f, 0.0f, 1.0f));
    parent->notifyMaterialChanged();
    REQUIRE(grandChild->getMaterial().getBaseColorFactor().g == 1.0f);
    REQUIRE(grandChild->getMaterial().getMetallicFactor() == 0.5f);

    instance->setValue("roughness", Vector4(0.3f, 0.0f, 0.0f, 0.0f));
    REQUIRE(grandChild->getMaterial().getRoughnessFactor() == 0.3f);
  }

  SECTION("Loops and long chains are refused")
  {
    const SPtr<TestInstanceAsset> grandChild = createTestInstance("MI_GrandChild", instance);
    REQUIRE_FALSE(instance->setParent(grandChild));
    REQUIRE_FALSE(instance->setParent(instance));
    REQUIRE(instance->getParentMaterial() == parent);

    // The parent material and MAX_DEPTH - 1 instances under it: one more is refused.
    Vector<SPtr<TestInstanceAsset>> chain;
    SPtr<MaterialInterfaceAsset> top = parent;
    for (uint32 i = 0; i + 1 < MaterialInstanceAsset::MAX_DEPTH; ++i) {
      chain.push_back(createTestInstance("MI_Link", top));
      top = chain.back();
    }
    const SPtr<TestInstanceAsset> tooDeep = createTestInstance("MI_TooDeep");
    REQUIRE_FALSE(tooDeep->setParent(top));
    REQUIRE(tooDeep->setParent(chain[chain.size() - 2]));
    tooDeep->setParent(nullptr);
    for (SIZE_T i = chain.size(); i-- > 0;) {
      chain[i]->setParent(nullptr);
    }
    grandChild->setParent(nullptr);
  }

  SECTION("Textures")
  {
    const chEngineSDK::UUID parentTexture = chEngineSDK::UUID::createRandom();
    parent->getMaterial().setTexture(MaterialTexture::BaseColor, parentTexture, nullptr);
    parent->notifyMaterialChanged();
    REQUIRE(instance->getMaterial().getTextureId(MaterialTexture::BaseColor) == parentTexture);

    // Null clears the parent's texture instead of using it.
    instance->setTexture(MaterialTexture::BaseColor, nullptr);
    REQUIRE(instance->getMaterial().getTextureId(MaterialTexture::BaseColor).isNull());
    REQUIRE(instance->resetTexture(MaterialTexture::BaseColor));
    REQUIRE(instance->getMaterial().getTextureId(MaterialTexture::BaseColor) == parentTexture);
  }

  SECTION("No parent takes the defaults")
  {
    REQUIRE(instance->setParent(nullptr));
    REQUIRE(instance->getParentId().isNull());
    REQUIRE(instance->getMaterial().getBaseColorFactor().r == 1.0f);
    REQUIRE(instance->getMaterial().getBaseColorFactor().g == 1.0f);
    REQUIRE(instance->getMaterial().getRoughnessFactor() == 0.2f);
  }

  SECTION("Saving keeps the parent id and the overrides")
  {
    const chEngineSDK::UUID textureId = chEngineSDK::UUID::createRandom();
    instance->setValue("baseColor", Vector4(0.1f, 0.2f, 0.3f, 1.0f));
    instance->setValue("alphaMode", Vector4(1.0f, 0.0f, 0.0f, 0.0f));
    instance->setTexture(MaterialTexture::Normal, nullptr);
    MaterialOverrides overridesWithId = instance->getOverrides();
    overridesWithId.setTexture(MaterialTexture::Normal, textureId, nullptr);

    Vector<chEngineSDK::UUID> references;
    instance->collectReferences(references);
    REQUIRE(references == Vector<chEngineSDK::UUID>{parent->getUUID()});

    const SPtr<MemoryDataStream> stream = chMakeShared<MemoryDataStream>();
    REQUIRE(instance->serialize(stream));
    const SIZE_T size = stream->size();

    // No asset manager runs here, so the parent is kept by id only.
    stream->seek(0);
    const SPtr<TestInstanceAsset> loaded = createTestInstance("MI_Loaded");
    REQUIRE(loaded->deserialize(stream));
    REQUIRE(loaded->getParentId() == parent->getUUID());
    REQUIRE(loaded->getParentMaterial() == nullptr);
    REQUIRE(loaded->getOverrides().getValues().size() == 3);
    REQUIRE(loaded->getMaterial().getRoughnessFactor() == 0.2f);
    REQUIRE(loaded->getMaterial().getAlphaMode() == MaterialAlphaMode::Mask);
    REQUIRE(loaded->getMaterial().getBaseColorFactor() == LinearColor(0.1f, 0.2f, 0.3f, 1.0f));
    REQUIRE(loaded->getOverrides().findTexture(MaterialTexture::Normal));

    const SPtr<MemoryDataStream> again = chMakeShared<MemoryDataStream>();
    REQUIRE(loaded->serialize(again));
    REQUIRE(again->size() == size);

    SECTION("A cut file is refused")
    {
      for (const SIZE_T cut : {SIZE_T(3), SIZE_T(22), size - 1}) {
        Vector<uint8> bytes(cut);
        stream->seek(0);
        REQUIRE(stream->read(bytes.data(), cut) == cut);
        const SPtr<MemoryDataStream> cutStream = chMakeShared<MemoryDataStream>();
        cutStream->write(bytes.data(), cut);
        cutStream->seek(0);
        REQUIRE_FALSE(createTestInstance("MI_Cut")->deserialize(cutStream));
      }
    }

    SECTION("A newer version is refused")
    {
      stream->seek(0);
      const uint32 newer = MaterialInstanceAssetHeader::VERSION + 1;
      stream->write(&newer, sizeof(newer));
      stream->seek(0);
      REQUIRE_FALSE(createTestInstance("MI_Newer")->deserialize(stream));
    }

    SECTION("Texture overrides with an id become references")
    {
      MaterialOverrides overrides = overridesWithId;
      references.clear();
      overrides.collectReferences(references);
      REQUIRE(references == Vector<chEngineSDK::UUID>{textureId});
    }
  }

  SECTION("Unknown names in a file are skipped")
  {
    MemoryDataStream stream;
    const uint32 counts[2] = {2, 1};
    stream.write(counts, sizeof(counts));
    const float value[4] = {0.7f, 0.0f, 0.0f, 0.0f};
    AssetFile::writeString(stream, "glowiness");
    stream.write(value, sizeof(value));
    AssetFile::writeString(stream, "metallic");
    stream.write(value, sizeof(value));
    const chEngineSDK::UUID textureId = chEngineSDK::UUID::createRandom();
    AssetFile::writeString(stream, "detailTexture");
    stream.write(&textureId, sizeof(textureId));
    stream.seek(0);

    MaterialOverrides overrides;
    REQUIRE(overrides.deserialize(stream, "Test"));
    REQUIRE(overrides.getValues().size() == 1);
    REQUIRE(overrides.getValues()[0].name == "metallic");
    REQUIRE(overrides.getTextures().empty());
  }

  SECTION("Unloading clears it and tells its own instances")
  {
    const SPtr<TestInstanceAsset> grandChild = createTestInstance("MI_GrandChild", instance);
    instance->clearAssetData();
    REQUIRE(instance->getOverrides().isEmpty());
    REQUIRE(instance->getParentMaterial() == nullptr);
    REQUIRE(grandChild->getMaterial().getRoughnessFactor() == 0.5f);
    grandChild->setParent(nullptr);
  }

  instance->setParent(nullptr);
}

TEST_CASE("chCore - Dynamic material", "[Resources]")
{
  const SPtr<MaterialAsset> parent = createTestMaterialAsset("M_Parent");
  parent->getMaterial().setRoughnessFactor(0.8f);
  parent->notifyMaterialChanged();

  SECTION("It follows its parent and keeps its own values")
  {
    DynamicMaterial dynamic(parent);
    REQUIRE(dynamic.getMaterial().getRoughnessFactor() == 0.8f);
    REQUIRE(dynamic.setValue("emissive", Vector4(5.0f, 0.0f, 0.0f, 1.0f)));
    REQUIRE(dynamic.getMaterial().getEmissiveFactor().r == 5.0f);
    REQUIRE(parent->getMaterial().getEmissiveFactor().r == 0.0f);

    parent->getMaterial().setRoughnessFactor(0.4f);
    parent->notifyMaterialChanged();
    REQUIRE(dynamic.getMaterial().getRoughnessFactor() == 0.4f);
    REQUIRE(dynamic.getMaterial().getEmissiveFactor().r == 5.0f);

    DynamicMaterial copy;
    copy.copyFrom(dynamic);
    REQUIRE(copy.getParent() == parent);
    REQUIRE(copy.getMaterial().getEmissiveFactor().r == 5.0f);
    REQUIRE(copy.resetValue("emissive"));
    REQUIRE(copy.getMaterial().getEmissiveFactor().r == 0.0f);
  }

  SECTION("Shapes draw with it, alone or shared")
  {
    Scene scene("Test", chEngineSDK::UUID::createRandom());
    const SPtr<GameObject> first = scene.createGameObject("First");
    const SPtr<GameObject> second = scene.createGameObject("Second");
    ShapeComponent& firstShape =
        first->addComponent<ShapeComponent>(ShapeDesc{.type = ShapeType::Box});
    ShapeComponent& secondShape =
        second->addComponent<ShapeComponent>(ShapeDesc{.type = ShapeType::Box});
    firstShape.setMaterial(parent);
    secondShape.setMaterial(parent);
    REQUIRE(getShapeRenderMaterial(scene, *first) == &parent->getMaterial());

    // Its own dynamic material changes only this object.
    DynamicMaterial& own = firstShape.getOwnDynamicMaterial();
    REQUIRE(&firstShape.getOwnDynamicMaterial() == &own);
    own.setValue("roughness", Vector4(0.1f, 0.0f, 0.0f, 0.0f));
    REQUIRE(getShapeRenderMaterial(scene, *first) == &own.getMaterial());
    REQUIRE(getShapeRenderMaterial(scene, *first)->getRoughnessFactor() == 0.1f);
    REQUIRE(getShapeRenderMaterial(scene, *second) == &parent->getMaterial());

    // A shared one draws both; asking for an own one then copies it.
    const SPtr<DynamicMaterial> shared = chMakeShared<DynamicMaterial>(parent);
    shared->setValue("metallic", Vector4(1.0f, 0.0f, 0.0f, 0.0f));
    firstShape.setDynamicMaterial(shared);
    secondShape.setDynamicMaterial(shared);
    REQUIRE(getShapeRenderMaterial(scene, *second) == &shared->getMaterial());
    DynamicMaterial& copied = secondShape.getOwnDynamicMaterial();
    REQUIRE(&copied != shared.get());
    REQUIRE(copied.getMaterial().getMetallicFactor() == 1.0f);
    REQUIRE(getShapeRenderMaterial(scene, *first) == &shared->getMaterial());

    // Setting an asset drops the dynamic material.
    firstShape.setMaterial(parent);
    REQUIRE(firstShape.getDynamicMaterial() == nullptr);
    REQUIRE(getShapeRenderMaterial(scene, *first) == &parent->getMaterial());
  }
}
