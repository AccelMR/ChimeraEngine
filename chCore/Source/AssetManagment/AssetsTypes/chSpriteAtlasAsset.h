/************************************************************************/
/**
 * @file chSpriteAtlasAsset.h
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Asset that holds a SpriteAtlas and keeps its texture loaded.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chIAsset.h"
#include "chSpriteAtlas.h"
#include "chTypeTraits.h"

namespace chEngineSDK {
class TextureAsset;

/**
 * Exists to save a SpriteAtlas and share it between sprites. The texture is a reference, so
 * it loads before the atlas and stays loaded while the atlas is; sprites read it from here.
 */
class CH_CORE_EXPORT SpriteAtlasAsset : public IAsset
{
 public:
  SpriteAtlasAsset() = delete;

  SpriteAtlasAsset(const AssetMetadata& metadata) : IAsset(metadata) {}

  /**
   * For assets made in code (importers, tests); texture may be null.
   */
  SpriteAtlasAsset(const AssetMetadata& metadata,
                   SpriteAtlas atlas,
                   const SPtr<TextureAsset>& texture);

  ~SpriteAtlasAsset() = default;

  NODISCARD FORCEINLINE const SpriteAtlas&
  getAtlas() const noexcept { return m_atlas; }

  /**
   * Null without a texture, or while it is missing.
   */
  NODISCARD FORCEINLINE const SPtr<TextureAsset>&
  getTexture() const noexcept { return m_texture; }

#if USING(CH_EDITOR)
  /**
   * Replaces the atlas (the editor's edits); sprites using it see the change when they
   * update their frame again.
   */
  void
  setAtlas(SpriteAtlas atlas, const SPtr<TextureAsset>& texture);
#endif // USING(CH_EDITOR)

 protected:
  bool
  serialize(SPtr<DataStream> stream) override;

  bool
  deserialize(SPtr<DataStream> stream) override;

  void
  clearAssetData() override;

  void
  collectReferences(Vector<UUID>& outReferences) const override;

 private:
  SpriteAtlas m_atlas;
  SPtr<TextureAsset> m_texture;
};

DECLARE_ASSET_TYPE(SpriteAtlasAsset);

} // namespace chEngineSDK
