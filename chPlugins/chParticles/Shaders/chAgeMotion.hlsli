// Motion by age, shared by the plugin's shaders: a thing is born with a velocity and a
// lifetime (AgeBirth) and everything after is computed from its age, so the CPU never
// touches it again.
#ifndef CH_AGE_MOTION_HLSLI
#define CH_AGE_MOTION_HLSLI

static const float kTwoPi = 6.28318530718f;

// Must match GpuAgeMotion in chAgeMotion.h.
static const uint kAgeMotionSize = 32;

struct AgeMotion
{
  float2 gravity;
  float drag;
  float fadeIn;
  float fadeOut;
};

template<typename Buffer>
AgeMotion
loadAgeMotion(Buffer buffer, uint address)
{
  const float4 first = asfloat(buffer.Load4(address));
  AgeMotion motion;
  motion.gravity = first.xy;
  motion.drag = first.z;
  motion.fadeIn = first.w;
  motion.fadeOut = asfloat(buffer.Load(address + 16));
  return motion;
}

// Where it is after age seconds: an arc along its velocity, slowed by drag, plus gravity.
float2
getAgeOffset(AgeMotion motion, float2 velocity, float age)
{
  const float travel =
      motion.drag > 0.0f ? (1.0f - exp(-motion.drag * age)) / motion.drag : age;
  return velocity * travel + 0.5f * motion.gravity * age * age;
}

// 0 to 1: fading in after birth and out before its lifetime ends.
float
getAgeFade(AgeMotion motion, float age, float lifetime)
{
  return (motion.fadeIn > 0.0f ? saturate(age / motion.fadeIn) : 1.0f) *
         (motion.fadeOut > 0.0f ? saturate((lifetime - age) / motion.fadeOut) : 1.0f);
}

// Born at popScale times its size, settling to 1 over duration with a cubic ease out.
float
getAgePop(float age, float popScale, float duration)
{
  const float pop = duration > 0.0f ? saturate(age / duration) : 1.0f;
  const float settle = 1.0f - pop;
  return lerp(1.0f, popScale, settle * settle * settle);
}

// A 16-bit seed as an angle, so things born together do not move together.
float
getSeedPhase(uint seed)
{
  return (float)(seed & 0xFFFFu) * (kTwoPi / 65536.0f);
}

// A wobble of amplitude world units fading to none over duration.
float2
getAgeShake(float age, float amplitude, float frequency, float duration, float phase)
{
  if (age >= duration || duration <= 0.0f) {
    return float2(0.0f, 0.0f);
  }
  const float strength = amplitude * (1.0f - age / duration);
  const float angle = age * frequency * kTwoPi + phase;
  return strength * float2(sin(angle), cos(angle * 1.37f + phase));
}

#endif // CH_AGE_MOTION_HLSLI
