/************************************************************************/
/**
 * @file chProjectileTargetComponent.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  A circle projectiles of other systems can hit.
 */
/************************************************************************/
#include "chProjectileTargetComponent.h"

#include "chAssetFile.h"
#include "chFileStream.h"
#include "chGameObject.h"
#include "chMath.h"

namespace chEngineSDK {

namespace {
struct TargetFileData {
  float radius;
  uint32 team;
};
static_assert(sizeof(TargetFileData) == 8, "TargetFileData must have no padding");

/*
 * Built on first use; the plugin's components are the only ones that touch it, on the main
 * thread.
 */
UnorderedMap<const Scene*, Vector<ProjectileTargetComponent*>>&
getTargetLists()
{
  static UnorderedMap<const Scene*, Vector<ProjectileTargetComponent*>> lists;
  return lists;
}
} // namespace

/*
 */
ProjectileTargetComponent::ProjectileTargetComponent(const float radius, const uint32 team)
{
  setRadius(radius);
  setTeam(team);
}

/*
 */
void
ProjectileTargetComponent::serialize(DataStream& stream) const
{
  const uint32 version = SERIALIZATION_VERSION;
  const TargetFileData data{.radius = m_radius, .team = m_team};
  stream.write(&version, sizeof(version));
  stream.write(&data, sizeof(data));
}

/*
 */
bool
ProjectileTargetComponent::deserialize(DataStream& stream)
{
  uint32 version = 0;
  TargetFileData data;
  if (!AssetFile::readValue(stream, version) || version != SERIALIZATION_VERSION
      || !AssetFile::readValue(stream, data) || !Math::isFinite(data.radius)) {
    return false;
  }
  setRadius(data.radius);
  setTeam(data.team);
  return true;
}

/*
 */
void
ProjectileTargetComponent::setRadius(const float radius)
{
  m_radius = Math::max(radius, MIN_RADIUS);
}

/*
 */
void
ProjectileTargetComponent::setTeam(const uint32 team)
{
  m_team = Math::clamp(team, 1u, MAX_TEAMS);
}

/*
 */
Vector2
ProjectileTargetComponent::getCenter() const
{
  const Matrix4 world = getOwner()->getCurrentWorldMatrix();
  return Vector2(world[3][1], world[3][2]);
}

/*
 */
Span<ProjectileTargetComponent* const>
ProjectileTargetComponent::getTargets(const Scene& scene)
{
  const auto& lists = getTargetLists();
  const auto it = lists.find(&scene);
  if (it == lists.end()) {
    return {};
  }
  return it->second;
}

/*
 */
void
ProjectileTargetComponent::onRegister()
{
  const Scene* scene = getOwner()->getScene();
  if (!scene) {
    return;
  }
  Vector<ProjectileTargetComponent*>& list = getTargetLists()[scene];
  m_listedScene = scene;
  m_listIndex = static_cast<uint32>(list.size());
  list.push_back(this);
}

/*
 */
void
ProjectileTargetComponent::onUnregister()
{
  if (!m_listedScene) {
    return;
  }
  auto& lists = getTargetLists();
  const auto it = lists.find(m_listedScene);
  m_listedScene = nullptr;
  if (it == lists.end()) {
    return;
  }

  // The last target takes this one's place, so leaving costs the same with any count.
  Vector<ProjectileTargetComponent*>& list = it->second;
  ProjectileTargetComponent* const last = list.back();
  list[m_listIndex] = last;
  last->m_listIndex = m_listIndex;
  list.pop_back();
  if (list.empty()) {
    lists.erase(it);
  }
}

} // namespace chEngineSDK
