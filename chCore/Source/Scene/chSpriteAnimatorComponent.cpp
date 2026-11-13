/************************************************************************/
/**
 * @file chSpriteAnimatorComponent.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Component that plays the animations of a sprite atlas on its GameObject's sprite.
 */
/************************************************************************/
#include "chSpriteAnimatorComponent.h"

#include "chAssetFile.h"
#include "chAssetManager.h"
#include "chFileStream.h"
#include "chGameObject.h"
#include "chMath.h"
#include "chSceneManager.h"
#include "chSpriteAtlasAsset.h"
#include "chSpriteComponent.h"

namespace chEngineSDK {
namespace {
struct SpriteAnimatorFileData
{
  UUID atlas;
  float speed;
  uint32 bPlaying;
};
static_assert(sizeof(SpriteAnimatorFileData) == 24,
              "SpriteAnimatorFileData must have no padding");

constexpr uint32 kMaxNameLength = SpriteAtlas::MAX_NAME_LENGTH;
} // namespace

/*
 */
SpriteAnimatorComponent::SpriteAnimatorComponent()
{
  setUpdateFlags(ComponentUpdate::Frame);
  setUpdatesWhileEditing(true);
}

/*
 */
void
SpriteAnimatorComponent::setAtlas(const SPtr<SpriteAtlasAsset>& atlas)
{
  m_atlas = atlas;
  m_atlasId = atlas ? atlas->getUUID() : UUID::null();
  m_animation = atlas && !m_animationName.empty()
                    ? atlas->getAtlas().findAnimation(m_animationName)
                    : NO_ANIMATION;
  if (m_animation == NO_ANIMATION) {
    m_bPlaying = false;
  }
  m_appliedFrame = NO_FRAME;
  applyFrame(true);
}

/*
 */
bool
SpriteAnimatorComponent::play(StringView animation, bool bRestart)
{
  if (!m_atlas) {
    return false;
  }
  const uint32 index = m_atlas->getAtlas().findAnimation(animation);
  if (index == SpriteAtlas::NOT_FOUND) {
    return false;
  }
  if (index == m_animation && !bRestart) {
    m_bPlaying = !m_bFinished;
    return true;
  }

  m_animation = index;
  m_animationName = m_atlas->getAtlas().getAnimations()[index].name;
  m_time = m_speed < 0.0f ? m_atlas->getAtlas().getAnimations()[index].getDuration() : 0.0f;
  m_bPlaying = true;
  m_bFinished = false;
  applyFrame(true);
  return true;
}

/*
 */
void
SpriteAnimatorComponent::stop()
{
  m_time = 0.0f;
  m_bPlaying = false;
  m_bFinished = false;
  applyFrame(false);
}

/*
 */
void
SpriteAnimatorComponent::setTime(float time)
{
  m_time = time;
  m_bFinished = false;
  applyFrame(false);
}

/*
 */
uint32
SpriteAnimatorComponent::getCurrentFrame() const
{
  if (!m_atlas || m_animation >= m_atlas->getAtlas().getAnimations().size()) {
    return NO_FRAME;
  }
  const SpriteAnimation& animation = m_atlas->getAtlas().getAnimations()[m_animation];
  if (animation.frames.empty()) {
    return NO_FRAME;
  }
  return animation.frames[animation.getStepAt(m_time)].frame;
}

/*
 */
void
SpriteAnimatorComponent::update(float deltaTime)
{
  if (!m_bPlaying || !m_atlas ||
      m_animation >= m_atlas->getAtlas().getAnimations().size()) {
    return;
  }
  const SpriteAnimation& animation = m_atlas->getAtlas().getAnimations()[m_animation];
  const float duration = animation.getDuration();
  m_time += deltaTime * m_speed;
  if (animation.bLoop) {
    // Kept in one period, so a long session does not lose float precision.
    if (duration > 0.0f && (m_time >= duration || m_time < 0.0f)) {
      m_time = Math::fmod(m_time, duration);
      if (m_time < 0.0f) {
        m_time += duration;
      }
    }
  }
  else if (m_time >= duration || m_time <= 0.0f) {
    const bool bForward = m_speed >= 0.0f;
    if ((bForward && m_time >= duration) || (!bForward && m_time <= 0.0f)) {
      m_time = bForward ? duration : 0.0f;
      m_bPlaying = false;
      m_bFinished = true;
    }
  }
  applyFrame(false);
}

/*
 */
void
SpriteAnimatorComponent::applyFrame(bool bForce)
{
  const uint32 frame = getCurrentFrame();
  if (frame == NO_FRAME || (!bForce && frame == m_appliedFrame) || !isRegistered()) {
    return;
  }
  SpriteComponent* sprite = getOwner()->getComponent<SpriteComponent>();
  if (!sprite) {
    return;
  }
  if (sprite->getAtlas() != m_atlas) {
    sprite->setAtlasFrame(m_atlas, frame);
  }
  else {
    sprite->setFrame(frame);
  }
  m_appliedFrame = frame;
}

/*
 */
void
SpriteAnimatorComponent::serialize(DataStream& stream) const
{
  const uint32 version = SERIALIZATION_VERSION;
  const SpriteAnimatorFileData data{
      .atlas = m_atlasId, .speed = m_speed, .bPlaying = m_bPlaying ? 1u : 0u};
  stream.write(&version, sizeof(version));
  stream.write(&data, sizeof(data));
  AssetFile::writeString(stream, m_animationName);
}

/*
 */
bool
SpriteAnimatorComponent::deserialize(DataStream& stream)
{
  uint32 version = 0;
  SpriteAnimatorFileData data;
  String animationName;
  if (!AssetFile::readValue(stream, version) || version != SERIALIZATION_VERSION ||
      !AssetFile::readValue(stream, data) || !Math::isFinite(data.speed) ||
      !AssetFile::readString(stream, animationName, kMaxNameLength)) {
    return false;
  }

  m_speed = data.speed;
  m_animationName = std::move(animationName);
  SPtr<SpriteAtlasAsset> atlas;
  if (!data.atlas.isNull() && AssetManager::isStarted()) {
    const SPtr<IAsset> asset = AssetManager::instance().getAsset(data.atlas);
    atlas = asset ? asset->as<SpriteAtlasAsset>() : nullptr;
  }
  setAtlas(atlas);
  m_atlasId = data.atlas;
  if (!atlas && !data.atlas.isNull()) {
    CH_LOG_WARNING(SceneManagerLog, "A sprite animator lost its atlas {0}.", data.atlas);
  }
  m_bPlaying = data.bPlaying != 0 && m_animation != NO_ANIMATION;
  return true;
}

/*
 */
void
SpriteAnimatorComponent::collectReferences(Vector<UUID>& outReferences) const
{
  if (!m_atlasId.isNull()) {
    outReferences.push_back(m_atlasId);
  }
}

/*
 */
void
SpriteAnimatorComponent::onRegister()
{
  m_appliedFrame = NO_FRAME;
  applyFrame(true);
}

} // namespace chEngineSDK
