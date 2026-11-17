/************************************************************************/
/**
 * @file chAgeMotion.h
 * @author AccelMR
 * @date 2026/11/16
 * @brief
 *  How things the plugin's systems spawn move and fade by their age.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chRandom.h"
#include "chVector2.h"

namespace chEngineSDK {

/**
 * The header of a system's motion buffer: what chAgeMotion.hlsli reads to move a spawned
 * thing by its age. 32 bytes; must match loadAgeMotion.
 */
struct GpuAgeMotion
{
  // World units per second squared, Y then Z.
  float gravity[2];
  // Per second; 0 keeps the velocity.
  float drag;
  // Seconds to fade in and out.
  float fadeIn;
  float fadeOut;
  float padding[3];
};
static_assert(sizeof(GpuAgeMotion) == 32, "GpuAgeMotion must match chAgeMotion.hlsli");

/**
 * What one spawn gets from AgeMotion::makeBirth: the rest is computed from its age.
 */
struct AgeBirth
{
  // World units per second, Y then Z.
  Vector2 velocity;
  float lifetime;
  // 16 bits, so motions that shake or pick at random look different per spawn.
  uint32 seed;

  /**
   * The velocity as two halves (Y low, Z high), the way the shaders unpack it.
   */
  NODISCARD uint32
  getPackedVelocity() const noexcept;
};

/**
 * Exists so floating numbers and particles move the same way and are set with the same
 * values: picked once when a thing is born (a direction within a spread, a speed and a
 * lifetime with their variance), then the GPU moves it along an arc under gravity, slowed
 * by drag, and fades it by its age alone. Angles in degrees, counterclockwise from +Y, so
 * 90 is up; world units and seconds.
 */
struct AgeMotion
{
  NODISCARD AgeMotion
  getClamped() const noexcept;

  NODISCARD AgeBirth
  makeBirth(const Random& random) const noexcept;

  NODISCARD GpuAgeMotion
  toGpu() const noexcept;

  static constexpr float MIN_LIFETIME = 0.05f;

  // Where things fly, and how far to each side they may leave it.
  float angle = 90.0f;
  float spread = 40.0f;
  float speed = 4.0f;
  float speedVariance = 1.5f;
  Vector2 gravity{0.0f, -9.0f};
  // Per second; 0 keeps the speed.
  float drag = 0.0f;
  float lifetime = 1.0f;
  float lifetimeVariance = 0.2f;
  float fadeIn = 0.05f;
  float fadeOut = 0.3f;
};

} // namespace chEngineSDK
