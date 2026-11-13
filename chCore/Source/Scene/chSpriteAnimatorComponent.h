/************************************************************************/
/**
 * @file chSpriteAnimatorComponent.h
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Component that plays the animations of a sprite atlas on its GameObject's sprite.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chComponent.h"
#include "chTypeTraits.h"
#include "chUUID.h"

namespace chEngineSDK {
class SpriteAtlasAsset;

/**
 * Exists to play frame by frame animations: it keeps an atlas, the animation playing and its
 * time, and each update shows the right frame on the SpriteComponent of the same GameObject
 * (only when the frame changes). Which animation plays is decided by whoever calls play
 * (usually a script); there is no state machine. It updates while editing too, so an
 * animation can be seen without Play.
 */
class CH_CORE_EXPORT SpriteAnimatorComponent : public Component
{
 public:
  SpriteAnimatorComponent();

  /**
   * Stops what played; the sprite keeps its frame until something plays.
   */
  void
  setAtlas(const SPtr<SpriteAtlasAsset>& atlas);

  NODISCARD FORCEINLINE const SPtr<SpriteAtlasAsset>&
  getAtlas() const noexcept { return m_atlas; }

  /**
   * The atlas asked for, kept even when the asset is missing.
   */
  NODISCARD FORCEINLINE const UUID&
  getAtlasId() const noexcept { return m_atlasId; }

#if USING(CH_EDITOR)
  /**
   * Finds the animation by its name again and shows its frame, after the atlas was edited.
   */
  void
  refreshAtlas();
#endif // USING(CH_EDITOR)

  /**
   * Plays the animation with that name (ignoring case) from its start; the one already
   * playing only resumes, unless bRestart, so calling it every frame is fine. False when the
   * atlas has no such animation (nothing changes then).
   */
  bool
  play(StringView animation, bool bRestart = false);

  /**
   * Back to the start of the animation, not playing.
   */
  void
  stop();

  FORCEINLINE void
  setPlaying(bool bPlaying) noexcept { m_bPlaying = bPlaying; }

  NODISCARD FORCEINLINE bool
  isPlaying() const noexcept { return m_bPlaying; }

  /**
   * True once an animation that does not loop reached its end.
   */
  NODISCARD FORCEINLINE bool
  isFinished() const noexcept { return m_bFinished; }

  /**
   * Multiplies the time; negative plays backwards.
   */
  FORCEINLINE void
  setSpeed(float speed) noexcept { m_speed = speed; }

  NODISCARD FORCEINLINE float
  getSpeed() const noexcept { return m_speed; }

  /**
   * Seconds from the start of the animation.
   */
  void
  setTime(float time);

  NODISCARD FORCEINLINE float
  getTime() const noexcept { return m_time; }

  /**
   * The name of the animation playing (or paused), empty without one.
   */
  NODISCARD const String&
  getAnimationName() const noexcept { return m_animationName; }

  /**
   * The atlas frame shown now, NO_FRAME without an animation.
   */
  NODISCARD uint32
  getCurrentFrame() const;

  void
  update(float deltaTime) override;

  void
  serialize(DataStream& stream) const override;

  NODISCARD bool
  deserialize(DataStream& stream) override;

  void
  collectReferences(Vector<UUID>& outReferences) const override;

  static constexpr uint32 SERIALIZATION_VERSION = 1;
  static constexpr uint32 NO_ANIMATION = 0xFFFFFFFFu;
  static constexpr uint32 NO_FRAME = 0xFFFFFFFFu;

 protected:
  void
  onRegister() override;

 private:
  /**
   * Shows the current frame on the sprite of the object when it changed since the last
   * time, or when bForce.
   */
  void
  applyFrame(bool bForce);

  SPtr<SpriteAtlasAsset> m_atlas;
  UUID m_atlasId = UUID::null();
  // Saved by name, so an atlas edited later still finds it.
  String m_animationName;
  uint32 m_animation = NO_ANIMATION;
  float m_time = 0.0f;
  float m_speed = 1.0f;
  uint32 m_appliedFrame = NO_FRAME;
  bool m_bPlaying = false;
  bool m_bFinished = false;
};

DECLARE_TYPE_TRAITS(SpriteAnimatorComponent)

} // namespace chEngineSDK
