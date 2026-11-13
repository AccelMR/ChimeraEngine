/************************************************************************/
/**
 * @file chFontAsset.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Asset that holds a font file and how its glyphs are drawn.
 */
/************************************************************************/
#include "chFontAsset.h"

#include "chAssetFile.h"
#include "chAssetManager.h"
#include "chFileStream.h"
#include "chLogger.h"

namespace chEngineSDK {

/*
 */
FontAsset::FontAsset(const AssetMetadata& metadata,
                     Vector<uint8> fontData,
                     const FontSettings& settings)
  : IAsset(metadata),
    m_settings(settings)
{
  makeFont(std::move(fontData));
}

#if USING(CH_EDITOR)
/*
 */
void
FontAsset::setSettings(const FontSettings& settings)
{
  Vector<uint8> fontData;
  if (m_font) {
    const Span<const uint8> data = m_font->getFontData();
    fontData.assign(data.begin(), data.end());
  }
  m_settings = settings;
  makeFont(std::move(fontData));
}
#endif // USING(CH_EDITOR)

/*
 */
bool
FontAsset::serialize(SPtr<DataStream> stream)
{
  if (!stream || !m_font) {
    return false;
  }
  const Span<const uint8> fontData = m_font->getFontData();
  FontAssetHeader header{
      .mode = static_cast<uint32>(m_settings.mode),
      .rasterSize = m_settings.rasterSize,
      .sdfSpread = m_settings.sdfSpread,
      .flags = (m_settings.bPreloadAscii ? FontAssetHeader::PRELOAD_ASCII_BIT : 0u) |
               (m_settings.bPreloadLatin1 ? FontAssetHeader::PRELOAD_LATIN1_BIT : 0u),
      .fontDataSize = static_cast<uint32>(fontData.size())};
  *stream << header;
  AssetFile::writeString(*stream, m_settings.preloadCharacters);
  return stream->write(fontData.data(), fontData.size()) == fontData.size();
}

/*
 */
bool
FontAsset::deserialize(SPtr<DataStream> stream)
{
  FontAssetHeader header;
  if (!stream || !AssetFile::readValue(*stream, header)) {
    CH_LOG_ERROR(AssetSystem, "Font {0} is cut short.", getName());
    return false;
  }
  if (header.version != FontAssetHeader::VERSION) {
    CH_LOG_ERROR(AssetSystem,
                 "Font {0} has version {1}, this build reads {2}; import it again.",
                 getName(),
                 header.version,
                 FontAssetHeader::VERSION);
    return false;
  }
  if (header.mode >= static_cast<uint32>(FontRenderMode::COUNT)) {
    CH_LOG_ERROR(AssetSystem, "Font {0} has an unknown render mode {1}.", getName(), header.mode);
    return false;
  }

  FontSettings settings{.mode = static_cast<FontRenderMode>(header.mode),
                        .rasterSize = header.rasterSize,
                        .sdfSpread = header.sdfSpread,
                        .bPreloadAscii = (header.flags & FontAssetHeader::PRELOAD_ASCII_BIT) != 0,
                        .bPreloadLatin1 =
                            (header.flags & FontAssetHeader::PRELOAD_LATIN1_BIT) != 0};
  if (!AssetFile::readString(*stream, settings.preloadCharacters, MAX_PRELOAD_CHARACTERS_SIZE)) {
    CH_LOG_ERROR(AssetSystem, "Font {0} has broken preload characters.", getName());
    return false;
  }
  if (header.fontDataSize == 0 || header.fontDataSize > MAX_FONT_DATA_SIZE ||
      header.fontDataSize > AssetFile::getBytesLeft(*stream)) {
    CH_LOG_ERROR(AssetSystem, "Font {0} is cut short.", getName());
    return false;
  }
  Vector<uint8> fontData(header.fontDataSize);
  if (!AssetFile::readBytes(*stream, fontData.data(), fontData.size())) {
    CH_LOG_ERROR(AssetSystem, "Font {0} is cut short.", getName());
    return false;
  }

  m_settings = std::move(settings);
  makeFont(std::move(fontData));
  return m_font != nullptr;
}

/*
 */
void
FontAsset::clearAssetData()
{
  m_font.reset();
}

/*
 */
void
FontAsset::makeFont(Vector<uint8> fontData)
{
  m_font = chMakeShared<Font>(std::move(fontData), m_settings);
  if (!m_font->isValid()) {
    CH_LOG_ERROR(AssetSystem, "Font {0} does not hold a TrueType or OpenType file.", getName());
    m_font.reset();
  }
}

} // namespace chEngineSDK
