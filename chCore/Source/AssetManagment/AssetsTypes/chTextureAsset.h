/************************************************************************/
/**
 * @file chTextureAsset.h
 * @author AccelMR
 * @date 2025/07/20
 * @brief
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"
#include "chTypeTraits.h"
#include "chIAsset.h"
#include "chGraphicsTypes.h"
#include "chITexture.h"

namespace chEngineSDK {
class ITexture;

// Written after the common asset start, before the pixels.
struct TextureAssetHeader
{
  static constexpr uint32 VERSION = 1;

  uint32 version = VERSION;
  uint32 width = 0;
  uint32 height = 0;
  Format format = Format::Unknown;
  // The pixels hold every level, one after the other, largest first.
  uint32 mipLevels = 1;
};
static_assert(sizeof(TextureAssetHeader) == 20, "TextureAssetHeader must have no padding");

#if USING(CH_EDITOR)
/**
 * How Package Project compresses a texture. Kept by name in the asset's editor data.
 */
enum class TextureCompression : uint32
{
  // BC7 for color, BC5 for normal maps, BC4 for gray linear images; textures without mips
  // (sprites, UI), floats and sizes that are not a multiple of 4 stay as they are.
  Auto = 0,
  // BC7, keeping alpha.
  Color = 1,
  // BC1, half the size of BC7, alpha dropped.
  ColorNoAlpha = 2,
  // BC5 from red and green; shaders rebuild the third axis.
  NormalMap = 3,
  // BC4 from red.
  Grayscale = 4,
  Uncompressed = 5,

  COUNT
};
#endif // USING(CH_EDITOR)

/**
 * @class TextureAsset
 * @brief Represents a texture asset in the engine.
 *
 * This class manages texture assets, providing functionality to load,
 * unload, and serialize texture data.
 */
class CH_CORE_EXPORT TextureAsset : public IAsset
{
 public:
  TextureAsset() = delete;
  TextureAsset(const AssetMetadata& metadata) : IAsset(metadata) {}

  /**
   * textureData holds mipLevels levels of the format, one after the other, largest first.
   */
  TextureAsset(const AssetMetadata& metadata,
               Vector<uint8> textureData,
               uint32 width,
               uint32 height,
               Format format,
               uint32 mipLevels);

  ~TextureAsset() = default;

  NODISCARD FORCEINLINE const SPtr<ITexture>&
  getTexture() const { return m_texture; }

  /**
   * Size of the largest level in pixels; zero until the texture is loaded.
   */
  NODISCARD FORCEINLINE uint32
  getWidth() const noexcept { return m_width; }

  NODISCARD FORCEINLINE uint32
  getHeight() const noexcept { return m_height; }

  NODISCARD FORCEINLINE Format
  getFormat() const noexcept { return m_format; }

#if USING(CH_EDITOR)
  /**
   * The pixels of the largest level as RGBA, 4 bytes each, for tools that read the image
   * (finding the sprites of a sheet). Empty when the format is not 8-bit RGBA or the CPU copy
   * is gone.
   */
  NODISCARD Span<const uint8>
  getRgba8Pixels() const;

  /**
   * Whether a pixel of the largest level has an alpha below alpha (0 to 1). Only 8-bit
   * RGBA and BGRA formats are read; other formats give false. It scans the CPU copy, so it
   * is meant for import time.
   */
  NODISCARD bool
  hasAlphaBelow(float alpha) const;

  /**
   * Every mip level one after the other, largest first; empty until the texture is loaded.
   */
  NODISCARD FORCEINLINE Span<const uint8>
  getTextureData() const noexcept { return m_textureData; }

  NODISCARD FORCEINLINE uint32
  getMipLevels() const noexcept { return m_mipLevels; }

  NODISCARD FORCEINLINE TextureCompression
  getCompression() const noexcept { return m_compression; }

  /**
   * Takes effect in the next package; save the asset to keep it.
   */
  FORCEINLINE void
  setCompression(TextureCompression compression) noexcept { m_compression = compression; }
#endif // USING(CH_EDITOR)

  /**
   * Whether a texture of this format can have this size: block compressed formats need a
   * largest level that is a whole number of blocks.
   */
  NODISCARD static bool
  isValidSize(Format format, uint32 width, uint32 height) noexcept;

 protected:
  bool
  serialize(SPtr<DataStream>) override;

  bool
  deserialize(SPtr<DataStream>) override;

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

  void
  createTextureFromData();

#if USING(CH_EDITOR)
  void
  writeEditorValues(ConfigFile& file) const override;

  void
  readEditorValues(const ConfigFile& file) override;
#endif // USING(CH_EDITOR)

 private:
  SPtr<ITexture> m_texture;
  Vector<uint8> m_textureData; ///< Compressed texture data for serialization
  uint32 m_width = 0; ///< Width of the texture
  uint32 m_height = 0; ///< Height of the texture
  Format m_format = Format::Unknown;
  uint32 m_mipLevels = 1;
  // What readAsync read on the load's worker, made live by finishAsyncRead.
  TextureAssetHeader m_pendingHeader{};
  Vector<uint8> m_pendingPixels;
#if USING(CH_EDITOR)
  TextureCompression m_compression = TextureCompression::Auto;
#endif // USING(CH_EDITOR)
};
DECLARE_ASSET_TYPE(TextureAsset);

} // namespace chEngineSDK
