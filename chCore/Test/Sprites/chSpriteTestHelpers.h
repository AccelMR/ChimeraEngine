/************************************************************************/
/**
 * @file chSpriteTestHelpers.h
 * @author AccelMR
 * @date 2026/11/17
 * @brief Helpers that the sprite, text and script tests share.
 */
/************************************************************************/
#pragma once

#include "chCamera.h"
#include "chSpriteAtlas.h"
#include "chSpriteAtlasAsset.h"
#include "chStringUtils.h"
#include "chUUID.h"

#include "catch.hpp"

namespace chEngineSDK {

/*
 * A hero sheet: two idle frames standing on their bottom center, three run frames, and
 * animations that loop and one that does not.
 */
NODISCARD inline SpriteAtlas
createTestAtlas()
{
  SpriteAtlas atlas;
  const Vector2 feet(0.5f, 0.0f);
  atlas.getFrames() = {
      {.name = "idle_0", .position = Vector2(0, 0), .size = Vector2(32, 32), .pivot = feet},
      {.name = "idle_1", .position = Vector2(32, 0), .size = Vector2(32, 32), .pivot = feet},
      {.name = "run_0", .position = Vector2(0, 32), .size = Vector2(64, 32)},
      {.name = "run_1", .position = Vector2(64, 32), .size = Vector2(64, 32)},
      {.name = "run_2", .position = Vector2(128, 32), .size = Vector2(64, 32)}};
  atlas.getAnimations() = {
      {.name = "Idle", .frames = {{0, 0.5f}, {1, 0.5f}}, .bLoop = true},
      {.name = "Run", .frames = {{2, 0.1f}, {3, 0.1f}, {4, 0.2f}}, .bLoop = true},
      {.name = "Die", .frames = {{0, 0.25f}, {1, 0.25f}}, .bLoop = false}};
  return atlas;
}

/*
 * Looks along +X at the YZ plane, which is where 2D games live: right is +Y, up is +Z.
 */
NODISCARD inline Camera
createSpriteTestCamera()
{
  Camera camera(Vector3(-10.0f, 0.0f, 0.0f), Vector3::ZERO, 1280.0f, 720.0f);
  camera.setProjectionType(CameraProjectionType::Orthographic);
  camera.setOrthographicSize(5.0f);
  camera.setClipPlanes(0.1f, 100.0f);
  return camera;
}

NODISCARD inline SPtr<SpriteAtlasAsset>
createTestAtlasAsset()
{
  AssetMetadata metadata;
  metadata.uuid = chEngineSDK::UUID::createRandom();
  metadata.assetType = AssetTypeTraits<SpriteAtlasAsset>::getTypeId();
  StringUtils::copyToBuffer(metadata.name, "Hero");
  return chMakeShared<SpriteAtlasAsset>(metadata, createTestAtlas(), nullptr);
}

} // namespace chEngineSDK
