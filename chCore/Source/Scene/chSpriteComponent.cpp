/************************************************************************/
/**
 * @file chSpriteComponent.cpp
 * @author AccelMR
 * @date 2026/11/12
 * @brief
 *  Component that draws a textured quad (a sprite) at its GameObject.
 */
/************************************************************************/
#include "chSpriteComponent.h"

#include "chAssetManager.h"
#include "chBox.h"
#include "chFileStream.h"
#include "chGameObject.h"
#include "chMath.h"
#include "chMatrix4.h"
#include "chScene.h"
#include "chSceneManager.h"
#include "chSpriteDraw.h"
#include "chTextureAsset.h"

namespace chEngineSDK {
namespace {
struct SpriteFileData
{
  UUID texture;
  float sourcePosition[2];
  float sourceSize[2];
  float pixelsPerUnit;
  float pivot[2];
  float color[4];
  int16 sortingLayer;
  int16 order;
  uint8 blendMode;
  uint8 filter;
  uint8 flipX;
  uint8 flipY;
};
static_assert(sizeof(SpriteFileData) == 68, "SpriteFileData must have no padding");

/*
 * Null when there is no such asset or it is of another type. Without an AssetManager
 * (tests) nothing is found.
 */
NODISCARD SPtr<TextureAsset>
findTexture(const UUID& id)
{
  if (id.isNull() || !AssetManager::isStarted()) {
    return nullptr;
  }
  const SPtr<IAsset> asset = AssetManager::instance().getAsset(id);
  return asset ? asset->as<TextureAsset>() : nullptr;
}
} // namespace

/*
 */
void
SpriteComponent::setTexture(const SPtr<TextureAsset>& texture)
{
  m_texture = texture;
  m_textureId = texture ? texture->getUUID() : UUID::null();
  updateSpriteItem();
}

/*
 */
void
SpriteComponent::setSourceRect(const Vector2& position, const Vector2& size)
{
  m_sourcePosition = position;
  m_sourceSize = Vector2(Math::max(size.x, 0.0f), Math::max(size.y, 0.0f));
  updateSpriteItem();
}

/*
 */
void
SpriteComponent::setPixelsPerUnit(float pixelsPerUnit)
{
  m_pixelsPerUnit = Math::max(pixelsPerUnit, MIN_PIXELS_PER_UNIT);
  updateSpriteItem();
}

/*
 */
void
SpriteComponent::setPivot(const Vector2& pivot)
{
  m_pivot = pivot;
  updateSpriteItem();
}

/*
 */
void
SpriteComponent::setColor(const LinearColor& color)
{
  m_color = color;
  updateSpriteItem();
}

/*
 */
void
SpriteComponent::setFlip(bool bFlipX, bool bFlipY)
{
  m_bFlipX = bFlipX;
  m_bFlipY = bFlipY;
  updateSpriteItem();
}

/*
 */
void
SpriteComponent::setSorting(int16 sortingLayer, int16 order)
{
  m_sortingLayer = sortingLayer;
  m_order = order;
  updateSpriteItem();
}

/*
 */
void
SpriteComponent::setBlendMode(SpriteBlendMode blendMode)
{
  m_blendMode = blendMode;
  updateSpriteItem();
}

/*
 */
void
SpriteComponent::setFilter(SpriteFilter filter)
{
  m_filter = filter;
  updateSpriteItem();
}

/*
 */
Vector2
SpriteComponent::getSize() const
{
  Vector2 pixels = m_sourceSize;
  if (pixels.x <= 0.0f || pixels.y <= 0.0f) {
    if (!m_texture || m_texture->getWidth() == 0 || m_texture->getHeight() == 0) {
      return Vector2(1.0f, 1.0f);
    }
    pixels = Vector2(static_cast<float>(m_texture->getWidth()),
                     static_cast<float>(m_texture->getHeight()));
  }
  return Vector2(pixels.x / m_pixelsPerUnit, pixels.y / m_pixelsPerUnit);
}

/*
 */
bool
SpriteComponent::getWorldBounds(AABox& outBounds) const
{
  if (!isRegistered()) {
    return false;
  }
  const GpuSpriteInstance& instance =
      getOwner()->getScene()->getSprite(m_spriteItemId).instance;
  const Vector3 extent = instance.halfAxisU.getAbs() + instance.halfAxisV.getAbs();
  outBounds = AABox(instance.center - extent, instance.center + extent);
  return true;
}

/*
 */
void
SpriteComponent::serialize(DataStream& stream) const
{
  const uint32 version = SERIALIZATION_VERSION;
  const SpriteFileData data{
      .texture = m_textureId,
      .sourcePosition = {m_sourcePosition.x, m_sourcePosition.y},
      .sourceSize = {m_sourceSize.x, m_sourceSize.y},
      .pixelsPerUnit = m_pixelsPerUnit,
      .pivot = {m_pivot.x, m_pivot.y},
      .color = {m_color.r, m_color.g, m_color.b, m_color.a},
      .sortingLayer = m_sortingLayer,
      .order = m_order,
      .blendMode = static_cast<uint8>(m_blendMode),
      .filter = static_cast<uint8>(m_filter),
      .flipX = static_cast<uint8>(m_bFlipX ? 1 : 0),
      .flipY = static_cast<uint8>(m_bFlipY ? 1 : 0)};
  stream.write(&version, sizeof(version));
  stream.write(&data, sizeof(data));
}

/*
 */
bool
SpriteComponent::deserialize(DataStream& stream)
{
  uint32 version = 0;
  SpriteFileData data;
  if (stream.read(&version, sizeof(version)) != sizeof(version) ||
      version != SERIALIZATION_VERSION || stream.read(&data, sizeof(data)) != sizeof(data) ||
      data.blendMode >= static_cast<uint8>(SpriteBlendMode::COUNT) ||
      data.filter >= static_cast<uint8>(SpriteFilter::COUNT) ||
      !Math::isFinite(data.pixelsPerUnit)) {
    return false;
  }

  m_sourcePosition = Vector2(data.sourcePosition[0], data.sourcePosition[1]);
  m_sourceSize = Vector2(Math::max(data.sourceSize[0], 0.0f),
                         Math::max(data.sourceSize[1], 0.0f));
  m_pixelsPerUnit = Math::max(data.pixelsPerUnit, MIN_PIXELS_PER_UNIT);
  m_pivot = Vector2(data.pivot[0], data.pivot[1]);
  m_color = LinearColor(data.color[0], data.color[1], data.color[2], data.color[3]);
  m_sortingLayer = data.sortingLayer;
  m_order = data.order;
  m_blendMode = static_cast<SpriteBlendMode>(data.blendMode);
  m_filter = static_cast<SpriteFilter>(data.filter);
  m_bFlipX = data.flipX != 0;
  m_bFlipY = data.flipY != 0;

  const SPtr<TextureAsset> texture = findTexture(data.texture);
  setTexture(texture);
  m_textureId = data.texture;
  if (!texture && !data.texture.isNull()) {
    CH_LOG_WARNING(SceneManagerLog, "A sprite lost its texture {0}.", data.texture);
  }
  return true;
}

/*
 */
void
SpriteComponent::collectReferences(Vector<UUID>& outReferences) const
{
  if (!m_textureId.isNull()) {
    outReferences.push_back(m_textureId);
  }
}

/*
 */
void
SpriteComponent::onRegister()
{
  m_spriteItemId = getOwner()->getScene()->addSprite(SpriteItem{.owner = getOwner()});
  updateSpriteItem();
}

/*
 */
void
SpriteComponent::onUnregister()
{
  getOwner()->getScene()->removeSprite(m_spriteItemId);
}

/*
 */
void
SpriteComponent::onTransformChanged()
{
  updateSpriteItem();
}

/*
 */
void
SpriteComponent::updateSpriteItem()
{
  if (!isRegistered()) {
    return;
  }

  // Rows of the world matrix are the object's axes in world space, scale included, and
  // the last row is its position.
  const Matrix4& world = getOwner()->getTransform().getWorldMatrix();
  const Vector3 right(world.at(1, 0), world.at(1, 1), world.at(1, 2));
  const Vector3 up(world.at(2, 0), world.at(2, 1), world.at(2, 2));
  const Vector3 origin(world.at(3, 0), world.at(3, 1), world.at(3, 2));

  const Vector2 size = getSize();
  const Vector3 halfU = right * (size.x * 0.5f);
  const Vector3 halfV = up * (size.y * 0.5f);

  SpriteItem& item = getOwner()->getScene()->getSprite(m_spriteItemId);
  GpuSpriteInstance& instance = item.instance;
  instance.center =
      origin - halfU * (m_pivot.x * 2.0f - 1.0f) - halfV * (m_pivot.y * 2.0f - 1.0f);
  instance.halfAxisU = halfU;
  instance.halfAxisV = halfV;

  SpriteDraw::makeUvRect(
      m_texture.get(), m_sourcePosition, m_sourceSize, m_bFlipX, m_bFlipY, instance.uvRect);
  instance.textureAndFlags = SpriteDraw::makeTextureFlags(m_texture.get(), m_filter);
  SpriteDraw::packColor(m_color, instance.colorRG, instance.colorBA);

  item.radius = halfU.magnitude() + halfV.magnitude();
  item.sortingLayer = m_sortingLayer;
  item.order = m_order;
  item.blendMode = m_blendMode;
}

} // namespace chEngineSDK
