/************************************************************************/
/**
 * @file chParticleGpu.h
 * @author AccelMR
 * @date 2026/11/16
 * @brief
 *  What particles.hlsl reads to move and draw the particles.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chAgeMotion.h"

namespace chEngineSDK {
class SpriteShader;

/**
 * One particle, written once when it is born. 32 bytes; must match particles.hlsl.
 */
struct GpuParticle
{
  // Y and Z where it was born: in the world, or from the emitter in local space.
  float position[2];
  // On the system's clock, in seconds.
  float birthTime;
  float lifetime;
  // World units per second as halves, Y low and Z high.
  uint32 velocity;
  // Picks its size, rotation, spin and color between the look's ranges.
  uint32 seed;
  // Times its color, as halves.
  uint32 tintRG;
  uint32 tintBA;
};
static_assert(sizeof(GpuParticle) == 32, "GpuParticle must match particles.hlsl");

/**
 * Exists so the GPU moves and draws the particles of a system from their age alone: the
 * shared AgeMotion, the look (size, rotation, colors between which each particle picks),
 * the color gradient and size curve over the life and the flipbook frames. A storage
 * buffer; must match particles.hlsl.
 */
struct GpuParticleLook
{
  static constexpr uint32 MAX_KEYS = 8;
  static constexpr uint32 MAX_FRAMES = 64;
  static constexpr uint32 FACE_VELOCITY_BIT = 0x1u;

  GpuAgeMotion motion;
  uint32 textureAndFlags;
  uint32 frameCount;
  // 0 spreads the frames over the lifetime.
  float framesPerSecond;
  uint32 flags;
  // World units, times a scale between minScale and maxScale and the size curve.
  float size[2];
  float minScale;
  float maxScale;
  // Radians, and radians per second.
  float rotation;
  float rotationVariance;
  float spin;
  float spinVariance;
  // Each particle picks a color between these two.
  float color[4];
  float color2[4];
  uint32 colorKeyCount;
  uint32 sizeKeyCount;
  // World X of every particle.
  float depth;
  uint32 padding;
  // Times from 0 (birth) to 1 (death), rising.
  float colorTimes[MAX_KEYS];
  float colorKeys[MAX_KEYS][4];
  float sizeTimes[MAX_KEYS];
  float sizeKeys[MAX_KEYS];
  // As GpuSpriteInstance::uvRect; without a flipbook, frame 0 is the whole look.
  float frames[MAX_FRAMES][4];
};
static_assert(sizeof(GpuParticleLook) == 352 + 16 * GpuParticleLook::MAX_FRAMES,
              "GpuParticleLook must match particles.hlsl");

/**
 * Push constants of particles.hlsl after the engine's header
 * (CustomSpriteBatch::parameters). In local space the emitter's place and turn move every
 * particle; in world space they are none.
 */
struct ParticleBatchParameters
{
  uint32 particleBufferIndex;
  uint32 firstParticle;
  uint32 lookIndex;
  // Now, on the clock the particles' birth times were written with, in seconds.
  float time;
  float emitterPosition[2];
  // Cosine and sine of the emitter's turn.
  float emitterRotation[2];
};

/**
 * The particles' shader.
 */
class ParticleGpu
{
 public:
  /**
   * Made on first use; its GPU objects are freed by SpriteShader::releaseAll.
   */
  NODISCARD static SpriteShader&
  getShader();
};

} // namespace chEngineSDK
