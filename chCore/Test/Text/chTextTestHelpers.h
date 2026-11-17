/************************************************************************/
/**
 * @file chTextTestHelpers.h
 * @author AccelMR
 * @date 2026/11/17
 * @brief Helpers that the font, text and script tests share.
 */
/************************************************************************/
#pragma once

#include "chFileSystem.h"
#include "chFloat16.h"
#include "chFont.h"
#include "chFontAsset.h"
#include "chPath.h"
#include "chTextLayout.h"
#include "chStringUtils.h"
#include "chUUID.h"

#include "catch.hpp"

namespace chEngineSDK {

/**
 * Makes the test font the default one while it lives, so texts without a font draw in tests
 * (where /Engine is not mounted).
 */
struct DefaultFontGuard
{
  explicit DefaultFontGuard(const SPtr<Font>& font) { Font::setDefault(font); }
  ~DefaultFontGuard() { Font::setDefault(nullptr); }
};

NODISCARD inline Vector<uint8>
readTestFont()
{
  return FileSystem::fastRead(FileSystem::absolutePath(Path(CH_TEST_FONT_FILE)));
}

NODISCARD inline SPtr<Font>
makeTestFont(const FontSettings& settings = {})
{
  Vector<uint8> data = readTestFont();
  REQUIRE_FALSE(data.empty());
  SPtr<Font> font = chMakeShared<Font>(std::move(data), settings);
  REQUIRE(font->isValid());
  return font;
}

NODISCARD inline float
glyphRight(const TextGlyph& glyph)
{
  return glyph.position.x + glyph.size.x;
}

NODISCARD inline float
lowHalf(uint32 packed)
{
  return Float16::toFloat(static_cast<uint16>(packed & 0xFFFFu));
}

NODISCARD inline float
highHalf(uint32 packed)
{
  return Float16::toFloat(static_cast<uint16>(packed >> 16));
}

NODISCARD inline SPtr<FontAsset>
makeTestFontAsset()
{
  AssetMetadata metadata;
  metadata.uuid = chEngineSDK::UUID::createRandom();
  metadata.assetType = AssetTypeTraits<FontAsset>::getTypeId();
  StringUtils::copyToBuffer(metadata.name, "TestFont");
  SPtr<FontAsset> asset = chMakeShared<FontAsset>(metadata, readTestFont(), FontSettings{});
  REQUIRE(asset->getFont());
  return asset;
}

} // namespace chEngineSDK
