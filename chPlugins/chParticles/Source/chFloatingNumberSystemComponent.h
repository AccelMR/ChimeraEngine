/************************************************************************/
/**
 * @file chFloatingNumberSystemComponent.h
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Damage numbers and other short texts that pop up, fly and fade on the GPU.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chAgeMotion.h"
#include "chComponent.h"
#include "chFloatingNumberGpu.h"
#include "chGpuAgeRing.h"
#include "chLinearColor.h"
#include "chNumberFormat.h"
#include "chRandom.h"
#include "chSpriteItem.h"
#include "chTypeTraits.h"
#include "chVector2.h"

namespace chEngineSDK {
class Font;
class FontAsset;
class IBuffer;

/**
 * How numbers from minValue up look: the highest tier whose minValue is at most the number
 * is used. Sizes are times the system's size, durations in seconds.
 */
struct FloatingNumberTier
{
  double minValue = 0.0;
  LinearColor color = LinearColor::WHITE;
  // Blinked to while flashing.
  LinearColor flashColor = LinearColor::WHITE;
  LinearColor outlineColor = LinearColor::BLACK;
  // SDF fonts only: pixels of the font's raster size; 0 draws no outline.
  float outlineWidth = 0.0f;
  float scale = 1.0f;
  // Born at popScale times scale, settling to scale over popDuration.
  float popScale = 1.0f;
  float popDuration = 0.15f;
  // World units, fading to none over shakeDuration.
  float shakeAmplitude = 0.0f;
  float shakeFrequency = 20.0f;
  float shakeDuration = 0.0f;
  float flashDuration = 0.0f;
  // Blinks per second.
  float flashRate = 8.0f;
};

/**
 * Optional changes of one spawn.
 */
struct FloatingNumberSpawn
{
  static constexpr int32 AUTO_TIER = -1;

  // AUTO_TIER picks the tier by value.
  int32 tier = AUTO_TIER;
  // Times the tier's color.
  LinearColor tint = LinearColor::WHITE;
  // Times the tier's scale.
  float scale = 1.0f;
};

/**
 * Exists so a game can show hundreds of thousands of damage numbers (an incremental game, a
 * bullet hell) without paying for them every frame: a number is formatted and written once,
 * as a glyph run in a GPU ring buffer of up to `getCapacity()` runs, and the GPU moves it
 * along an arc, pops, shakes, flashes and fades it by its age with the look of its tier
 * (GpuFloatingNumberMotion). Each frame only the runs made since the last one are uploaded,
 * and only the part of the ring that can still be alive is drawn. When the ring is full the
 * oldest numbers are written over.
 *
 * Costs: a spawn is one number format and one glyph run (two with an outline), about a
 * tenth of a microsecond; a frame is one small upload and one draw, whatever the count.
 */
class FloatingNumberSystemComponent : public Component
{
 public:
  FloatingNumberSystemComponent();

  ~FloatingNumberSystemComponent() override;

  /**
   * value written with the number format, at (y, z) in world units. Returns false when there
   * is no font or the system is not in a scene.
   */
  bool
  spawn(double value, const Vector2& position, const FloatingNumberSpawn& options = {});

  /**
   * Any short text (up to GpuGlyphRun::MAX_GLYPHS characters), with the given tier (the
   * first without one).
   */
  bool
  spawnText(StringView text, const Vector2& position, const FloatingNumberSpawn& options = {});

  /**
   * Forgets every number shown.
   */
  void
  clear();

  /**
   * The tier a number gets by value.
   */
  NODISCARD uint32
  findTier(double value) const noexcept;

  /**
   * Numbers that may still be alive (made within the longest lifetime).
   */
  NODISCARD uint32
  getLiveCount() const noexcept;

  NODISCARD FORCEINLINE uint32
  getCapacity() const noexcept { return m_capacity; }

  /**
   * Runs in the ring (an outlined number takes two); at most MAX_CAPACITY. Clears it.
   */
  void
  setCapacity(uint32 capacity);

  void
  setFont(const SPtr<FontAsset>& font);

  void
  setFontId(const UUID& fontId);

  NODISCARD FORCEINLINE const UUID&
  getFontId() const noexcept { return m_fontId; }

  /**
   * Height of the font in world units, times each tier's scale.
   */
  void
  setSize(float size);

  NODISCARD FORCEINLINE float
  getSize() const noexcept { return m_size; }

  void
  setSortingLayer(int16 sortingLayer);

  NODISCARD FORCEINLINE int16
  getSortingLayer() const noexcept { return m_sortingLayer; }

  void
  setOrder(int16 order);

  NODISCARD FORCEINLINE int16
  getOrder() const noexcept { return m_order; }

  void
  setBlendMode(SpriteBlendMode blendMode);

  NODISCARD FORCEINLINE SpriteBlendMode
  getBlendMode() const noexcept { return m_blendMode; }

  void
  setMonospaceDigits(bool bMonospace);

  NODISCARD FORCEINLINE bool
  isMonospaceDigits() const noexcept { return m_bMonospaceDigits; }

  void
  setMotion(const AgeMotion& motion);

  NODISCARD FORCEINLINE const AgeMotion&
  getMotion() const noexcept { return m_motion; }

  /**
   * Up to GpuFloatingNumberMotion::MAX_TIERS, kept sorted by minValue. None gives one plain
   * tier.
   */
  void
  setTiers(Span<const FloatingNumberTier> tiers);

  NODISCARD FORCEINLINE Span<const FloatingNumberTier>
  getTiers() const noexcept { return m_tiers; }

  void
  setNumberFormat(const NumberFormatDesc& format);

  NODISCARD FORCEINLINE const NumberFormatDesc&
  getNumberFormat() const noexcept { return m_numberFormat; }

  /**
   * Seconds on the system's own clock, which only moves while it updates.
   */
  NODISCARD FORCEINLINE float
  getTime() const noexcept { return m_ring.getTime(); }

  /**
   * The runs as last written, for tests and tools.
   */
  NODISCARD FORCEINLINE Span<const GpuGlyphRun>
  getRuns() const noexcept { return m_ring.getEntries(); }

  NODISCARD FORCEINLINE uint32
  getWriteCursor() const noexcept { return m_ring.getWriteCursor(); }

  /**
   * The motion the GPU reads, as last built.
   */
  NODISCARD FORCEINLINE const GpuFloatingNumberMotion&
  getGpuMotion() const noexcept { return m_gpuMotion; }

#if USING(CH_EDITOR)
  /**
   * The Inspector's Test Spawn: TEST_SPAWN_COUNT numbers around the owner, of values that
   * reach every tier.
   */
  void
  testSpawn();

  static constexpr uint32 TEST_SPAWN_COUNT = 24;
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
  static constexpr uint32 DEFAULT_CAPACITY = 8192;
  static constexpr uint32 MAX_CAPACITY = 1u << 20;

 protected:
  void
  onRegister() override;

  void
  onUnregister() override;

 private:
  NODISCARD Font*
  getShownFont() const;

  bool
  spawnRuns(StringView text,
            const Vector2& position,
            uint32 tier,
            const FloatingNumberSpawn& options);

  void
  writeRun(const GpuGlyphRun& run, float lifetime);

  void
  rebuildGpuMotion();

  /**
   * Writes the atlas and glyph table indices of the font again into every run, after its
   * atlas changed.
   */
  void
  refreshFontIndices(Font& font);

  void
  addBatch(uint32 first, uint32 count);

  SPtr<FontAsset> m_font;
  UUID m_fontId = UUID::null();
  float m_size = 0.4f;
  int16 m_sortingLayer = 0;
  int16 m_order = 0;
  SpriteBlendMode m_blendMode = SpriteBlendMode::Alpha;
  bool m_bMonospaceDigits = false;
  AgeMotion m_motion;
  Vector<FloatingNumberTier> m_tiers;
  NumberFormatDesc m_numberFormat{};
  uint32 m_capacity = DEFAULT_CAPACITY;

  // Keeps a CPU copy of the runs, so they can be written again when the atlas changes.
  GpuAgeRing<GpuGlyphRun> m_ring;
  // The most glyphs a run written since the ring was last empty has, which is how many
  // quads each run of the batch draws.
  uint32 m_maxGlyphs = 1;
  Random m_random{0x5EED1234u};

  GpuFloatingNumberMotion m_gpuMotion{};
  bool m_bMotionDirty = true;
  SPtr<IBuffer> m_motionBuffer;
  const Font* m_writtenFont = nullptr;
  uint32 m_writtenRevision = 0;
};

DECLARE_TYPE_TRAITS(FloatingNumberSystemComponent)

} // namespace chEngineSDK
