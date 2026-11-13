/************************************************************************/
/**
 * @file chProjectileTargetComponent.h
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  A circle projectiles of other systems can hit.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chComponent.h"
#include "chTypeTraits.h"
#include "chVector2.h"

namespace chEngineSDK {
class Scene;

/**
 * Exists so an object (an enemy, the player) can be hit by projectiles without a script
 * per projectile: a circle in the YZ plane around its world position, on one team. Every
 * registered target of a scene is listed, so a ProjectileSystemComponent reads them all in
 * one pass.
 */
class ProjectileTargetComponent : public Component
{
 public:
  ProjectileTargetComponent() = default;

  ProjectileTargetComponent(float radius, uint32 team);

  void
  serialize(DataStream& stream) const override;

  NODISCARD bool
  deserialize(DataStream& stream) override;

  /**
   * Kept at least MIN_RADIUS.
   */
  void
  setRadius(float radius);

  NODISCARD FORCEINLINE float
  getRadius() const noexcept
  {
    return m_radius;
  }

  /**
   * Clamped to 1..MAX_TEAMS.
   */
  void
  setTeam(uint32 team);

  NODISCARD FORCEINLINE uint32
  getTeam() const noexcept
  {
    return m_team;
  }

  /**
   * Y and Z of the owner's world position as of now.
   */
  NODISCARD Vector2
  getCenter() const;

  /**
   * The registered targets of scene, in no particular order. Valid until a target
   * registers or unregisters.
   */
  NODISCARD static Span<ProjectileTargetComponent* const>
  getTargets(const Scene& scene);

  static constexpr float MIN_RADIUS = 1e-3f;
  static constexpr uint32 MAX_TEAMS = 32;
  static constexpr uint32 SERIALIZATION_VERSION = 1;

 protected:
  void
  onRegister() override;

  void
  onUnregister() override;

 private:
  float m_radius = 0.5f;
  uint32 m_team = 1;
  // The scene the target is listed in while registered.
  const Scene* m_listedScene = nullptr;
  uint32 m_listIndex = 0;
};

DECLARE_TYPE_TRAITS(ProjectileTargetComponent)

} // namespace chEngineSDK
