/************************************************************************/
/**
 * @file chEditorCamera.cpp
 * @author AccelMR
 * @date 2026/10/07
 * @details
 *  Camera of the editor viewport, moved with the mouse and keyboard.
 */
/************************************************************************/
#include "chEditorCamera.h"

#include "chBox.h"
#include "chConsoleVariable.h"
#include "chEngineConfig.h"
#include "chInputManager.h"
#include "chMath.h"
#include "chRenderSettings.h"
#include "chStringUtils.h"

namespace chEngineSDK {

namespace {
ConsoleVariable<float> g_cvarCameraSpeed("Editor.CameraSpeed", 5.0f,
                                         "Units per second the viewport camera flies.",
                                         "CameraSpeed");
ConsoleVariable<float>
    g_cvarCameraLookSensitivity("Editor.CameraLookSensitivity", 0.15f,
                                "Degrees the viewport camera turns per pixel the mouse moves.",
                                "LookSensitivity");
ConsoleVariable<float> g_cvarCameraSprintMultiplier(
    "Editor.CameraSprintMultiplier", 3.0f,
    "How many times faster the viewport camera flies while Shift is held.",
    "SprintMultiplier");
ConsoleVariable<float>
    g_cvarCameraFieldOfView("Editor.CameraFieldOfView", 90.0f,
                            "Horizontal field of view of the viewport camera, in degrees.",
                            "FieldOfView");
ConsoleVariable<float> g_cvarCameraNearPlane("Editor.CameraNearPlane", 0.1f,
                                             "Near clip plane of the viewport camera.",
                                             "NearPlane");
ConsoleVariable<float> g_cvarCameraFarPlane("Editor.CameraFarPlane", 10000.0f,
                                            "Far clip plane of the viewport camera.",
                                            "FarPlane");
ConsoleVariable<bool> g_cvarViewport2D(
    "Editor.Viewport2D", false,
    "Shows the viewport as a 2D view: orthographic, looking along +X at the plane x = 0. "
    "Until it is set, projects whose Renderer.Path is Sprite2D start in 2D.",
    "Viewport2D");
ConsoleVariable<float> g_cvarCamera2DHalfHeight(
    "Editor.Camera2DHalfHeight", 5.0f,
    "Half the height the 2D view shows, in world units; the wheel changes it.");
ConsoleVariable<float> g_cvarCamera2DZoomStep(
    "Editor.Camera2DZoomStep", 1.15f,
    "How many times the 2D view grows or shrinks per wheel step.");

constexpr Vector3 kStartPosition(-5.0f, 0.0f, 0.0f);
// Keeps the look at point from collapsing onto the camera while turning.
constexpr float kMinLookAtDistance = 0.1f;
// Each wheel step while flying multiplies or divides the speed by this.
constexpr float kWheelSpeedStep = 1.2f;
// Outside a flight a wheel step moves this fraction of a second of flight.
constexpr float kWheelMoveSeconds = 0.1f;
constexpr float kPanSpeed = 0.01f;
// Looking straight up or down would flip the view, because world Z is always up.
constexpr float kMaxPitchDegrees = 89.0f;
// Keeps focus from putting the camera inside a point-sized object.
constexpr float kMinFocusRadius = 0.1f;
// The 2D camera stands this far in front of the plane, so sprites with a depth (world X)
// within it on either side are seen.
constexpr float k2DDistance = 1000.0f;
constexpr float k2DNearPlane = 0.1f;
// Focus leaves this much room around what it frames in 2D.
constexpr float k2DFocusMargin = 1.2f;
// Coming back to 3D without a 3D view, the camera steps back this far.
constexpr float k3DFallbackDistance = 10.0f;

/*
 */
void
saveSetting(StringView key, float value)
{
  ANSICHAR buffer[StringUtils::MAX_FLOAT_CHARS];
  EngineConfig::setUserValue("Editor", "Editor", key, StringUtils::toChars(buffer, value));
}
} // namespace

/*
 */
EditorCamera::EditorCamera(float viewportWidth, float viewportHeight)
 : m_camera(kStartPosition, Vector3::ZERO, viewportWidth, viewportHeight)
{
  m_camera.setProjectionType(CameraProjectionType::Perspective);
  // A config file may hold values out of range: the camera gets them clamped, and the
  // variables keep what the file says until the user edits them.
  applyProjection(g_cvarCameraFieldOfView.get(), g_cvarCameraNearPlane.get(),
                  g_cvarCameraFarPlane.get());

  // Never chosen by the user: a 2D game starts in 2D.
  bool b2D = g_cvarViewport2D.get();
  if (g_cvarViewport2D.getSource() == ConsoleVariableSource::Default) {
    const Optional<RenderPathType> path =
        RenderSettingsUtils::renderPathFromName(g_cvarRenderPath.get());
    b2D = path && *path == RenderPathType::Sprite2D;
  }
  if (b2D) {
    m_b2D = true;
    m_position3D = m_camera.getPosition();
    m_lookAt3D = m_camera.getLookAt();
    m_bHasView3D = true;
    m_camera.setProjectionType(CameraProjectionType::Orthographic);
    m_camera.setOrthographicSize(Math::clamp(g_cvarCamera2DHalfHeight.get(),
                                             MIN_2D_HALF_HEIGHT, MAX_2D_HALF_HEIGHT));
    m_camera.setClipPlanes(k2DNearPlane, k2DDistance * 2.0f);
    place2D(0.0f, 0.0f);
  }
}

/*
 */
void
EditorCamera::update(float deltaTime)
{
  if (m_b2D) {
    update2D();
    return;
  }
  const InputState& input = InputManager::instance().getState();
  const bool bRightButton = input.isDown(InputKey::MouseRight);
  bool bFlightStarted = false;
  if (!m_bFlying && bRightButton && m_bFocused) {
    m_bFlying = true;
    bFlightStarted = true;
  }
  else if (m_bFlying && !bRightButton) {
    m_bFlying = false;
    if (m_bSpeedChanged) {
      m_bSpeedChanged = false;
      saveSettings();
    }
  }

  const float wheelSteps = input.getMouseWheel().y;
  if (wheelSteps != 0.0f) {
    if (m_bFlying) {
      setMoveSpeed(getMoveSpeed() * Math::pow(kWheelSpeedStep, wheelSteps));
      m_bSpeedChanged = true;
    }
    else if (m_bFocused) {
      m_camera.moveForward(wheelSteps * getMoveSpeed() * kWheelMoveSeconds);
    }
  }

  const Vector2& mouseDelta = input.getMouseDelta();
  if (m_bFlying) {
    // The click that starts the flight must not turn the camera.
    if (!bFlightStarted) {
      const float sensitivity = getLookSensitivity();
      // Moving the mouse down looks down.
      look(mouseDelta.x * sensitivity, -mouseDelta.y * sensitivity);
    }
    fly(deltaTime);
  }
  else if (m_bFocused && input.isDown(InputKey::MouseMiddle)) {
    m_camera.pan(-mouseDelta.x * kPanSpeed, -mouseDelta.y * kPanSpeed);
  }
}

/*
 * Panning moves the view so the point under the mouse follows it.
 */
void
EditorCamera::update2D()
{
  if (!m_bFocused) {
    return;
  }
  const InputState& input = InputManager::instance().getState();
  const float halfHeight = m_camera.getOrthographicSize();
  const float viewportHeight = Math::max(m_camera.getViewportHeight(), 1.0f);
  const float unitsPerPixel = halfHeight * 2.0f / viewportHeight;

  if (input.isDown(InputKey::MouseRight) || input.isDown(InputKey::MouseMiddle)) {
    const Vector2& mouseDelta = input.getMouseDelta();
    if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f) {
      const Vector3& lookAt = m_camera.getLookAt();
      // Screen y grows downward, world Z upward.
      place2D(lookAt.y - mouseDelta.x * unitsPerPixel, lookAt.z + mouseDelta.y * unitsPerPixel);
    }
  }

  const float wheelSteps = input.getMouseWheel().y;
  if (wheelSteps == 0.0f) {
    return;
  }
  const float newHalfHeight =
      Math::clamp(halfHeight * Math::pow(Math::max(g_cvarCamera2DZoomStep.get(), 1.001f),
                                         -wheelSteps),
                  MIN_2D_HALF_HEIGHT, MAX_2D_HALF_HEIGHT);
  // The point under the cursor stays under it.
  const float halfWidth = halfHeight * m_camera.getAspectRatio();
  const Vector3& lookAt = m_camera.getLookAt();
  const float cursorY = lookAt.y + (m_cursor.x * 2.0f - 1.0f) * halfWidth;
  const float cursorZ = lookAt.z + (1.0f - m_cursor.y * 2.0f) * halfHeight;
  const float ratio = newHalfHeight / halfHeight;
  m_camera.setOrthographicSize(newHalfHeight);
  place2D(cursorY - (cursorY - lookAt.y) * ratio, cursorZ - (cursorZ - lookAt.z) * ratio);
  g_cvarCamera2DHalfHeight.set(newHalfHeight, ConsoleVariableSource::Console);
}

/*
 */
void
EditorCamera::place2D(const float y, const float z)
{
  m_camera.setPosition(Vector3(-k2DDistance, y, z));
  m_camera.lookAt(Vector3(0.0f, y, z));
}

/*
 */
void
EditorCamera::set2D(const bool b2D)
{
  g_cvarViewport2D.set(b2D, ConsoleVariableSource::Console);
  EngineConfig::setUserValue("Editor", "Editor", "Viewport2D", b2D ? "true" : "false");
  if (b2D == m_b2D) {
    return;
  }
  m_b2D = b2D;
  m_bFlying = false;

  if (b2D) {
    const Vector3 lookAt = m_camera.getLookAt();
    m_position3D = m_camera.getPosition();
    m_lookAt3D = lookAt;
    m_bHasView3D = true;
    m_camera.setProjectionType(CameraProjectionType::Orthographic);
    m_camera.setOrthographicSize(Math::clamp(g_cvarCamera2DHalfHeight.get(),
                                             MIN_2D_HALF_HEIGHT, MAX_2D_HALF_HEIGHT));
    m_camera.setClipPlanes(k2DNearPlane, k2DDistance * 2.0f);
    place2D(lookAt.y, lookAt.z);
    return;
  }

  saveSetting("Camera2DHalfHeight", m_camera.getOrthographicSize());
  const Vector3 lookAt = m_camera.getLookAt();
  m_camera.setProjectionType(CameraProjectionType::Perspective);
  applyProjection(g_cvarCameraFieldOfView.get(), g_cvarCameraNearPlane.get(),
                  g_cvarCameraFarPlane.get());
  if (m_bHasView3D) {
    m_camera.setPosition(m_position3D);
    m_camera.lookAt(m_lookAt3D);
  }
  else {
    m_camera.setPosition(lookAt - Vector3::FORWARD * k3DFallbackDistance);
    m_camera.lookAt(lookAt);
  }
}

/*
 */
void
EditorCamera::look(float yawDegrees, float pitchDegrees)
{
  if (yawDegrees == 0.0f && pitchDegrees == 0.0f) {
    return;
  }

  // Turns around the camera itself; the look at point keeps its distance, so focus and
  // panning still work around it afterwards.
  const Vector3& position = m_camera.getPosition();
  const Vector3 toLookAt = m_camera.getLookAt() - position;
  const float distance = Math::max(toLookAt.magnitude(), kMinLookAtDistance);
  const Vector3 forward = toLookAt * (1.0f / distance);

  const float yaw = Math::atan2(forward.y, forward.x).valueDegree() + yawDegrees;
  const float pitch = Math::clamp(Math::asin(forward.z).valueDegree() + pitchDegrees,
                                  -kMaxPitchDegrees, kMaxPitchDegrees);
  const float cosPitch = Math::cos(Degree(pitch));
  const Vector3 newForward(cosPitch * Math::cos(Degree(yaw)),
                           cosPitch * Math::sin(Degree(yaw)), Math::sin(Degree(pitch)));
  m_camera.lookAt(position + newForward * distance);
}

/*
 */
void
EditorCamera::fly(float deltaTime)
{
  const InputState& input = InputManager::instance().getState();
  const auto axis = [](bool bPositive, bool bNegative) {
    return (bPositive ? 1.0f : 0.0f) - (bNegative ? 1.0f : 0.0f);
  };
  const float forwardAxis = axis(input.isDown(InputKey::W), input.isDown(InputKey::S));
  const float rightAxis = axis(input.isDown(InputKey::D), input.isDown(InputKey::A));
  const float upAxis = axis(input.isDown(InputKey::E), input.isDown(InputKey::Q));
  if (forwardAxis == 0.0f && rightAxis == 0.0f && upAxis == 0.0f) {
    return;
  }

  // Forward follows the view, pitch included, so looking up and pressing W climbs. E and Q
  // stay on the world's up axis. The camera never rolls, so its right is level.
  Vector3 direction = m_camera.getForwardVector() * forwardAxis
                      + m_camera.getRightVector() * rightAxis + Vector3::UP * upAxis;
  // Normalized so a diagonal is not faster than a single key.
  if (!direction.normalize()) {
    return;
  }
  const bool bSprint = input.isDown(InputKey::LeftShift) || input.isDown(InputKey::RightShift);
  const float distance = getMoveSpeed() * deltaTime * (bSprint ? getSprintMultiplier() : 1.0f);
  const Vector3 offset = direction * distance;

  m_camera.setPosition(m_camera.getPosition() + offset);
  m_camera.lookAt(m_camera.getLookAt() + offset);
}

/*
 */
void
EditorCamera::setViewportSize(float width, float height)
{
  m_camera.setViewportSize(width, height);
}

/*
 */
void
EditorCamera::focus(const AABox& bounds)
{
  if (m_b2D) {
    const Vector3 center = bounds.getCenter();
    const Vector3 extent = bounds.getExtent();
    const float halfHeight =
        Math::max(extent.z, extent.y / Math::max(m_camera.getAspectRatio(), 0.01f)) *
        k2DFocusMargin;
    m_camera.setOrthographicSize(
        Math::clamp(halfHeight, MIN_2D_HALF_HEIGHT, MAX_2D_HALF_HEIGHT));
    g_cvarCamera2DHalfHeight.set(m_camera.getOrthographicSize(),
                                 ConsoleVariableSource::Console);
    place2D(center.y, center.z);
    return;
  }
  // The field of view is horizontal, so on a wide viewport the vertical one decides how
  // far the camera must be for the sphere around the bounds to fit.
  const Radian halfHorizontal = m_camera.getFieldOfView() * 0.5f;
  const Radian halfVertical =
      Math::atan(Math::tan(halfHorizontal) / m_camera.getAspectRatio());
  const Radian halfAngle = halfVertical < halfHorizontal ? halfVertical : halfHorizontal;

  const float radius = Math::max(bounds.getExtent().magnitude(), kMinFocusRadius);
  const float distance = radius / Math::sin(halfAngle);
  const Vector3 center = bounds.getCenter();
  m_camera.setPosition(center - m_camera.getForwardVector() * distance);
  m_camera.lookAt(center);
}

/*
 */
void
EditorCamera::setView(const Vector3& position, const Vector3& lookAt)
{
  if (m_b2D) {
    m_position3D = position;
    m_lookAt3D = lookAt;
    m_bHasView3D = true;
    place2D(lookAt.y, lookAt.z);
    return;
  }
  m_camera.setPosition(position);
  m_camera.lookAt(lookAt);
}

/*
 */
float
EditorCamera::getMoveSpeed() const
{
  return g_cvarCameraSpeed.get();
}

/*
 */
void
EditorCamera::setMoveSpeed(float speed)
{
  g_cvarCameraSpeed.set(Math::clamp(speed, MIN_MOVE_SPEED, MAX_MOVE_SPEED),
                        ConsoleVariableSource::Console);
}

/*
 */
float
EditorCamera::getLookSensitivity() const
{
  return g_cvarCameraLookSensitivity.get();
}

/*
 */
void
EditorCamera::setLookSensitivity(float degreesPerPixel)
{
  g_cvarCameraLookSensitivity.set(degreesPerPixel, ConsoleVariableSource::Console);
}

/*
 */
float
EditorCamera::getSprintMultiplier() const
{
  return g_cvarCameraSprintMultiplier.get();
}

/*
 */
void
EditorCamera::setSprintMultiplier(float multiplier)
{
  g_cvarCameraSprintMultiplier.set(multiplier, ConsoleVariableSource::Console);
}

/*
 */
float
EditorCamera::getFieldOfView() const
{
  return m_b2D ? g_cvarCameraFieldOfView.get() : m_camera.getFieldOfView().valueDegree();
}

/*
 */
void
EditorCamera::setFieldOfView(float degrees)
{
  g_cvarCameraFieldOfView.set(
      Math::clamp(degrees, MIN_FIELD_OF_VIEW, MAX_FIELD_OF_VIEW), ConsoleVariableSource::Console);
  if (!m_b2D) {
    applyProjection(degrees, getNearPlane(), getFarPlane());
  }
}

/*
 */
float
EditorCamera::getNearPlane() const
{
  return m_b2D ? g_cvarCameraNearPlane.get() : m_camera.getNearClipPlane();
}

/*
 */
float
EditorCamera::getFarPlane() const
{
  return m_b2D ? g_cvarCameraFarPlane.get() : m_camera.getFarClipPlane();
}

/*
 * The 2D view keeps its own clip planes; the ones set here wait for the 3D view.
 */
void
EditorCamera::setClipPlanes(float nearPlane, float farPlane)
{
  const float nearClip =
      Math::clamp(nearPlane, MIN_NEAR_PLANE, MAX_FAR_PLANE / MIN_FAR_TO_NEAR_RATIO);
  const float farClip = Math::clamp(farPlane, nearClip * MIN_FAR_TO_NEAR_RATIO, MAX_FAR_PLANE);
  g_cvarCameraNearPlane.set(nearClip, ConsoleVariableSource::Console);
  g_cvarCameraFarPlane.set(farClip, ConsoleVariableSource::Console);
  if (!m_b2D) {
    applyProjection(getFieldOfView(), nearClip, farClip);
  }
}

/*
 */
void
EditorCamera::applyProjection(float fieldOfViewDegrees, float nearPlane, float farPlane)
{
  const float fieldOfView =
      Math::clamp(fieldOfViewDegrees, MIN_FIELD_OF_VIEW, MAX_FIELD_OF_VIEW);
  const float nearClip =
      Math::clamp(nearPlane, MIN_NEAR_PLANE, MAX_FAR_PLANE / MIN_FAR_TO_NEAR_RATIO);
  const float farClip = Math::clamp(farPlane, nearClip * MIN_FAR_TO_NEAR_RATIO, MAX_FAR_PLANE);
  m_camera.setFieldOfView(Radian(Degree(fieldOfView)));
  m_camera.setClipPlanes(nearClip, farClip);
}

/*
 */
void
EditorCamera::resetSettings()
{
  setMoveSpeed(g_cvarCameraSpeed.getDefault());
  setLookSensitivity(g_cvarCameraLookSensitivity.getDefault());
  setSprintMultiplier(g_cvarCameraSprintMultiplier.getDefault());
  setFieldOfView(g_cvarCameraFieldOfView.getDefault());
  setClipPlanes(g_cvarCameraNearPlane.getDefault(), g_cvarCameraFarPlane.getDefault());
  saveSettings();
}

/*
 */
void
EditorCamera::saveSettings() const
{
  saveSetting("CameraSpeed", getMoveSpeed());
  saveSetting("CameraLookSensitivity", getLookSensitivity());
  saveSetting("CameraSprintMultiplier", getSprintMultiplier());
  saveSetting("CameraFieldOfView", getFieldOfView());
  saveSetting("CameraNearPlane", getNearPlane());
  saveSetting("CameraFarPlane", getFarPlane());
  if (m_b2D) {
    saveSetting("Camera2DHalfHeight", m_camera.getOrthographicSize());
  }
}

} // namespace chEngineSDK
