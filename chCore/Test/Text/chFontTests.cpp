/************************************************************************/
/**
 * @file chFontTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of fonts, font assets and the glyph table.
 */
/************************************************************************/
#include "chAssetMeta.h"
#include "chFileStream.h"
#include "chFileSystem.h"
#include "chFloat16.h"
#include "chFont.h"
#include "chFontAsset.h"
#include "chGraphicsTypes.h"
#include "chMath.h"
#include "chSpriteItem.h"
#include "chStringUtils.h"
#include "chUUID.h"

#include "catch.hpp"
#include "chTextTestHelpers.h"

using namespace chEngineSDK;

namespace {

NODISCARD uint8
atlasPixel(const Font& font, uint32 x, uint32 y)
{
  return font.getAtlasPixels()[static_cast<SIZE_T>(y) * font.getAtlasWidth() + x];
}

class TestFontAsset : public FontAsset
{
 public:
  using FontAsset::FontAsset;
  using FontAsset::deserialize;
  using FontAsset::serialize;
};
} // namespace

TEST_CASE("chCore - Font", "[Text]")
{
  SECTION("Metrics and glyphs")
  {
    const SPtr<Font> font = makeTestFont();
    REQUIRE(font->getAscent() > 0.0f);
    REQUIRE(font->getDescent() < 0.0f);
    REQUIRE(font->getLineHeight() >= font->getAscent() - font->getDescent());
    // ASCII and Latin-1 are drawn when the font is made.
    REQUIRE(font->getGlyphCount() > 150);

    const uint32 count = font->getGlyphCount();
    const uint32 a = font->findGlyph('A');
    REQUIRE(font->findGlyph('A') == a);
    REQUIRE(font->getGlyphCount() == count);
    const FontGlyph glyphA = font->getGlyph(a);
    REQUIRE(glyphA.width > 0);
    REQUIRE(glyphA.height > 0);
    REQUIRE(glyphA.advance > 0.0f);

    const FontGlyph space = font->getGlyph(font->findGlyph(' '));
    REQUIRE(space.width == 0);
    REQUIRE(space.advance > 0.0f);

    // Private use characters of the last plane, which the font does not have, show the
    // fallback.
    const uint32 missing = font->findGlyph(0x10FFFC);
    REQUIRE(missing == font->findGlyph(0x10FFFD));
    REQUIRE(font->getGlyph(missing).width > 0);

    // Inside 'A' the field is past the edge.
    uint8 highest = 0;
    for (uint32 row = 0; row < glyphA.height; ++row) {
      for (uint32 column = 0; column < glyphA.width; ++column) {
        highest = Math::max(highest, atlasPixel(*font, glyphA.atlasX + column, glyphA.atlasY + row));
      }
    }
    REQUIRE(highest > 128);
  }

  SECTION("Characters past Latin-1 are drawn when asked for")
  {
    const SPtr<Font> font = makeTestFont();
    const uint32 count = font->getGlyphCount();
    // Euro sign.
    const uint32 euro = font->findGlyph(0x20AC);
    REQUIRE(font->getGlyphCount() == count + 1);
    REQUIRE(font->findGlyph(0x20AC) == euro);
  }

  SECTION("Kerning and digits")
  {
    const SPtr<Font> font = makeTestFont();
    const uint32 a = font->findGlyph('A');
    const uint32 v = font->findGlyph('V');
    const float kerning = font->getKerning(a, v);
    REQUIRE(kerning < 0.0f);
    REQUIRE(font->getKerning(a, v) == kerning);
    const float digitAdvance = font->getDigitAdvance();
    for (WCHAR32 digit = '0'; digit <= '9'; ++digit) {
      REQUIRE(font->getGlyph(font->findGlyph(digit)).advance <= digitAdvance);
    }
  }

  SECTION("The atlas grows and keeps what it drew")
  {
    const SPtr<Font> font =
        makeTestFont({.rasterSize = 200, .bPreloadAscii = false, .bPreloadLatin1 = false});
    REQUIRE(font->getAtlasWidth() == Font::INITIAL_ATLAS_SIZE);
    const FontGlyph glyph = font->getGlyph(font->findGlyph('W'));
    Vector<uint8> before;
    for (uint32 row = 0; row < glyph.height; ++row) {
      for (uint32 column = 0; column < glyph.width; ++column) {
        before.push_back(atlasPixel(*font, glyph.atlasX + column, glyph.atlasY + row));
      }
    }
    const uint32 revision = font->getRevision();
    for (WCHAR32 character = 'a'; character <= 'z'; ++character) {
      CH_PARAMETER_UNUSED(font->findGlyph(character));
    }
    REQUIRE(font->getAtlasWidth() * font->getAtlasHeight() >
            Font::INITIAL_ATLAS_SIZE * Font::INITIAL_ATLAS_SIZE);
    REQUIRE(font->getRevision() > revision);
    REQUIRE(font->getAtlasPixels().size() ==
            static_cast<SIZE_T>(font->getAtlasWidth()) * font->getAtlasHeight());
    SIZE_T index = 0;
    for (uint32 row = 0; row < glyph.height; ++row) {
      for (uint32 column = 0; column < glyph.width; ++column) {
        REQUIRE(atlasPixel(*font, glyph.atlasX + column, glyph.atlasY + row) == before[index++]);
      }
    }
  }

  SECTION("Pixel fonts store coverage")
  {
    const SPtr<Font> font = makeTestFont({.mode = FontRenderMode::Pixel, .rasterSize = 16});
    const FontGlyph glyph = font->getGlyph(font->findGlyph('H'));
    REQUIRE(glyph.width > 0);
    REQUIRE(glyph.height <= 16);
    // Unlike a distance field, the corner of the box is empty and the stems are covered.
    uint8 highest = 0;
    for (uint32 row = 0; row < glyph.height; ++row) {
      for (uint32 column = 0; column < glyph.width; ++column) {
        highest = Math::max(highest, atlasPixel(*font, glyph.atlasX + column, glyph.atlasY + row));
      }
    }
    REQUIRE(highest > 200);
    REQUIRE(atlasPixel(*font, glyph.atlasX, glyph.atlasY + glyph.height / 2) < 255);
  }

  SECTION("Data that is not a font")
  {
    Font font(Vector<uint8>(64, 7), {});
    REQUIRE_FALSE(font.isValid());
    REQUIRE(font.prepareGpuAtlas() == GpuSpriteInstance::NO_TEXTURE);
  }
}

TEST_CASE("chCore - Font asset", "[Text]")
{
  AssetMetadata metadata;
  metadata.uuid = chEngineSDK::UUID::createRandom();
  metadata.assetType = AssetTypeTraits<FontAsset>::getTypeId();
  StringUtils::copyToBuffer(metadata.name, "Inter");

  const FontSettings settings{.mode = FontRenderMode::Sdf,
                              .rasterSize = 40,
                              .sdfSpread = 6,
                              .bPreloadAscii = true,
                              .bPreloadLatin1 = false,
                              .preloadCharacters = "\xE2\x82\xAC"};
  TestFontAsset asset(metadata, readTestFont(), settings);
  REQUIRE(asset.getFont());

  const SPtr<MemoryDataStream> stream = chMakeShared<MemoryDataStream>();
  REQUIRE(asset.serialize(stream));
  const SIZE_T size = stream->size();

  SECTION("Round trip")
  {
    stream->seek(0);
    TestFontAsset loaded(metadata);
    REQUIRE(loaded.deserialize(stream));
    const FontSettings& read = loaded.getSettings();
    REQUIRE(read.mode == FontRenderMode::Sdf);
    REQUIRE(read.rasterSize == 40);
    REQUIRE(read.sdfSpread == 6);
    REQUIRE(read.bPreloadAscii);
    REQUIRE_FALSE(read.bPreloadLatin1);
    REQUIRE(read.preloadCharacters == "\xE2\x82\xAC");
    REQUIRE(loaded.getFont());
    REQUIRE(loaded.getFont()->getFontData().size() == asset.getFont()->getFontData().size());
  }

  SECTION("A cut file is refused")
  {
    stream->seek(0);
    Vector<uint8> bytes(size / 2);
    REQUIRE(stream->read(bytes.data(), bytes.size()) == bytes.size());
    const SPtr<MemoryDataStream> half = chMakeShared<MemoryDataStream>();
    REQUIRE(half->write(bytes.data(), bytes.size()) == bytes.size());
    half->seek(0);
    TestFontAsset loaded(metadata);
    REQUIRE_FALSE(loaded.deserialize(half));
  }
}

TEST_CASE("chCore - Font glyph table", "[Text]")
{
  const SPtr<Font> font = makeTestFont();
  Span<const GpuGlyph> table = font->getGpuGlyphs();
  REQUIRE(table.size() == font->getGlyphCount());

  const uint32 slot = font->findGlyph('W');
  const FontGlyph& glyph = font->getGlyph(slot);
  const float height = font->getHeight();
  table = font->getGpuGlyphs();
  const GpuGlyph& gpu = table[slot];
  REQUIRE(gpu.uvRect[0] == Approx(static_cast<float>(glyph.atlasX) / font->getAtlasWidth()));
  REQUIRE(gpu.uvRect[3] == Approx(static_cast<float>(glyph.height) / font->getAtlasHeight()));
  REQUIRE(gpu.box[0] == Approx(glyph.offsetX / height));
  REQUIRE(gpu.box[1] == Approx((glyph.offsetY - glyph.height) / height));
  REQUIRE(gpu.box[2] == Approx(glyph.width / height));

  // A new glyph is added at the end of the table.
  const uint32 euro = font->findGlyph(0x20AC);
  REQUIRE(font->getGpuGlyphs().size() == euro + 1);
  // Without a graphics API there is no buffer.
  REQUIRE(font->prepareGpuGlyphTable() == 0);

  SECTION("A bigger atlas moves every texture coordinate")
  {
    const SPtr<Font> big =
        makeTestFont({.rasterSize = 200, .bPreloadAscii = false, .bPreloadLatin1 = false});
    const uint32 w = big->findGlyph('W');
    const float uvTop = big->getGpuGlyphs()[w].uvRect[3];
    for (WCHAR32 character = 'a'; character <= 'z'; ++character) {
      CH_PARAMETER_UNUSED(big->findGlyph(character));
    }
    REQUIRE(big->getGpuGlyphs()[w].uvRect[3] < uvTop);
    REQUIRE(big->getGpuGlyphs().size() == big->getGlyphCount());
  }
}
