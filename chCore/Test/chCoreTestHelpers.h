/************************************************************************/
/**
 * @file chCoreTestHelpers.h
 * @author AccelMR
 * @date 2026/11/17
 * @brief Helpers that tests of several topics share.
 */
/************************************************************************/
#pragma once

#include "chAngle.h"
#include "chCamera.h"
#include "chVector3.h"

namespace chEngineSDK {

// Looks along +X (FORWARD) from 5 units behind the origin.
inline Camera
createTestCamera()
{
  Camera camera(Vector3(-5.0f, 0.0f, 0.0f), Vector3::ZERO, 1280.0f, 720.0f);
  camera.setProjectionType(CameraProjectionType::Perspective);
  camera.setFieldOfView(Radian(Degree(90.0f)));
  camera.setClipPlanes(0.1f, 100.0f);
  return camera;
}

} // namespace chEngineSDK
