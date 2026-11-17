/************************************************************************/
/**
 * @file chRenderFrameTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of the render frame copy and the debug lines.
 */
/************************************************************************/
#include "chBox.h"
#include "chCamera.h"
#include "chCapsule.h"
#include "chConsoleVariable.h"
#include "chDebugDraw.h"
#include "chLightComponent.h"
#include "chLinearColor.h"
#include "chMath.h"
#include "chMesh.h"
#include "chModel.h"
#include "chModelAsset.h"
#include "chModelComponent.h"
#include "chOrientedBox.h"
#include "chQuaternion.h"
#include "chRenderFrame.h"
#include "chRenderSettings.h"
#include "chRotator.h"
#include "chScene.h"
#include "chSkyLightComponent.h"
#include "chSphere.h"
#include "chUUID.h"
#include "chVector3.h"

#include "catch.hpp"
#include "chCoreTestHelpers.h"
#include "chModelTestHelpers.h"

using namespace chEngineSDK;

TEST_CASE("chCore - Render frame", "[Renderer]")
{
  Scene scene("Test", chEngineSDK::UUID::createRandom());
  const SPtr<Model> model = createTriangleModel();
  const SPtr<GameObject> first = scene.createGameObject("First");
  first->addComponent<ModelComponent>(createTestModelAsset(model));
  const SPtr<GameObject> second = scene.createGameObject("Second");
  second->addComponent<ModelComponent>(createTestModelAsset(model));
  const SPtr<GameObject> sun = scene.createGameObject("Sun");
  sun->addComponent<DirectionalLightComponent>();
  const SPtr<GameObject> sky = scene.createGameObject("Sky");
  SkyLightComponent& skyLight = sky->addComponent<SkyLightComponent>();
  skyLight.setIntensity(2.0f);
  scene.updateTransforms();

  // A free slot in the middle of the scene's items.
  scene.destroyGameObject(*first);
  REQUIRE(scene.getRenderItemSlots().size() == 2);

  const Camera camera = createTestCamera();
  const LinearColor clearColor(0.1f, 0.2f, 0.3f, 1.0f);
  RenderFrame frame;
  frame.extract(scene, camera, clearColor);
  REQUIRE(frame.getItems().size() == 1);
  REQUIRE(frame.getItems()[0].owner == second.get());
  REQUIRE(frame.getLights().size() == 1);
  REQUIRE(frame.getSkyLight() != nullptr);
  REQUIRE(frame.getSkyLight()->intensity == 2.0f);
  REQUIRE(frame.getClearColor().b == 0.3f);
  REQUIRE(frame.getCamera().getPosition() == camera.getPosition());

  Vector<const RenderItem*> visibleItems;
  frame.gatherItems(frame.getCamera().getFrustum(), visibleItems);
  REQUIRE(visibleItems.size() == 1);
  REQUIRE(visibleItems[0] == &frame.getItems()[0]);
  Vector<const LightItem*> visibleLights;
  frame.gatherLights(frame.getCamera().getFrustum(), visibleLights);
  REQUIRE(visibleLights.size() == 1);

  SECTION("Changes to the scene wait for the next extract")
  {
    second->getTransform().setLocalPosition(Vector3(-20.0f, 0.0f, 0.0f));
    scene.updateTransforms();
    sky->removeComponent(skyLight);
    REQUIRE(frame.getItems()[0].worldBounds.center.nearEqual(Vector3(0.0f, 1.0f, 0.0f)));
    REQUIRE(frame.getSkyLight() != nullptr);

    frame.extract(scene, camera, clearColor);
    frame.gatherItems(frame.getCamera().getFrustum(), visibleItems);
    REQUIRE(visibleItems.empty());
    REQUIRE(frame.getSkyLight() == nullptr);
  }
}

#if USING(CH_DEBUG_DRAW)
TEST_CASE("chCore - Debug draw", "[Renderer]")
{
  DebugDraw debugDraw;
  REQUIRE(debugDraw.getLineCount() == 0);

  SECTION("Each shape adds its lines")
  {
    debugDraw.box(AABox(Vector3(-1.0f, -1.0f, -1.0f), Vector3(1.0f, 1.0f, 1.0f)),
                  LinearColor::RED);
    REQUIRE(debugDraw.getLineCount() == 12);
    debugDraw.circle(Vector3::ZERO, Vector3::UP, 1.0f, LinearColor::RED, DebugDepth::Tested,
                     16);
    REQUIRE(debugDraw.getLineCount() == 28);
    debugDraw.sphere(Sphere(Vector3::ZERO, 1.0f), LinearColor::RED, DebugDepth::Tested, 8);
    REQUIRE(debugDraw.getLineCount() == 52);
    // Two circles, four side lines and four half circles.
    debugDraw.capsule(Capsule(Vector3::ZERO, Vector3::UP, 0.5f), LinearColor::RED,
                      DebugDepth::Tested, 8);
    REQUIRE(debugDraw.getLineCount() == 52 + 16 + 4 + 16);
    debugDraw.axes(Matrix4::IDENTITY, 1.0f);
    REQUIRE(debugDraw.getLineCount() == 91);
    REQUIRE(debugDraw.getVertices(DebugDepth::OnTop).empty());

    debugDraw.clear();
    REQUIRE(debugDraw.getLineCount() == 0);
  }

  SECTION("Lines on top are kept apart, with their color packed")
  {
    debugDraw.line(Vector3::ZERO, Vector3::UP, LinearColor(1.0f, 0.5f, 0.0f, 1.0f),
                   DebugDepth::OnTop);
    REQUIRE(debugDraw.getVertices(DebugDepth::Tested).empty());
    const Span<const DebugLineVertex> vertices = debugDraw.getVertices(DebugDepth::OnTop);
    REQUIRE(vertices.size() == 2);
    REQUIRE(vertices[1].position == Vector3::UP);
    REQUIRE(vertices[0].color == 0xFF0080FFu);
  }

  SECTION("An oriented box turns with its rotation")
  {
    debugDraw.orientedBox(OrientedBox(Vector3(0.0f, 0.0f, 5.0f), Vector3(2.0f, 1.0f, 1.0f),
                                      Quaternion(Rotator(0.0f, 90.0f, 0.0f))),
                          LinearColor::GREEN);
    REQUIRE(debugDraw.getLineCount() == 12);
    for (const DebugLineVertex& vertex : debugDraw.getVertices(DebugDepth::Tested)) {
      REQUIRE(Math::abs(Math::abs(vertex.position.x) - 1.0f) < 1e-4f);
      REQUIRE(Math::abs(Math::abs(vertex.position.y) - 2.0f) < 1e-4f);
      REQUIRE(Math::abs(Math::abs(vertex.position.z - 5.0f) - 1.0f) < 1e-4f);
    }
  }

  SECTION("A frustum joins the corners of what the camera sees")
  {
    const Camera camera(Vector3::ZERO, Vector3::FORWARD, 1280.0f, 720.0f);
    debugDraw.frustum(camera.getViewProjectionMatrix(), LinearColor::WHITE);
    REQUIRE(debugDraw.getLineCount() == 12);
    for (const DebugLineVertex& vertex : debugDraw.getVertices(DebugDepth::Tested)) {
      // In front of the camera, between its near and far planes.
      REQUIRE(vertex.position.x >= camera.getNearClipPlane() * 0.99f);
      REQUIRE(vertex.position.x <= camera.getFarClipPlane() * 1.01f);
    }
  }

  SECTION("Lines past the limit are dropped")
  {
    g_cvarDebugLinesMax.set(10, ConsoleVariableSource::Console);
    debugDraw.box(AABox(Vector3::ZERO, Vector3::UNIT), LinearColor::RED);
    REQUIRE(debugDraw.getLineCount() == 10);
    g_cvarDebugLinesMax.set(g_cvarDebugLinesMax.getDefault(), ConsoleVariableSource::Console);
  }

  SECTION("A scene forgets its lines when it updates")
  {
    Scene scene("Test", chEngineSDK::UUID::createRandom());
    scene.getDebugDraw().line(Vector3::ZERO, Vector3::UP, LinearColor::RED);
    REQUIRE(scene.getDebugDraw().getLineCount() == 1);
    scene.update(0.016f);
    REQUIRE(scene.getDebugDraw().getLineCount() == 0);
  }

  SECTION("Lines of the fixed step last until the next fixed step")
  {
    Scene scene("Test", chEngineSDK::UUID::createRandom());
    scene.getFixedDebugDraw().line(Vector3::ZERO, Vector3::UP, LinearColor::RED);
    scene.update(0.016f);
    REQUIRE(scene.getFixedDebugDraw().getLineCount() == 1);
    scene.fixedUpdate(0.016f);
    REQUIRE(scene.getFixedDebugDraw().getLineCount() == 0);
  }

  SECTION("A render frame copies every list")
  {
    Scene scene("Test", chEngineSDK::UUID::createRandom());
    scene.getDebugDraw().line(Vector3::ZERO, Vector3::UP, LinearColor::RED);
    scene.getFixedDebugDraw().line(Vector3::ZERO, Vector3::RIGHT, LinearColor::RED);
    DebugDraw extraLines;
    extraLines.axes(Matrix4::IDENTITY, 1.0f);
    RenderFrame frame;
    frame.extract(scene, createTestCamera(), LinearColor::BLACK, &extraLines);
    scene.update(0.016f);
    const Span<const DebugDraw* const> lists = frame.getDebugLines();
    REQUIRE(lists.size() == 3);
    REQUIRE(lists[0]->getLineCount() == 1);
    REQUIRE(lists[1]->getLineCount() == 1);
    REQUIRE(lists[2]->getLineCount() == 3);
    frame.extract(scene, createTestCamera(), LinearColor::BLACK);
    REQUIRE(lists[0]->getLineCount() == 0);
    REQUIRE(lists[2]->getLineCount() == 0);
  }
}
#endif
#if USING(CH_DEBUG_DRAW)
#else
TEST_CASE("chCore - Debug draw", "[Renderer]")
{
  // Built without the debug lines: every call compiles and keeps nothing.
  DebugDraw debugDraw;
  debugDraw.line(Vector3::ZERO, Vector3::UP, LinearColor::RED);
  debugDraw.box(AABox(Vector3::ZERO, Vector3::UNIT), LinearColor::RED, DebugDepth::OnTop);
  debugDraw.sphere(Sphere(Vector3::ZERO, 1.0f), LinearColor::RED);
  debugDraw.capsule(Capsule(Vector3::ZERO, Vector3::UP, 0.5f), LinearColor::RED);
  debugDraw.frustum(Matrix4::IDENTITY, LinearColor::RED);
  debugDraw.axes(Matrix4::IDENTITY, 1.0f);
  REQUIRE(debugDraw.getLineCount() == 0);
  REQUIRE(debugDraw.getVertices(DebugDepth::Tested).empty());
}
#endif // USING(CH_DEBUG_DRAW)
