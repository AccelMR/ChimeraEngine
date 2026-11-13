/************************************************************************/
/**
 * @file chFontCodec.h
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Codec that imports TrueType and OpenType files as font assets.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chAssetCodec.h"
#include "chFontAsset.h"
#include "chTypeTraits.h"

namespace chEngineSDK {

/**
 * Exists so the editor turns a font file into a FontAsset, which keeps the file inside the
 * project. Nothing is converted: the glyphs are drawn when the game asks for them, with the
 * settings the asset keeps (edited in the Asset Inspector).
 */
class FontCodec : public IAssetCodec
{
 public:
  NODISCARD UUID
  getCodecType() const override
  {
    static const UUID codecType = UUID::createFromName("FontCodec");
    return codecType;
  }

  NODISCARD Vector<UUID>
  getSupportedAssetTypes() const override
  {
    return {AssetTypeTraits<FontAsset>::getTypeId()};
  }

  NODISCARD const Vector<String>&
  getSupportedExtensions() const override
  {
    return m_extensions;
  }

  NODISCARD UniquePtr<IImportTask>
  createImportTask(const Path& filePath,
                   const String& assetName,
                   const ImportSettings& settings) override;

 private:
  const Vector<String> m_extensions = {"ttf", "otf", "ttc"};
};
DECLARE_ASSET_TYPE(FontCodec);

} // namespace chEngineSDK
