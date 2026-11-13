/************************************************************************/
/**
 * @file chSpriteAtlasAsset.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Asset that holds a SpriteAtlas and keeps its texture loaded.
 */
/************************************************************************/
#include "chSpriteAtlasAsset.h"

#include "chAssetManager.h"
#include "chFileStream.h"
#include "chLogger.h"
#include "chTextureAsset.h"

namespace chEngineSDK {

/*
 */
SpriteAtlasAsset::SpriteAtlasAsset(const AssetMetadata& metadata,
                                   SpriteAtlas atlas,
                                   const SPtr<TextureAsset>& texture)
  : IAsset(metadata),
    m_atlas(std::move(atlas)),
    m_texture(texture)
{
  m_atlas.setTextureId(texture ? texture->getUUID() : UUID::null());
}

#if USING(CH_EDITOR)
/*
 */
void
SpriteAtlasAsset::setAtlas(SpriteAtlas atlas, const SPtr<TextureAsset>& texture)
{
  m_atlas = std::move(atlas);
  m_texture = texture;
  m_atlas.setTextureId(texture ? texture->getUUID() : UUID::null());
}
#endif // USING(CH_EDITOR)

/*
 */
bool
SpriteAtlasAsset::serialize(SPtr<DataStream> stream)
{
  if (!stream) {
    return false;
  }
  m_atlas.serialize(*stream);
  return true;
}

/*
 */
bool
SpriteAtlasAsset::deserialize(SPtr<DataStream> stream)
{
  String error;
  if (!stream || !m_atlas.deserialize(*stream, error)) {
    CH_LOG_ERROR(AssetSystem, "Failed to read sprite atlas {0}: {1}", getName(), error);
    return false;
  }

  // IAsset::load already loaded the references, so the texture is ready when it exists.
  const UUID& textureId = m_atlas.getTextureId();
  m_texture.reset();
  if (!textureId.isNull() && AssetManager::isStarted()) {
    const SPtr<IAsset> asset = AssetManager::instance().getAsset(textureId);
    m_texture = asset ? asset->as<TextureAsset>() : nullptr;
  }
  if (!m_texture && !textureId.isNull()) {
    CH_LOG_WARNING(AssetSystem, "Sprite atlas {0} lost its texture {1}", getName(), textureId);
  }
  return true;
}

/*
 */
void
SpriteAtlasAsset::clearAssetData()
{
  m_atlas = SpriteAtlas();
  m_texture.reset();
}

/*
 */
void
SpriteAtlasAsset::collectReferences(Vector<UUID>& outReferences) const
{
  if (!m_atlas.getTextureId().isNull()) {
    outReferences.push_back(m_atlas.getTextureId());
  }
}

} // namespace chEngineSDK
