/************************************************************************/
/**
 * @file chParticleSystemComponent.h
 * @author AccelMR
 * @date 2026/11/16
 * @brief
 *  Particles written once when they are born and moved, colored and sized by the GPU.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chAgeMotion.h"
#include "chComponent.h"
#include "chGpuAgeRing.h"
#include "chKeyCurve.h"
#include "chLinearColor.h"
#include "chParticleGpu.h"
#include "chRandom.h"
#include "chSpriteItem.h"
#include "chTypeTraits.h"
#include "chVector2.h"

namespace chEngineSDK {
class IBuffer;
class SpriteAtlasAsset;
class TextureAsset;

/**
 * Where particles are born around the emitter, before its turn and scale.
 */
enum class ParticleShape : uint8 {
  Point = 0,
  // Anywhere inside a circle of radius shapeSize.x.
  Circle = 1,
  // On the edge of that circle.
  CircleEdge = 2,
  // Inside a rectangle of half size shapeSize.
  Rectangle = 3,
  // Along a segment of half length shapeSize.x on the emitter's right axis.
  Line = 4,
  COUNT
};

enum class ParticleRotation : uint8 {
  // rotation plus or minus rotationVariance, turning at spin plus or minus spinVariance.
  Random = 0,
  // The image's right side points where the particle flies.
  FaceVelocity = 1,
  COUNT
};

/**
 * When and where a system makes particles by itself while it plays.
 */
struct ParticleEmission
{
  // Particles per second.
  float rate = 20.0f;
  // Seconds of one cycle: bursts happen within it, and it starts again when it loops.
  float duration = 2.0f;
  bool bLoop = true;
  // Plays when its scene starts playing.
  bool bPlayOnStart = true;
  // Particles follow the emitter after they are born instead of staying where they were.
  bool bLocalSpace = false;
  ParticleShape shape = ParticleShape::Point;
  Vector2 shapeSize{0.5f, 0.5f};
};

/**
 * count particles at time seconds into each cycle, cycles times, interval seconds apart.
 */
struct ParticleBurst
{
  float time = 0.0f;
  uint32 count = 10;
  uint32 cycles = 1;
  float interval = 0.5f;
};

/**
 * What every particle of a system looks like. Each one picks its scale, rotation, spin and
 * color between the ranges when it is born.
 */
struct ParticleLook
{
  UUID texture = UUID::null();
  // As GpuSpriteInstance::uvRect.
  float uvRect[4] = {0.0f, 0.0f, 1.0f, 1.0f};
  // World units.
  Vector2 size{0.2f, 0.2f};
  float minScale = 1.0f;
  float maxScale = 1.0f;
  LinearColor color = LinearColor::WHITE;
  LinearColor color2 = LinearColor::WHITE;
  ParticleRotation rotationMode = ParticleRotation::Random;
  // Degrees, and degrees per second.
  float rotation = 0.0f;
  float rotationVariance = 0.0f;
  float spin = 0.0f;
  float spinVariance = 0.0f;
  // World X of every particle.
  float depth = 0.0f;
  int16 sortingLayer = 0;
  int16 order = 0;
  SpriteBlendMode blendMode = SpriteBlendMode::Alpha;
  SpriteFilter filter = SpriteFilter::Linear;
};

/**
 * Frames of an atlas played over each particle's life; its texture replaces the look's.
 */
struct ParticleFlipbook
{
  UUID atlas = UUID::null();
  uint32 firstFrame = 0;
  // 0 takes every frame from firstFrame on.
  uint32 frameCount = 0;
  // 0 spreads the frames over the lifetime.
  float framesPerSecond = 0.0f;
};

/**
 * Exists so a game can show many particles (sparks, smoke, magic) for little: a particle
 * is written once when it is born, into a GPU ring of up to `getCapacity()`, and the GPU
 * moves it by its age along the shared AgeMotion, colors it along a gradient, sizes it
 * along a curve and plays the flipbook frames. While it plays the system makes particles
 * at a rate and in bursts, on a shape around its object; scripts also emit them where they
 * want. In local space they follow the object after they are born.
 *
 * Costs: a particle costs a few random numbers and a 32-byte write when born; a frame is
 * one upload of the new particles and one or two draws, whatever the count.
 */
class ParticleSystemComponent : public Component
{
 public:
  ParticleSystemComponent();

  ~ParticleSystemComponent() override;

  /**
   * count particles on the emission shape. Returns how many were made (none outside a
   * scene).
   */
  uint32
  emit(uint32 count);

  /**
   * count particles at position (world Y and Z), their colors times tint.
   */
  uint32
  emitAt(uint32 count, const Vector2& position, const LinearColor& tint = LinearColor::WHITE);

  /**
   * Starts a cycle of the emission (rate and bursts).
   */
  void
  play();

  /**
   * Ends the emission; the particles alive keep flying unless bClear.
   */
  void
  stop(bool bClear = false);

  NODISCARD FORCEINLINE bool
  isPlaying() const noexcept { return m_bPlaying; }

  /**
   * Forgets every particle shown.
   */
  void
  clear();

  /**
   * Particles that may still live.
   */
  NODISCARD FORCEINLINE uint32
  getLiveCount() const noexcept { return m_ring.getLiveCount(); }

  NODISCARD FORCEINLINE uint32
  getCapacity() const noexcept { return m_capacity; }

  /**
   * At most MAX_CAPACITY. Clears the particles.
   */
  void
  setCapacity(uint32 capacity);

  void
  setEmission(const ParticleEmission& emission);

  NODISCARD FORCEINLINE const ParticleEmission&
  getEmission() const noexcept { return m_emission; }

  /**
   * Up to MAX_BURSTS.
   */
  void
  setBursts(Span<const ParticleBurst> bursts);

  NODISCARD FORCEINLINE Span<const ParticleBurst>
  getBursts() const noexcept { return m_bursts; }

  void
  setMotion(const AgeMotion& motion);

  NODISCARD FORCEINLINE const AgeMotion&
  getMotion() const noexcept { return m_motion; }

  void
  setLook(const ParticleLook& look);

  NODISCARD FORCEINLINE const ParticleLook&
  getLook() const noexcept { return m_look; }

  void
  setTextureId(const UUID& texture);

  NODISCARD FORCEINLINE const UUID&
  getTextureId() const noexcept { return m_look.texture; }

  void
  setFlipbook(const ParticleFlipbook& flipbook);

  NODISCARD FORCEINLINE const ParticleFlipbook&
  getFlipbook() const noexcept { return m_flipbook; }

  /**
   * Up to GpuParticleLook::MAX_KEYS, kept sorted by time; none is white all its life.
   */
  void
  setColorGradient(Span<const GradientKey> keys);

  NODISCARD FORCEINLINE Span<const GradientKey>
  getColorGradient() const noexcept { return m_colorKeys; }

  /**
   * Up to GpuParticleLook::MAX_KEYS, kept sorted by time; none is 1 all its life.
   */
  void
  setSizeCurve(Span<const CurveKey> keys);

  NODISCARD FORCEINLINE Span<const CurveKey>
  getSizeCurve() const noexcept { return m_sizeKeys; }

  /**
   * Seconds on the system's own clock, which only moves while it updates.
   */
  NODISCARD FORCEINLINE float
  getTime() const noexcept { return m_ring.getTime(); }

  /**
   * The particles as last written, for tests and tools.
   */
  NODISCARD FORCEINLINE Span<const GpuParticle>
  getParticles() const noexcept { return m_ring.getEntries(); }

  /**
   * The look the GPU reads, as last built.
   */
  NODISCARD FORCEINLINE const GpuParticleLook&
  getGpuLook() const noexcept { return m_gpuLook; }

#if USING(CH_EDITOR)
  /**
   * The Inspector's Test Emit: TEST_EMIT_COUNT particles on the shape.
   */
  void
  testEmit();

  /**
   * The Inspector's Stop: ends the emission and clears the particles.
   */
  void
  stopAndClear();

  static constexpr uint32 TEST_EMIT_COUNT = 50;
#endif // USING(CH_EDITOR)

  void
  update(float deltaTime) override;

  void
  serialize(DataStream& stream) const override;

  NODISCARD bool
  deserialize(DataStream& stream) override;

  void
  collectReferences(Vector<UUID>& outReferences) const override;

  static constexpr uint32 SERIALIZATION_VERSION = 1;
  static constexpr uint32 DEFAULT_CAPACITY = 4096;
  static constexpr uint32 MAX_CAPACITY = 1u << 20;
  static constexpr uint32 MAX_BURSTS = 8;

 protected:
  void
  onRegister() override;

  void
  onUnregister() override;

 private:
  /**
   * Where the emitter stands this frame, in the YZ plane.
   */
  struct EmitterPlace
  {
    Vector2 position;
    // Cosine and sine of its turn.
    Vector2 rotation;
    float scale;
  };

  NODISCARD EmitterPlace
  getEmitterPlace() const;

  NODISCARD Vector2
  pickShapePoint();

  /**
   * One particle at position from the emitter, before its turn; kept from the emitter in
   * local space, placed in the world otherwise.
   */
  void
  writeParticle(const Vector2& position, const EmitterPlace& place, const LinearColor& tint);

  void
  emitOverTime(float deltaTime);

  void
  emitBursts(float from, float to);

  void
  resolveAssets();

  void
  rebuildGpuLook();

  void
  addBatch(uint32 first, uint32 count, const EmitterPlace& place);

  ParticleEmission m_emission;
  Vector<ParticleBurst> m_bursts;
  AgeMotion m_motion;
  ParticleLook m_look;
  ParticleFlipbook m_flipbook;
  Vector<GradientKey> m_colorKeys;
  Vector<CurveKey> m_sizeKeys;
  uint32 m_capacity = DEFAULT_CAPACITY;

  GpuAgeRing<GpuParticle> m_ring;
  Random m_random{0x9A27C1E5u};
  bool m_bPlaying = false;
  // Play on start runs once per registration, when the scene first plays.
  bool m_bStartChecked = false;
  float m_cycleTime = 0.0f;
  float m_rateDebt = 0.0f;

  SPtr<TextureAsset> m_texture;
  SPtr<SpriteAtlasAsset> m_atlas;
  GpuParticleLook m_gpuLook{};
  uint32 m_textureAndFlags = GpuSpriteInstance::NO_TEXTURE;
  bool m_bLookDirty = true;
  SPtr<IBuffer> m_lookBuffer;
};

DECLARE_TYPE_TRAITS(ParticleSystemComponent)

} // namespace chEngineSDK
