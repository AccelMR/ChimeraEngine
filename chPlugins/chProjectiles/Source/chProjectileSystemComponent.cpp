/************************************************************************/
/**
 * @file chProjectileSystemComponent.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Many projectiles of one look, moved, hit and drawn without GameObjects.
 */
/************************************************************************/
#include "chProjectileSystemComponent.h"

#include "chAssetFile.h"
#include "chAssetManager.h"
#include "chFileStream.h"
#include "chGameObject.h"
#include "chMath.h"
#include "chProjectileTargetComponent.h"
#include "chScene.h"
#include "chScriptManager.h"
#include "chScriptObjectTypes.h"
#include "chSpriteDraw.h"
#include "chTextureAsset.h"

#include "lua.h"

namespace chEngineSDK {

namespace {
struct SystemFileData {
  UUID texture;
  float uvRect[4];
  float size[2];
  float pivot[2];
  float color[4];
  float depth;
  int16 sortingLayer;
  int16 order;
  uint8 blendMode;
  uint8 filter;
  uint8 bFaceVelocity;
  uint8 padding;
  float speed;
  float lifetime;
  float radius;
  float damage;
  float turnRate;
  float scale;
  float acceleration[2];
  uint32 pierce;
  uint32 capacity;
  uint32 hitTeams;
  float cellSize;
};
static_assert(sizeof(SystemFileData) == 124, "SystemFileData must have no padding");

NODISCARD bool
isFinite(const float* values, const SIZE_T count)
{
  for (SIZE_T i = 0; i < count; ++i) {
    if (!Math::isFinite(values[i])) {
      return false;
    }
  }
  return true;
}

/*
 * The unit vector of angle degrees, counterclockwise from +Y.
 */
NODISCARD Vector2
getDirection(const float angle)
{
  float sine = 0.0f;
  float cosine = 1.0f;
  Math::sinCos(Degree(angle).valueRadian(), sine, cosine);
  return Vector2(cosine, sine);
}
} // namespace

/*
 */
ProjectileSystemComponent::ProjectileSystemComponent()
 : m_hitCallback(ScriptClassInfo::NO_REFERENCE)
{
  setUpdateFlags(ComponentUpdate::Fixed);
  // Projectiles fired outside Play (Test Fire) must move too; with none it returns at once.
  setUpdatesWhileEditing(true);
}

/*
 */
ProjectileSystemComponent::~ProjectileSystemComponent() { releaseHitCallback(); }

/*
 */
void
ProjectileSystemComponent::fixedUpdate(const float deltaTime)
{
  const Scene* scene = getOwner() ? getOwner()->getScene() : nullptr;
  if (!scene || (!scene->isPlaying() && m_projectiles.empty())) {
    return;
  }
  if (scene->isPlaying()) {
    step(deltaTime);
  }
  else {
    // No scripts run while editing, and a target hit would only remove the preview.
    m_hits.clear();
    move(deltaTime);
    removeDead();
  }
  draw();
  callHitScripts();
}

/*
 */
void
ProjectileSystemComponent::step(const float deltaTime)
{
  m_hits.clear();
  if (m_projectiles.empty()) {
    return;
  }
  move(deltaTime);
  hitTargets();
  removeDead();
}

/*
 */
void
ProjectileSystemComponent::move(const float deltaTime)
{
  const GameObject* homing = getHomingTarget();
  Vector2 homingPoint = Vector2::ZERO;
  if (homing) {
    const Matrix4 world = homing->getCurrentWorldMatrix();
    homingPoint = Vector2(world[3][1], world[3][2]);
  }

  for (Projectile& projectile : m_projectiles) {
    projectile.age += deltaTime;
    if (projectile.age >= projectile.lifetime) {
      projectile.bDead = true;
      continue;
    }
    projectile.velocity += projectile.acceleration * deltaTime;

    if (homing && projectile.turnRate > 0.0f) {
      const Vector2 toTarget = homingPoint - projectile.position;
      const float angle =
          Math::atan2(projectile.velocity.cross(toTarget), projectile.velocity.dot(toTarget))
              .valueRadian();
      const float maxTurn = Degree(projectile.turnRate * deltaTime).valueRadian();
      float sine = 0.0f;
      float cosine = 1.0f;
      Math::sinCos(Math::clamp(angle, -maxTurn, maxTurn), sine, cosine);
      const Vector2 velocity = projectile.velocity;
      projectile.velocity = Vector2(velocity.x * cosine - velocity.y * sine,
                                    velocity.x * sine + velocity.y * cosine);
    }
    projectile.position += projectile.velocity * deltaTime;
  }
}

/*
 * Projectiles are hashed and each target asks for the ones it touches, because targets are
 * few and projectiles many.
 */
void
ProjectileSystemComponent::hitTargets()
{
  const Scene* scene = getOwner() ? getOwner()->getScene() : nullptr;
  if (!scene || m_hitTeams == 0) {
    return;
  }
  const Span<ProjectileTargetComponent* const> targets =
      ProjectileTargetComponent::getTargets(*scene);
  if (targets.empty()) {
    return;
  }

  m_hash.clear();
  for (uint32 i = 0; i < static_cast<uint32>(m_projectiles.size()); ++i) {
    const Projectile& projectile = m_projectiles[i];
    if (!projectile.bDead) {
      m_hash.addCircle(projectile.position, projectile.radius, i);
    }
  }
  if (m_hash.getShapeCount() == 0) {
    return;
  }
  m_hash.build();

  for (const ProjectileTargetComponent* target : targets) {
    const uint32 teamBit = 1u << (target->getTeam() - 1);
    GameObject* const owner = target->getOwner();
    if ((m_hitTeams & teamBit) == 0 || owner == getOwner() || owner->isDestroyRequested()) {
      continue;
    }
    m_hash.queryCircle(target->getCenter(), target->getRadius(), m_found);
    for (const uint32 index : m_found) {
      Projectile& projectile = m_projectiles[index];
      if (projectile.bDead || projectile.lastHit == owner) {
        continue;
      }
      m_hits.push_back(
          {.target = owner, .position = projectile.position, .damage = projectile.damage});
      projectile.lastHit = owner;
      if (projectile.pierce == 0) {
        projectile.bDead = true;
      }
      else {
        --projectile.pierce;
      }
    }
  }
}

/*
 * The last one takes each dead one's place, so the order changes but nothing moves twice.
 */
void
ProjectileSystemComponent::removeDead()
{
  SIZE_T i = 0;
  while (i < m_projectiles.size()) {
    if (m_projectiles[i].bDead) {
      m_projectiles[i] = m_projectiles.back();
      m_projectiles.pop_back();
    }
    else {
      ++i;
    }
  }
}

/*
 * The instance parts every projectile shares are made once; each projectile adds its
 * center and axes.
 */
void
ProjectileSystemComponent::draw()
{
  Scene* scene = getOwner() ? getOwner()->getScene() : nullptr;
  if (!scene || m_projectiles.empty()) {
    return;
  }
  if ((m_textureAndFlags & ~GpuSpriteInstance::POINT_FILTER_BIT)
          == GpuSpriteInstance::NO_TEXTURE
      && m_texture) {
    m_textureAndFlags = SpriteDraw::makeTextureFlags(m_texture.get(), m_look.filter);
  }

  GpuSpriteInstance shared{};
  shared.textureAndFlags = m_textureAndFlags;
  SpriteDraw::packColor(m_look.color, shared.colorRG, shared.colorBA);
  for (uint32 i = 0; i < 4; ++i) {
    shared.uvRect[i] = m_look.uvRect[i];
  }
  const float pivotU = m_look.pivot.x * 2.0f - 1.0f;
  const float pivotV = m_look.pivot.y * 2.0f - 1.0f;

  SpriteDraw& spriteDraw = scene->getFixedSpriteDraw();
  for (const Projectile& projectile : m_projectiles) {
    const float halfWidth = m_look.size.x * projectile.scale * 0.5f;
    const float halfHeight = m_look.size.y * projectile.scale * 0.5f;
    float cosine = 1.0f;
    float sine = 0.0f;
    if (m_look.bFaceVelocity) {
      const float lengthSquared = projectile.velocity.sqrMagnitude();
      if (lengthSquared > Math::SMALL_NUMBER) {
        const float inverseLength = Math::invSqrt(lengthSquared);
        cosine = projectile.velocity.x * inverseLength;
        sine = projectile.velocity.y * inverseLength;
      }
    }

    // The right side turns from +Y toward +Z, counterclockwise seen from the camera.
    GpuSpriteInstance instance = shared;
    instance.halfAxisU = Vector3(0.0f, cosine * halfWidth, sine * halfWidth);
    instance.halfAxisV = Vector3(0.0f, -sine * halfHeight, cosine * halfHeight);
    instance.center = Vector3(m_look.depth, projectile.position.x, projectile.position.y)
                      - instance.halfAxisU * pivotU - instance.halfAxisV * pivotV;
    spriteDraw.instance(instance, Math::abs(halfWidth) + Math::abs(halfHeight),
                        m_look.sortingLayer, m_look.order, m_look.blendMode);
  }
}

/*
 * The scene of the system is the running script's scene during the calls, so the function
 * can use Scene.find, Sprites.draw and the like.
 */
void
ProjectileSystemComponent::callHitScripts()
{
  if (m_hitCallback == ScriptClassInfo::NO_REFERENCE || m_hits.empty()
      || !ScriptManager::isStarted()) {
    return;
  }
  ScriptManager& scriptManager = ScriptManager::instance();
  lua_State* const state = scriptManager.getState();
  Scene* const previousScene = scriptManager.getCurrentScene();
  scriptManager.setCurrentScene(getOwner()->getScene());

  for (const ProjectileHit& hit : m_hits) {
    // A function may drop itself.
    if (m_hitCallback == ScriptClassInfo::NO_REFERENCE) {
      break;
    }
    if (!hit.target->getScene()) {
      continue;
    }
    scriptManager.beginCall(m_hitCallback);
    ScriptTraits<GameObject>::push(state, *hit.target);
    lua_pushnumber(state, static_cast<lua_Number>(hit.damage));
    lua_pushnumber(state, static_cast<lua_Number>(hit.position.x));
    lua_pushnumber(state, static_cast<lua_Number>(hit.position.y));
    // An error is logged and the other hits still reach the function.
    static_cast<void>(scriptManager.endCall(4));
  }
  scriptManager.setCurrentScene(previousScene);
}

/*
 */
void
ProjectileSystemComponent::serialize(DataStream& stream) const
{
  SystemFileData data{
      .texture = m_look.texture,
      .uvRect = {m_look.uvRect[0], m_look.uvRect[1], m_look.uvRect[2], m_look.uvRect[3]},
      .size = {m_look.size.x, m_look.size.y},
      .pivot = {m_look.pivot.x, m_look.pivot.y},
      .color = {m_look.color.r, m_look.color.g, m_look.color.b, m_look.color.a},
      .depth = m_look.depth,
      .sortingLayer = m_look.sortingLayer,
      .order = m_look.order,
      .blendMode = static_cast<uint8>(m_look.blendMode),
      .filter = static_cast<uint8>(m_look.filter),
      .bFaceVelocity = static_cast<uint8>(m_look.bFaceVelocity ? 1 : 0),
      .padding = 0,
      .speed = m_defaults.speed,
      .lifetime = m_defaults.lifetime,
      .radius = m_defaults.radius,
      .damage = m_defaults.damage,
      .turnRate = m_defaults.turnRate,
      .scale = m_defaults.scale,
      .acceleration = {m_defaults.acceleration.x, m_defaults.acceleration.y},
      .pierce = m_defaults.pierce,
      .capacity = m_capacity,
      .hitTeams = m_hitTeams,
      .cellSize = m_cellSize};
  const uint32 version = SERIALIZATION_VERSION;
  stream.write(&version, sizeof(version));
  stream.write(&data, sizeof(data));
}

/*
 */
bool
ProjectileSystemComponent::deserialize(DataStream& stream)
{
  uint32 version = 0;
  SystemFileData data;
  if (!AssetFile::readValue(stream, version) || version != SERIALIZATION_VERSION
      || !AssetFile::readValue(stream, data)) {
    return false;
  }
  if (!isFinite(data.uvRect, 4) || !isFinite(data.size, 2) || !isFinite(data.pivot, 2)
      || !isFinite(data.color, 4) || !Math::isFinite(data.depth) || !Math::isFinite(data.speed)
      || !Math::isFinite(data.lifetime) || !Math::isFinite(data.radius)
      || !Math::isFinite(data.damage) || !Math::isFinite(data.turnRate)
      || !Math::isFinite(data.scale) || !isFinite(data.acceleration, 2)
      || !Math::isFinite(data.cellSize)
      || data.blendMode >= static_cast<uint8>(SpriteBlendMode::COUNT)
      || data.filter >= static_cast<uint8>(SpriteFilter::COUNT)) {
    return false;
  }

  ProjectileLook look;
  look.texture = data.texture;
  for (uint32 i = 0; i < 4; ++i) {
    look.uvRect[i] = data.uvRect[i];
  }
  look.size = Vector2(data.size[0], data.size[1]);
  look.pivot = Vector2(data.pivot[0], data.pivot[1]);
  look.color = LinearColor(data.color[0], data.color[1], data.color[2], data.color[3]);
  look.depth = data.depth;
  look.sortingLayer = data.sortingLayer;
  look.order = data.order;
  look.blendMode = static_cast<SpriteBlendMode>(data.blendMode);
  look.filter = static_cast<SpriteFilter>(data.filter);
  look.bFaceVelocity = data.bFaceVelocity != 0;
  setLook(look);

  setDefaults({.speed = data.speed,
               .lifetime = data.lifetime,
               .radius = data.radius,
               .damage = data.damage,
               .turnRate = data.turnRate,
               .scale = data.scale,
               .acceleration = Vector2(data.acceleration[0], data.acceleration[1]),
               .pierce = static_cast<uint16>(Math::min(data.pierce, 0xFFFFu))});
  setCapacity(data.capacity);
  m_hitTeams = data.hitTeams;
  setCellSize(data.cellSize);
  return true;
}

/*
 */
void
ProjectileSystemComponent::collectReferences(Vector<UUID>& outReferences) const
{
  if (!m_look.texture.isNull()) {
    outReferences.push_back(m_look.texture);
  }
}

/*
 */
void
ProjectileSystemComponent::setCapacity(const uint32 capacity)
{
  m_capacity = Math::clamp(capacity, 1u, MAX_CAPACITY);
  if (m_projectiles.size() > m_capacity) {
    m_projectiles.resize(m_capacity);
  }
  if (isRegistered()) {
    m_projectiles.reserve(m_capacity);
  }
}

/*
 */
void
ProjectileSystemComponent::setLook(const ProjectileLook& look)
{
  m_look = look;
  m_look.size = Vector2(Math::max(look.size.x, 0.0f), Math::max(look.size.y, 0.0f));
  resolveTexture();
}

/*
 */
void
ProjectileSystemComponent::setDefaults(const ProjectileDefaults& defaults)
{
  m_defaults = defaults;
  m_defaults.lifetime = Math::max(defaults.lifetime, 0.0f);
  m_defaults.radius = Math::max(defaults.radius, 0.0f);
  m_defaults.turnRate = Math::max(defaults.turnRate, 0.0f);
}

/*
 */
void
ProjectileSystemComponent::setCellSize(const float cellSize)
{
  m_cellSize = Math::max(cellSize, SpatialHash2D::MIN_CELL_SIZE);
  m_hash.setCellSize(m_cellSize);
}

/*
 */
void
ProjectileSystemComponent::setHomingTarget(GameObject* target)
{
  m_homingTarget = target ? target->weak_from_this() : WeakPtr<GameObject>();
}

/*
 */
GameObject*
ProjectileSystemComponent::getHomingTarget() const
{
  const SPtr<GameObject> target = m_homingTarget.lock();
  return target && target->getScene() && !target->isDestroyRequested() ? target.get()
                                                                       : nullptr;
}

/*
 */
bool
ProjectileSystemComponent::spawn(const ProjectileSpawn& projectile)
{
  if (m_projectiles.size() >= m_capacity) {
    return false;
  }
  m_projectiles.push_back({.position = projectile.position,
                           .velocity = projectile.velocity,
                           .acceleration = projectile.acceleration,
                           .age = 0.0f,
                           .lifetime = Math::max(projectile.lifetime, 0.0f),
                           .radius = Math::max(projectile.radius, 0.0f),
                           .damage = projectile.damage,
                           .turnRate = Math::max(projectile.turnRate, 0.0f),
                           .scale = projectile.scale,
                           .lastHit = nullptr,
                           .pierce = projectile.pierce,
                           .bDead = false});
  return true;
}

/*
 */
bool
ProjectileSystemComponent::fire(const Vector2& position, const float angle, const float speed)
{
  return spawn({.position = position,
                .velocity = getDirection(angle) * speed,
                .acceleration = m_defaults.acceleration,
                .lifetime = m_defaults.lifetime,
                .radius = m_defaults.radius,
                .damage = m_defaults.damage,
                .turnRate = m_defaults.turnRate,
                .scale = m_defaults.scale,
                .pierce = m_defaults.pierce});
}

/*
 */
uint32
ProjectileSystemComponent::fireRing(const Vector2& position, const uint32 count,
                                    const float startAngle, const float speed)
{
  const float step = count > 0 ? 360.0f / static_cast<float>(count) : 0.0f;
  uint32 fired = 0;
  while (fired < count
         && fire(position, startAngle + step * static_cast<float>(fired), speed)) {
    ++fired;
  }
  return fired;
}

/*
 */
uint32
ProjectileSystemComponent::fireSpread(const Vector2& position, const uint32 count,
                                      const float angle, const float arc, const float speed)
{
  if (count <= 1) {
    return count == 1 && fire(position, angle, speed) ? 1 : 0;
  }
  const float first = angle - arc * 0.5f;
  const float step = arc / static_cast<float>(count - 1);
  uint32 fired = 0;
  while (fired < count && fire(position, first + step * static_cast<float>(fired), speed)) {
    ++fired;
  }
  return fired;
}

/*
 */
void
ProjectileSystemComponent::clear() noexcept
{
  m_projectiles.clear();
}

#if USING(CH_EDITOR)
/*
 */
void
ProjectileSystemComponent::testFire()
{
  const Matrix4 world = getOwner()->getCurrentWorldMatrix();
  static_cast<void>(
      fireRing(Vector2(world[3][1], world[3][2]), TEST_FIRE_COUNT, 0.0f, m_defaults.speed));
}
#endif // USING(CH_EDITOR)

/*
 */
void
ProjectileSystemComponent::setHitCallback(const int32 function)
{
  releaseHitCallback();
  m_hitCallback = function;
}

/*
 */
void
ProjectileSystemComponent::releaseHitCallback() noexcept
{
  if (m_hitCallback != ScriptClassInfo::NO_REFERENCE && ScriptManager::isStarted()) {
    ScriptManager::instance().releaseReference(m_hitCallback);
  }
  m_hitCallback = ScriptClassInfo::NO_REFERENCE;
}

/*
 * Every array is sized once here, so spawning, stepping and drawing never allocate.
 */
void
ProjectileSystemComponent::onRegister()
{
  m_projectiles.reserve(m_capacity);
  m_hits.reserve(64);
  resolveTexture();
}

/*
 * The projectiles alive are dropped; the hit function stays for when it registers again.
 */
void
ProjectileSystemComponent::onUnregister()
{
  m_projectiles.clear();
  m_hits.clear();
}

/*
 */
void
ProjectileSystemComponent::resolveTexture()
{
  m_texture.reset();
  m_textureAndFlags = SpriteDraw::makeTextureFlags(nullptr, m_look.filter);
  if (m_look.texture.isNull() || !AssetManager::isStarted()) {
    return;
  }
  AssetManager& assetManager = AssetManager::instance();
  const SPtr<IAsset> asset = assetManager.getAsset(m_look.texture);
  m_texture = asset ? asset->as<TextureAsset>() : nullptr;
  if (m_texture && !m_texture->isLoaded() && !assetManager.syncLoadAsset(m_texture)) {
    m_texture.reset();
  }
  m_textureAndFlags = SpriteDraw::makeTextureFlags(m_texture.get(), m_look.filter);
}

} // namespace chEngineSDK
