/************************************************************************/
/**
 * @file chScriptPostProcessTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of the PostProcess handle scripts get from a GameObject.
 */
/************************************************************************/
#include "chGameObject.h"
#include "chPostProcessComponent.h"
#include "chScene.h"
#include "chScriptManager.h"

#include "catch.hpp"
#include "chPostProcessTestHelpers.h"
#include "chScriptTestHelpers.h"

using namespace chEngineSDK;

TEST_CASE("chCore - Script post-process", "[Scripting]")
{
  ScriptManager& scripts = getTestScriptManager();
  Scene scene("Looks", chEngineSDK::UUID::createRandom());
  struct SceneGuard
  {
    ScriptManager& scripts;
    ~SceneGuard() { scripts.setCurrentScene(nullptr); }
  } guard{scripts};
  scripts.setCurrentScene(&scene);

  const SPtr<ShaderAsset> vignette = makeTestPostProcessShader("PP_Vignette");
  const SPtr<ShaderAsset> grade = makeTestPostProcessShader("PP_Grade");
  const SPtr<GameObject> look = scene.createGameObject("Look");
  PostProcessComponent& postProcess = look->addComponent<PostProcessComponent>();
  postProcess.addPass(vignette);
  postProcess.addPass(grade);
  const SPtr<GameObject> plain = scene.createGameObject("Plain");

  SECTION("Passes are found by place or by shader name")
  {
    REQUIRE(scripts.runString(R"lua(
      local pp = Scene.find("Look"):getPostProcess()
      assert(pp ~= nil)
      assert(pp == Scene.find("Look"):getPostProcess())
      assert(pp:getGameObject() == Scene.find("Look"))
      assert(pp:getPassCount() == 2)
      assert(pp:findPass("PP_Grade") == 2)
      assert(pp:findPass("PP_Missing") == nil)

      pp:setEnabled("PP_Grade", false)
      assert(not pp:isEnabled(2))
      pp:setWeight(1, 0.5)
      assert(pp:getWeight("PP_Vignette") == 0.5)
      pp:setPriority(3)
      assert(pp:getPriority() == 3)
    )lua"));
    REQUIRE_FALSE(postProcess.getPass(1).bEnabled);
    REQUIRE(postProcess.getPass(0).weight == 0.5f);
    REQUIRE(postProcess.getPriority() == 3);

    REQUIRE_FALSE(scripts.runString(R"lua(Scene.find("Look"):getPostProcess():isEnabled(3))lua"));
    REQUIRE_FALSE(scripts.runString(R"lua(Scene.find("Look"):getPostProcess():isEnabled("X"))lua"));
  }

  SECTION("Values take the type of the shader's parameter")
  {
    REQUIRE(scripts.runString(R"lua(
      local pp = Scene.find("Look"):getPostProcess()
      assert(pp:getValue(1, "intensity") == 1)
      pp:setValue(1, "intensity", 1.75)
      assert(pp:getValue(1, "intensity") == 1.75)
      pp:setValue(1, "tint", Color(1, 0.5, 0, 1))
      assert(pp:getValue(1, "tint") == Color(1, 0.5, 0, 1))
      pp:setValue(1, "steps", 5)
      assert(math.type(pp:getValue(1, "steps")) == "integer")
      assert(pp:getValue(1, "steps") == 5)
      pp:setValue(1, "flip", true)
      assert(pp:getValue(1, "flip") == true)
      pp:resetValue(1, "intensity")
      assert(pp:getValue(1, "intensity") == 1)
      -- The other pass keeps its defaults.
      assert(pp:getValue("PP_Grade", "steps") == 3)
    )lua"));
    REQUIRE(postProcess.findOverride(0, "tint"));
    REQUIRE_FALSE(postProcess.findOverride(0, "intensity"));

    REQUIRE_FALSE(scripts.runString(
        R"lua(Scene.find("Look"):getPostProcess():setValue(1, "missing", 1))lua"));
    REQUIRE_FALSE(scripts.runString(
        R"lua(Scene.find("Look"):getPostProcess():setValue(1, "tint", 1))lua"));
    REQUIRE_FALSE(scripts.runString(
        R"lua(Scene.find("Look"):getPostProcess():setValue(1, "intensity", "high"))lua"));
  }

  SECTION("Adding and removing")
  {
    REQUIRE(scripts.runString(R"lua(
      local plain = Scene.find("Plain")
      assert(plain:getPostProcess() == nil)
      local pp = plain:addPostProcess()
      assert(pp == plain:getPostProcess())
      assert(plain:addPostProcess() == pp)
      assert(pp:getPassCount() == 0)

      local look = Scene.find("Look"):getPostProcess()
      look:removePass("PP_Vignette")
      assert(look:getPassCount() == 1)
      assert(look:findPass("PP_Grade") == 1)
    )lua"));
    REQUIRE(plain->getComponent<PostProcessComponent>());
    REQUIRE(scene.getPostProcesses().size() == 2);
    // Without an AssetManager no shader can be named.
    REQUIRE_FALSE(scripts.runString(
        R"lua(Scene.find("Plain"):getPostProcess():addPass("PP_Vignette"))lua"));
  }
}
