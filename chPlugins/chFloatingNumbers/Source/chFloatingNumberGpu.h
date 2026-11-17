/************************************************************************/
/**
 * @file chFloatingNumberGpu.h
 * @author AccelMR
 * @date 2026/11/16
 * @brief
 *  What floatingnumbers.hlsl reads to move the numbers.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

namespace chEngineSDK {
class SpriteShader;

/**
 * How runs of one tier look as they age (see GpuFloatingNumberMotion). 80 bytes; must match
 * floatingnumbers.hlsl.
 */
struct GpuFloatingNumberTier
{
  // Times the run's own color.
  float color[4];
  // The color the run blinks to while it flashes.
  float flashColor[4];
  // The color of outline runs (FloatingNumberGpu::OUTLINE_BIT).
  float outlineColor[4];
  // Times the run's size.
  float scale;
  // The size the run is born at, times scale, shrinking to scale over popDuration.
  float popScale;
  float popDuration;
  // World units the run shakes by, fading to none over shakeDuration.
  float shakeAmplitude;
  // Shakes per second.
  float shakeFrequency;
  float shakeDuration;
  // Seconds it blinks for, and blinks per second.
  float flashDuration;
  float flashRate;
};
static_assert(sizeof(GpuFloatingNumberTier) == 80,
              "GpuFloatingNumberTier must match floatingnumbers.hlsl");

/**
 * Exists so the GPU moves the numbers: each run moves from where it was made along its
 * velocity under gravity, slowed by drag, and pops, shakes, flashes and fades by its age
 * with the look of its tier. A storage buffer; must match floatingnumbers.hlsl.
 */
struct GpuFloatingNumberMotion
{
  static constexpr uint32 MAX_TIERS = 8;

  // World units per second squared, Y then Z.
  float gravity[2];
  // Per second; 0 keeps the velocity.
  float drag;
  // Seconds to fade in and out.
  float fadeIn;
  float fadeOut;
  uint32 tierCount;
  float padding[2];
  GpuFloatingNumberTier tiers[MAX_TIERS];
};
static_assert(sizeof(GpuFloatingNumberMotion) == 32 + 80 * GpuFloatingNumberMotion::MAX_TIERS,
              "GpuFloatingNumberMotion must match floatingnumbers.hlsl");

/**
 * Push constants of floatingnumbers.hlsl after the engine's header
 * (CustomSpriteBatch::parameters).
 */
struct FloatingNumberBatchParameters
{
  uint32 runBufferIndex;
  uint32 firstRun;
  uint32 motionIndex;
  // Now, on the clock the runs' times were written with, in seconds.
  float time;
};

/**
 * The plugin's shader and how a run's GpuGlyphRun::userData is filled: [0] the time the run
 * was made (float), [1] its velocity in world units per second (halves, Y then Z), [2] how
 * long it lives (float), [3] its tier, OUTLINE_BIT and a seed for the shake.
 */
class FloatingNumberGpu
{
 public:
  static constexpr uint32 TIER_MASK = 0xFFu;
  // The run is the outline of the next one: it takes its tier's outline color.
  static constexpr uint32 OUTLINE_BIT = 0x100u;
  static constexpr uint32 SEED_SHIFT = 16;

  /**
   * Made on first use; its GPU objects are freed by SpriteShader::releaseAll.
   */
  NODISCARD static SpriteShader&
  getShader();
};

} // namespace chEngineSDK
