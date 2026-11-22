/************************************************************************/
/**
 * @file chFontAsset.h
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Asset that holds a font file and how its glyphs are drawn.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chFont.h"
#include "chIAsset.h"
#include "chTypeTraits.h"

namespace chEngineSDK {

// Written after the common asset start, before the preload characters and the font file.
struct FontAssetHeader
{
  static constexpr uint32 VERSION = 1;
  static constexpr uint32 PRELOAD_ASCII_BIT = 0x1u;
  static constexpr uint32 PRELOAD_LATIN1_BIT = 0x2u;

  uint32 version = VERSION;
  // FontRenderMode
  uint32 mode = 0;
  uint32 rasterSize = 0;
  uint32 sdfSpread = 0;
  uint32 flags = 0;
  uint32 fontDataSize = 0;
};
static_assert(sizeof(FontAssetHeader) == 24, "FontAssetHeader must have no padding");

/**
 * Exists to keep a TrueType or OpenType file inside the project, so the game reads its
 * glyphs without the original file; the Font is made when the asset loads and draws its
 * glyphs as texts ask for them.
 */
class CH_CORE_EXPORT FontAsset : public IAsset
{
 public:
  FontAsset() = delete;

  FontAsset(const AssetMetadata& metadata) : IAsset(metadata) {}

  /**
   * For assets made in code (importers, tests).
   */
  FontAsset(const AssetMetadata& metadata, Vector<uint8> fontData, const FontSettings& settings);

  ~FontAsset() = default;

  /**
   * Null until loaded, and when the file is not a font.
   */
  NODISCARD FORCEINLINE const SPtr<Font>&
  getFont() const noexcept
  {
    return m_font;
  }

  NODISCARD FORCEINLINE const FontSettings&
  getSettings() const noexcept
  {
    return m_settings;
  }

#if USING(CH_EDITOR)
  /**
   * Makes the font again with other settings; texts using it see the change once they lay
   * themselves out again.
   */
  void
  setSettings(const FontSettings& settings);
#endif // USING(CH_EDITOR)

  // Above any real font file; only a broken asset asks for more.
  static constexpr uint32 MAX_FONT_DATA_SIZE = 64 * 1024 * 1024;
  static constexpr uint32 MAX_PRELOAD_CHARACTERS_SIZE = 64 * 1024;

 protected:
  bool
  serialize(SPtr<DataStream> stream) override;

  bool
  deserialize(SPtr<DataStream> stream) override;

  void
  clearAssetData() override;

  NODISCARD bool
  hasAsyncRead() const override
  {
    return true;
  }

  bool
  readAsync(Vector<uint8>& fileBytes, SIZE_T dataOffset, String& outError) override;

  bool
  finishAsyncRead() override;

  /**
   * Reads the header, the preload characters and the font file after them.
   */
  bool
  readFont(DataStream& stream, FontSettings& outSettings, Vector<uint8>& outFontData) const;

 private:
  void
  makeFont(Vector<uint8> fontData);

  SPtr<Font> m_font;
  FontSettings m_settings;
  // Made by readAsync on the load's worker.
  SPtr<Font> m_pendingFont;
  FontSettings m_pendingSettings;
};

DECLARE_ASSET_TYPE(FontAsset);

} // namespace chEngineSDK
