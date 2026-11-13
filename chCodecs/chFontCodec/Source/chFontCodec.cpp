/************************************************************************/
/**
 * @file chFontCodec.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Codec that imports TrueType and OpenType files as font assets.
 */
/************************************************************************/
#include "chFontCodec.h"

#include "chFileSystem.h"
#include "chFont.h"
#include "chLogger.h"

namespace chEngineSDK {

CH_LOG_DECLARE_STATIC(FontCodecLog, All);

namespace {
/**
 * Exists so the file is read and checked on the import's worker thread, and only the asset
 * is made on the main thread.
 */
class FontImportTask final : public IImportTask
{
 public:
  FontImportTask(const Path& filePath, const String& assetName, const ImportSettings& settings)
    : m_filePath(filePath),
      m_assetName(assetName),
      m_settings(settings)
  {}

  NODISCARD bool
  readSource(ImportProgress& progress) override
  {
    progress.set("Reading the font", -1.0f);
    m_fontData = FileSystem::fastRead(m_filePath);
    if (m_fontData.empty()) {
      CH_LOG_ERROR(FontCodecLog, "The font {0} could not be read.", m_filePath);
      return false;
    }
    if (m_fontData.size() > FontAsset::MAX_FONT_DATA_SIZE) {
      CH_LOG_ERROR(FontCodecLog, "The font {0} is bigger than fonts can be.", m_filePath);
      return false;
    }
    // Made once without glyphs, only to know the file is a font before an asset exists.
    const Font check(m_fontData,
                     {.bPreloadAscii = false, .bPreloadLatin1 = false});
    if (!check.isValid()) {
      CH_LOG_ERROR(FontCodecLog, "{0} is not a TrueType or OpenType font.", m_filePath);
      return false;
    }
    return !progress.isCancelled();
  }

  SPtr<IAsset>
  createAssets(const Path& assetFolder) override
  {
    const AssetMetadata metadata = IAssetCodec::makeMetadata<FontAsset>(m_assetName, assetFolder);
    const SPtr<FontAsset> asset =
        chMakeShared<FontAsset>(metadata, std::move(m_fontData), FontSettings{});
    if (!asset->getFont() ||
        !IAssetCodec::saveAndRegister(asset, FileSystem::absolutePath(m_filePath), StringView(),
                                      m_settings)) {
      CH_LOG_ERROR(FontCodecLog, "Failed to save font asset: {0}", m_assetName);
      return nullptr;
    }
    CH_LOG_INFO(FontCodecLog, "Imported font {0} from {1}", asset->getName(), m_filePath);
    return asset;
  }

 private:
  Path m_filePath;
  String m_assetName;
  ImportSettings m_settings;
  Vector<uint8> m_fontData;
};
} // namespace

/*
 */
UniquePtr<IImportTask>
FontCodec::createImportTask(const Path& filePath,
                            const String& assetName,
                            const ImportSettings& settings)
{
  return chMakeUnique<FontImportTask>(filePath, assetName, settings);
}

} // namespace chEngineSDK
