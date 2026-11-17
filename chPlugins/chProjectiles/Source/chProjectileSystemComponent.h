/************************************************************************/
/**
 * @file chProjectileSystemComponent.h
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Many projectiles of one look, moved, hit and drawn without GameObjects.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chComponent.h"
#include "chLinearColor.h"
#include "chSpatialHash2D.h"
#include "chSpriteItem.h"
#include "chTypeTraits.h"
#include "chVector2.h"

namespace chEngineSDK {
class SpriteMaterialAsset;
class TextureAsset;

/**
 * What every projectile of a system looks like. The texture part is the one a SpriteStyle
 * holds; the size is multiplied by each projectile's scale.
 */
struct ProjectileLook {
  UUID texture = UUID::null();
  // As GpuSpriteInstance::uvRect.
  float uvRect[4] = {0.0f, 0.0f, 1.0f, 1.0f};
  Vector2 size{0.2f, 0.2f};
  // (0, 0) is the bottom left corner.
  Vector2 pivot{0.5f, 0.5f};
  LinearColor color = LinearColor::WHITE;
  // World X of every projectile.
  float depth = 0.0f;
  int16 sortingLayer = 0;
  int16 order = 0;
  SpriteBlendMode blendMode = SpriteBlendMode::Alpha;
  SpriteFilter filter = SpriteFilter::Point;
  // The image's right side points along the velocity; otherwise it never turns.
  bool bFaceVelocity = true;
  // A SpriteMaterialAsset that shades every projectile; null for the engine's shader.
  UUID material = UUID::null();
};

/**
 * What a projectile starts with. Positions and vectors are Y and Z of the YZ plane.
 */
struct ProjectileSpawn {
  Vector2 position = Vector2::ZERO;
  Vector2 velocity = Vector2::ZERO;
  Vector2 acceleration = Vector2::ZERO;
  // Seconds before it dies on its own.
  float lifetime = 5.0f;
  float radius = 0.1f;
  float damage = 1.0f;
  // Degrees per second it turns toward the homing target; 0 flies straight.
  float turnRate = 0.0f;
  // Multiplies the look's size.
  float scale = 1.0f;
  // Targets it goes through before the next hit kills it.
  uint16 pierce = 0;
};

/**
 * What the fire helpers give every projectile they make, besides position and direction.
 */
struct ProjectileDefaults {
  float speed = 5.0f;
  float lifetime = 5.0f;
  float radius = 0.1f;
  float damage = 1.0f;
  float turnRate = 0.0f;
  float scale = 1.0f;
  Vector2 acceleration = Vector2::ZERO;
  uint16 pierce = 0;
};

/**
 * One projectile reaching a target in the last step.
 */
struct ProjectileHit {
  GameObject* target = nullptr;
  Vector2 position = Vector2::ZERO;
  float damage = 0.0f;
};

/**
 * Exists so a game can have thousands of bullets without a GameObject or a script each: the
 * projectiles are plain data in arrays sized once, moved in every fixed step, tested against
 * the ProjectileTargetComponents of the scene whose team the system hits, and drawn through
 * the scene's fixed step SpriteDraw. Hits are listed after each step and handed to a script
 * function when one is set. Each system has one look, so player and enemy bullets are two
 * systems. Projectiles are never saved; the settings are. Hits are only tested and reported
 * while the scene plays; outside Play the projectiles alive (Test Fire in the editor) still
 * move and draw.
 */
class ProjectileSystemComponent : public Component
{
 public:
  ProjectileSystemComponent();

  ~ProjectileSystemComponent() override;

  void
  fixedUpdate(float deltaTime) override;

  void
  serialize(DataStream& stream) const override;

  NODISCARD bool
  deserialize(DataStream& stream) override;

  void
  collectReferences(Vector<UUID>& outReferences) const override;

  /**
   * The projectiles alive now are dropped when the capacity shrinks below their count.
   * Kept within 1..MAX_CAPACITY.
   */
  void
  setCapacity(uint32 capacity);

  NODISCARD FORCEINLINE uint32
  getCapacity() const noexcept
  {
    return m_capacity;
  }

  NODISCARD FORCEINLINE uint32
  getCount() const noexcept
  {
    return static_cast<uint32>(m_projectiles.size());
  }

  /**
   * Loads look.texture when it is set and not loaded.
   */
  void
  setLook(const ProjectileLook& look);

  NODISCARD FORCEINLINE const ProjectileLook&
  getLook() const noexcept
  {
    return m_look;
  }

  void
  setDefaults(const ProjectileDefaults& defaults);

  NODISCARD FORCEINLINE const ProjectileDefaults&
  getDefaults() const noexcept
  {
    return m_defaults;
  }

  /**
   * Bit t - 1 set hits targets of team t.
   */
  FORCEINLINE void
  setHitTeams(const uint32 teamMask) noexcept
  {
    m_hitTeams = teamMask;
  }

  NODISCARD FORCEINLINE uint32
  getHitTeams() const noexcept
  {
    return m_hitTeams;
  }

  /**
   * Side of the cells projectiles are sorted into for the hit tests; about twice the
   * biggest radius among projectiles and targets is a good start. Kept at least
   * SpatialHash2D::MIN_CELL_SIZE.
   */
  void
  setCellSize(float cellSize);

  NODISCARD FORCEINLINE float
  getCellSize() const noexcept
  {
    return m_cellSize;
  }

  /**
   * Projectiles with a turn rate steer toward target while it is in a scene; null flies
   * them straight.
   */
  void
  setHomingTarget(GameObject* target);

  NODISCARD GameObject*
  getHomingTarget() const;

  /**
   * False when the system is full, which drops the projectile.
   */
  bool
  spawn(const ProjectileSpawn& projectile);

  /**
   * One projectile at position going toward angle (degrees, counterclockwise from +Y) at
   * speed, the rest from the defaults.
   */
  bool
  fire(const Vector2& position, float angle, float speed);

  /**
   * count projectiles spread evenly around position, the first at startAngle.
   */
  uint32
  fireRing(const Vector2& position, uint32 count, float startAngle, float speed);

  /**
   * count projectiles spread evenly over arc degrees centered on angle (one goes along
   * angle).
   */
  uint32
  fireSpread(const Vector2& position, uint32 count, float angle, float arc, float speed);

  /**
   * Drops every projectile now.
   */
  void
  clear() noexcept;

#if USING(CH_EDITOR)
  /**
   * A ring of TEST_FIRE_COUNT projectiles from the owner's position with the defaults, to
   * try the look in the editor; they move and draw also while the scene does not play.
   */
  void
  testFire();

  static constexpr uint32 TEST_FIRE_COUNT = 16;
#endif // USING(CH_EDITOR)

  /**
   * What the projectiles did in the last step, in the order they hit. Valid until the next
   * step.
   */
  NODISCARD FORCEINLINE Span<const ProjectileHit>
  getLastHits() const noexcept
  {
    return m_hits;
  }

  /**
   * Registry reference of a script function called as function(target, damage, x, y) for
   * each hit after a step, x and y being the world Y and Z of the projectile;
   * ScriptClassInfo::NO_REFERENCE for none. The component releases the reference it held.
   */
  void
  setHitCallback(int32 function);

  NODISCARD FORCEINLINE int32
  getHitCallback() const noexcept
  {
    return m_hitCallback;
  }

  /**
   * Moves, ages and hits for deltaTime seconds, without drawing or calling scripts;
   * fixedUpdate calls it, tests call it directly.
   */
  void
  step(float deltaTime);

  static constexpr uint32 DEFAULT_CAPACITY = 2048;
  static constexpr uint32 MAX_CAPACITY = 1u << 20;
  static constexpr uint32 ALL_TEAMS = 0xFFFFFFFFu;
  static constexpr uint32 SERIALIZATION_VERSION = 1;

 protected:
  void
  onRegister() override;

  void
  onUnregister() override;

 private:
  struct Projectile {
    Vector2 position;
    Vector2 velocity;
    Vector2 acceleration;
    float age;
    float lifetime;
    float radius;
    float damage;
    float turnRate;
    float scale;
    // Last target hit, so a piercing projectile hits each target it crosses once.
    const GameObject* lastHit;
    uint16 pierce;
    bool bDead;
  };

  void
  resolveTexture();

  void
  move(float deltaTime);

  void
  hitTargets();

  void
  removeDead();

  void
  draw();

  void
  callHitScripts();

  void
  releaseHitCallback() noexcept;

  Vector<Projectile> m_projectiles;
  Vector<ProjectileHit> m_hits;
  ProjectileLook m_look;
  ProjectileDefaults m_defaults;
  SPtr<TextureAsset> m_texture;
  SPtr<SpriteMaterialAsset> m_material;
  WeakPtr<GameObject> m_homingTarget;
  SpatialHash2D m_hash;
  // Filled by queries, kept to reuse its memory.
  Vector<uint32> m_found;
  uint32 m_capacity = DEFAULT_CAPACITY;
  uint32 m_hitTeams = ALL_TEAMS;
  float m_cellSize = 1.0f;
  int32 m_hitCallback;
  // Texture index and filter bit of the look, read again when the texture's GPU copy
  // appears.
  uint32 m_textureAndFlags = GpuSpriteInstance::NO_TEXTURE;
};

DECLARE_TYPE_TRAITS(ProjectileSystemComponent)

} // namespace chEngineSDK
