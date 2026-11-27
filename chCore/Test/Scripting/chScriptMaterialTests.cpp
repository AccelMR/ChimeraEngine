/************************************************************************/
/**
 * @file chScriptMaterialTests.cpp
 * @author AccelMR
 * @date 2026/11/27
 * @brief Tests of materials, material instances and the Model and Shape handles in scripts.
 */
/************************************************************************/
#include "chDynamicMaterial.h"
#include "chGameObject.h"
#include "chMaterialAsset.h"
#include "chModel.h"
#include "chModelAsset.h"
#include "chModelComponent.h"
#include "chScene.h"
#include "chScriptManager.h"
#include "chShapeComponent.h"
#include "chUUID.h"

#include "catch.hpp"
#include "chModelTestHelpers.h"
#include "chScriptTestHelpers.h"

using namespace chEngineSDK;

TEST_CASE("chCore - Script materials", "[Scripting]")
{
  ScriptManager& scripts = getTestScriptManager();
  Scene scene("Materials", chEngineSDK::UUID::createRandom());
  struct SceneGuard
  {
    ScriptManager& scripts;
    ~SceneGuard() { scripts.setCurrentScene(nullptr); }
  } guard{scripts};
  scripts.setCurrentScene(&scene);

  const SPtr<MaterialAsset> parent = createTestMaterialAsset("M_Stone");
  parent->getMaterial().setRoughnessFactor(0.8f);
  parent->notifyMaterialChanged();

  const SPtr<GameObject> box = scene.createGameObject("Box");
  ShapeComponent& shape = box->addComponent<ShapeComponent>(ShapeDesc{.type = ShapeType::Box});
  shape.setMaterial(parent);
  const SPtr<GameObject> otherBox = scene.createGameObject("OtherBox");
  ShapeComponent& otherShape =
      otherBox->addComponent<ShapeComponent>(ShapeDesc{.type = ShapeType::Box});
  otherShape.setMaterial(parent);

  const SPtr<Model> model = createTriangleModel();
  model->addMaterialSlot(
      {.name = "Body", .materialId = chEngineSDK::UUID::null(), .material = nullptr});
  model->setSlotMaterial(0, parent);
  const SPtr<GameObject> statue = scene.createGameObject("Statue");
  ModelComponent& modelComponent =
      statue->addComponent<ModelComponent>(createTestModelAsset(model));

  SECTION("A MaterialInstance made at run time")
  {
    REQUIRE(scripts.runString(R"lua(
      local instance = Material.createInstance()
      assert(instance:getParent() == nil)
      assert(instance:getValue("roughness") == 0.5)
      instance:setValue("roughness", 0.25)
      instance:setValue("baseColor", Color(1, 0, 0, 1))
      instance:setValue("alphaMode", "mask")
      instance:setValue("twoSided", true)
      assert(instance:getValue("roughness") == 0.25)
      assert(instance:getValue("baseColor") == Color(1, 0, 0, 1))
      assert(instance:getValue("alphaMode") == "Mask")
      assert(instance:getValue("twoSided") == true)
      instance:resetValue("roughness")
      assert(instance:getValue("roughness") == 0.5)
      assert(instance == instance)
      assert(tostring(instance) == "MaterialInstance(no parent)")
      instance:setTexture("normalTexture", nil)
      instance:resetTexture("normalTexture")
    )lua"));
    REQUIRE_FALSE(scripts.runString(R"lua(Material.createInstance():setValue("glow", 1))lua"));
    REQUIRE_FALSE(
        scripts.runString(R"lua(Material.createInstance():setValue("roughness", "a"))lua"));
    REQUIRE_FALSE(
        scripts.runString(R"lua(Material.createInstance():setValue("alphaMode", "Fade"))lua"));
    REQUIRE_FALSE(
        scripts.runString(R"lua(Material.createInstance():setTexture("detail", nil))lua"));
    // Without an AssetManager no material can be named.
    REQUIRE_FALSE(scripts.runString(R"lua(Material.createInstance("M_Stone"))lua"));
  }

  SECTION("A Shape's own values change only that object")
  {
    REQUIRE(scripts.runString(R"lua(
      local shape = Scene.find("Box"):getShape()
      assert(shape ~= nil)
      assert(shape:getGameObject() == Scene.find("Box"))
      assert(shape:getMaterialValue("roughness") > 0.79)
      shape:setMaterialValue("emissive", Color(3, 0, 0, 1))
      assert(shape:getMaterialValue("emissive") == Color(3, 0, 0, 1))
      assert(Scene.find("OtherBox"):getShape():getMaterialValue("emissive") == Color(0, 0, 0, 1))
      assert(Scene.find("Statue"):getShape() == nil)
    )lua"));
    REQUIRE(shape.getDynamicMaterial());
    REQUIRE(shape.getDynamicMaterial()->getParent() == parent);
    REQUIRE(shape.getDynamicMaterial()->getMaterial().getEmissiveFactor().r == 3.0f);
    REQUIRE_FALSE(otherShape.getDynamicMaterial());

    // The parent's edits still reach the object.
    parent->getMaterial().setMetallicFactor(1.0f);
    parent->notifyMaterialChanged();
    REQUIRE(shape.getDynamicMaterial()->getMaterial().getMetallicFactor() == 1.0f);

    REQUIRE(scripts.runString(R"lua(
      local shape = Scene.find("Box"):getShape()
      shape:resetMaterialValue("emissive")
      assert(shape:getMaterialValue("emissive") == Color(0, 0, 0, 1))
      shape:setMaterial(nil)
      assert(shape:getMaterial() == nil)
    )lua"));
    REQUIRE_FALSE(shape.getDynamicMaterial());
    REQUIRE_FALSE(shape.getMaterial());
  }

  SECTION("One MaterialInstance shared by several objects")
  {
    REQUIRE(scripts.runString(R"lua(
      local glow = Material.createInstance()
      glow:setValue("emissive", Color(0, 2, 0, 1))
      Scene.find("Box"):getShape():setMaterial(glow)
      Scene.find("OtherBox"):getShape():setMaterial(glow)
      assert(Scene.find("Box"):getShape():getMaterial() == glow)
      glow:setValue("metallic", 0.5)
    )lua"));
    REQUIRE(shape.getDynamicMaterial() == otherShape.getDynamicMaterial());
    REQUIRE(shape.getDynamicMaterial()->getMaterial().getMetallicFactor() == 0.5f);

    // The script let go of its value; the shapes keep the instance alive.
    REQUIRE(scripts.runString("collectgarbage()"));
    REQUIRE(shape.getDynamicMaterial()->getMaterial().getEmissiveFactor().g == 2.0f);
  }

  SECTION("A Model's slots")
  {
    REQUIRE(scripts.runString(R"lua(
      local model = Scene.find("Statue"):getModel()
      assert(model ~= nil)
      assert(model:getMaterialSlotCount() == 1)
      assert(model:getMaterialSlotName(1) == "Body")
      assert(model:getMaterialSlotName(2) == nil)
      assert(model:getMaterialValue(1, "roughness") > 0.79)
      model:setMaterialValue("Body", "roughness", 0.1)
      assert(model:getMaterialValue(1, "roughness") < 0.11)
      local instance = model:getMaterial(1)
      assert(instance ~= nil)
      instance:setValue("metallic", 0.75)
      model:resetMaterialValue(1, "roughness")
      assert(model:getMaterialValue(1, "roughness") > 0.79)
    )lua"));
    REQUIRE(modelComponent.getDynamicMaterial(0));
    REQUIRE(modelComponent.getDynamicMaterial(0)->getMaterial().getMetallicFactor() == 0.75f);
    REQUIRE_FALSE(scripts.runString(R"lua(Scene.find("Statue"):getModel():getMaterial(2))lua"));
    REQUIRE_FALSE(
        scripts.runString(R"lua(Scene.find("Statue"):getModel():getMaterial("Head"))lua"));

    REQUIRE(scripts.runString(R"lua(
      Scene.find("Statue"):getModel():setMaterial(1, nil)
    )lua"));
    REQUIRE_FALSE(modelComponent.getDynamicMaterial(0));
  }
}
