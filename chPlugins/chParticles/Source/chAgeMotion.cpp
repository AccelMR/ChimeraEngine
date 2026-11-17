/************************************************************************/
/**
 * @file chAgeMotion.cpp
 * @author AccelMR
 * @date 2026/11/16
 * @brief
 *  How things the plugin's systems spawn move and fade by their age.
 */
/************************************************************************/
#include "chAgeMotion.h"

#include "chAngle.h"
#include "chFloat16.h"
#include "chMath.h"

namespace chEngineSDK {

namespace {
/*
 * Between -1 and 1.
 */
NODISCARD float
signedRandom(const Random& random)
{
  return random.getRandom01() * 2.0f - 1.0f;
}
} // namespace

/*
 */
uint32
AgeBirth::getPackedVelocity() const noexcept
{
  return static_cast<uint32>(Float16::fromFloat(velocity.x)) |
         (static_cast<uint32>(Float16::fromFloat(velocity.y)) << 16);
}

/*
 */
AgeMotion
AgeMotion::getClamped() const noexcept
{
  AgeMotion clamped = *this;
  clamped.lifetime = Math::max(lifetime, MIN_LIFETIME);
  clamped.lifetimeVariance = Math::clamp(lifetimeVariance, 0.0f, clamped.lifetime);
  return clamped;
}

/*
 */
AgeBirth
AgeMotion::makeBirth(const Random& random) const noexcept
{
  const float radians = Radian(Degree(angle + signedRandom(random) * spread)).valueRadian();
  const float birthSpeed = speed + signedRandom(random) * speedVariance;
  float sine = 0.0f;
  float cosine = 1.0f;
  Math::sinCos(radians, sine, cosine);
  AgeBirth birth;
  birth.velocity = Vector2(cosine * birthSpeed, sine * birthSpeed);
  birth.lifetime =
      Math::max(lifetime + signedRandom(random) * lifetimeVariance, MIN_LIFETIME);
  birth.seed = random.getPseudoRandom() >> 16;
  return birth;
}

/*
 */
GpuAgeMotion
AgeMotion::toGpu() const noexcept
{
  GpuAgeMotion gpu{};
  gpu.gravity[0] = gravity.x;
  gpu.gravity[1] = gravity.y;
  gpu.drag = Math::max(drag, 0.0f);
  gpu.fadeIn = Math::max(fadeIn, 0.0f);
  gpu.fadeOut = Math::max(fadeOut, 0.0f);
  return gpu;
}

} // namespace chEngineSDK
