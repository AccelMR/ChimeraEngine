/************************************************************************/
/**
 * @file chGraphicsTypesTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of the vertex layouts, the pipeline description hash and the formats.
 */
/************************************************************************/
#include <cstddef>

#include "chGraphicsTypes.h"
#include "chIShader.h"
#include "chMath.h"
#include "chTextureAsset.h"
#include "chUUID.h"
#include "chVertexLayout.h"

#include "catch.hpp"

using namespace chEngineSDK;

TEST_CASE("chCore - VertexLayout", "[Graphics]")
{
  // Each predefined layout must describe its vertex struct byte for byte, or the GPU reads
  // the vertices with the wrong offsets.
  REQUIRE(VertexPosColor::getLayout().getVertexSize() == sizeof(VertexPosColor));
  REQUIRE(VertexStatic::getLayout().getVertexSize() == sizeof(VertexStatic));

  const VertexLayout layout = VertexStatic::getLayout();
  const Vector<VertexAttributeDesc>& attributes = layout.getAttributes();
  REQUIRE(attributes.size() == 4);
  REQUIRE(attributes[0].offset == offsetof(VertexStatic, position));
  REQUIRE(attributes[1].offset == offsetof(VertexStatic, normal));
  REQUIRE(attributes[2].offset == offsetof(VertexStatic, texCoord));
  REQUIRE(attributes[3].offset == offsetof(VertexStatic, tangent));
  REQUIRE(attributes[3].format == VertexFormat::Float4);

  // Without an explicit offset, each attribute goes right after the previous one, and each
  // binding has its own stride.
  VertexLayout custom;
  custom.addAttribute(VertexAttributeType::Position, VertexFormat::Float3);
  custom.addAttribute(VertexAttributeType::TexCoord0, VertexFormat::Float2);
  custom.addAttribute(VertexAttributeType::Color, VertexFormat::Float4, UINT32_MAX, 1);
  REQUIRE(custom.getAttributes()[1].offset == 12);
  REQUIRE(custom.getAttributes()[2].offset == 0);
  REQUIRE(custom.getVertexSize() == 20);
  REQUIRE(custom.getStride(0) == 20);
  REQUIRE(custom.getStride(1) == 16);
  REQUIRE(custom.getBindingCount() == 2);

  // Packed formats count their real size (Dear ImGui's vertex is two Float2 and a color).
  VertexLayout packed;
  packed.addAttribute(VertexAttributeType::Position, VertexFormat::Float2);
  packed.addAttribute(VertexAttributeType::TexCoord0, VertexFormat::Float2);
  packed.addAttribute(VertexAttributeType::Color, VertexFormat::UByte4Normalized);
  packed.addAttribute(VertexAttributeType::Custom, VertexFormat::Short2Normalized);
  packed.addAttribute(VertexAttributeType::Custom, VertexFormat::UInt3);
  REQUIRE(packed.getAttributes()[3].offset == 20);
  REQUIRE(packed.getAttributes()[4].offset == 24);
  REQUIRE(packed.getStride(0) == 36);
}

namespace {
// The pipeline key only uses shader identity, so a shader needs no GPU code here.
class TestShader : public IShader
{
 public:
  chEngineSDK::UUID
  getShaderId() const override
  {
    return chEngineSDK::UUID::null();
  }
};

GraphicsPipelineDesc
makeTestPipelineDesc(const SPtr<IShader>& vertexShader, const SPtr<IShader>& fragmentShader)
{
  GraphicsPipelineDesc desc{.vertexShader = vertexShader,
                            .fragmentShader = fragmentShader,
                            .vertexLayout = VertexLayout::createStaticLayout(),
                            .colorAttachmentCount = 1,
                            .depthFormat = Format::D32_SFLOAT};
  desc.colorFormats[0] = Format::R8G8B8A8_UNORM;
  return desc;
}
} // namespace

TEST_CASE("chCore - GraphicsPipelineDesc hash", "[Graphics]")
{
  const SPtr<IShader> vertexShader = chMakeShared<TestShader>();
  const SPtr<IShader> fragmentShader = chMakeShared<TestShader>();
  const GraphicsPipelineDesc base = makeTestPipelineDesc(vertexShader, fragmentShader);

  SECTION("Equal descriptions give equal keys")
  {
    REQUIRE(base.getHash() == makeTestPipelineDesc(vertexShader, fragmentShader).getHash());
  }

  SECTION("Every kind of state changes the key")
  {
    GraphicsPipelineDesc otherShader = base;
    otherShader.fragmentShader = chMakeShared<TestShader>();
    REQUIRE(otherShader.getHash() != base.getHash());

    GraphicsPipelineDesc otherLayout = base;
    otherLayout.vertexLayout = VertexLayout::createPostionColorLayout();
    REQUIRE(otherLayout.getHash() != base.getHash());

    GraphicsPipelineDesc otherCull = base;
    otherCull.raster.cullMode = CullMode::None;
    REQUIRE(otherCull.getHash() != base.getHash());

    GraphicsPipelineDesc otherDepth = base;
    otherDepth.depth.writeEnable = false;
    REQUIRE(otherDepth.getHash() != base.getHash());

    GraphicsPipelineDesc otherBlend = base;
    otherBlend.blendStates[0].enable = true;
    REQUIRE(otherBlend.getHash() != base.getHash());

    GraphicsPipelineDesc otherFormat = base;
    otherFormat.colorFormats[0] = Format::R16G16B16A16_SFLOAT;
    REQUIRE(otherFormat.getHash() != base.getHash());

    GraphicsPipelineDesc moreTargets = base;
    moreTargets.colorAttachmentCount = 2;
    moreTargets.colorFormats[1] = Format::R8G8B8A8_UNORM;
    REQUIRE(moreTargets.getHash() != base.getHash());
  }

  SECTION("Unused attachment slots do not change the key")
  {
    GraphicsPipelineDesc unusedSlot = base;
    unusedSlot.colorFormats[3] = Format::R16G16B16A16_SFLOAT;
    unusedSlot.blendStates[3].enable = true;
    REQUIRE(unusedSlot.getHash() == base.getHash());
  }
}

TEST_CASE("chCore - FormatUtils", "[Graphics]")
{
  // Texture assets store these values, so they must never move.
  STATIC_REQUIRE(static_cast<uint32>(Format::R8G8B8A8_UNORM) == 1);
  STATIC_REQUIRE(static_cast<uint32>(Format::D24_UNORM_S8_UINT) == 6);

  SECTION("Every format but Unknown has a size")
  {
    REQUIRE(FormatUtils::getInfo(Format::Unknown).bytesPerBlock == 0);
    for (uint32 i = 1; i < static_cast<uint32>(Format::COUNT); ++i) {
      REQUIRE(FormatUtils::getInfo(static_cast<Format>(i)).bytesPerBlock > 0);
    }
  }

  SECTION("Depth and stencil")
  {
    REQUIRE(FormatUtils::isDepth(Format::D32_SFLOAT));
    REQUIRE_FALSE(FormatUtils::getInfo(Format::D32_SFLOAT).hasStencil);
    REQUIRE(FormatUtils::getInfo(Format::D24_UNORM_S8_UINT).hasStencil);
    REQUIRE(FormatUtils::getInfo(Format::D32_SFLOAT_S8_UINT).hasStencil);
    REQUIRE_FALSE(FormatUtils::isDepth(Format::R8G8B8A8_SRGB));
    REQUIRE(FormatUtils::getInfo(Format::R8G8B8A8_SRGB).isSrgb);
  }

  SECTION("Mip size")
  {
    REQUIRE(FormatUtils::getMipSize(Format::R8G8B8A8_UNORM, 4, 2) == 32);
    REQUIRE(FormatUtils::getMipSize(Format::R8_UNORM, 3, 3) == 9);
    REQUIRE(FormatUtils::getMipSize(Format::R32G32B32A32_SFLOAT, 1, 1, 2) == 32);
  }

  SECTION("Block compressed formats")
  {
    STATIC_REQUIRE(static_cast<uint32>(Format::BC1_RGBA_UNORM) == 18);
    STATIC_REQUIRE(static_cast<uint32>(Format::BC7_SRGB) == 23);
    REQUIRE(FormatUtils::isCompressed(Format::BC7_UNORM));
    REQUIRE_FALSE(FormatUtils::isCompressed(Format::R8G8B8A8_UNORM));
    REQUIRE(FormatUtils::getInfo(Format::BC1_RGBA_SRGB).isSrgb);
    REQUIRE(FormatUtils::getInfo(Format::BC7_SRGB).isSrgb);
    REQUIRE_FALSE(FormatUtils::getInfo(Format::BC5_UNORM).isSrgb);

    // A level smaller than a block or not a whole number of blocks still takes whole blocks.
    REQUIRE(FormatUtils::getMipSize(Format::BC1_RGBA_UNORM, 4, 4) == 8);
    REQUIRE(FormatUtils::getMipSize(Format::BC4_UNORM, 8, 4) == 16);
    REQUIRE(FormatUtils::getMipSize(Format::BC7_UNORM, 5, 5) == 64);
    REQUIRE(FormatUtils::getMipSize(Format::BC5_UNORM, 1, 1) == 16);
    REQUIRE(FormatUtils::getMipSize(Format::BC7_SRGB, 2, 1) == 16);

    // A whole chain of 16x8: 16x8, 8x4, 4x2, 2x1, 1x1 is 8 + 2 + 1 + 1 + 1 blocks.
    SIZE_T chainSize = 0;
    for (uint32 mip = 0; mip < 5; ++mip) {
      chainSize += FormatUtils::getMipSize(Format::BC7_UNORM, Math::max(16u >> mip, 1u),
                                           Math::max(8u >> mip, 1u));
    }
    REQUIRE(chainSize == 13 * 16);
  }

  SECTION("Texture sizes a format allows")
  {
    REQUIRE(TextureAsset::isValidSize(Format::R8G8B8A8_UNORM, 3, 5));
    REQUIRE(TextureAsset::isValidSize(Format::BC7_UNORM, 8, 12));
    REQUIRE_FALSE(TextureAsset::isValidSize(Format::BC7_UNORM, 6, 8));
    REQUIRE_FALSE(TextureAsset::isValidSize(Format::BC1_RGBA_UNORM, 4, 2));
    REQUIRE_FALSE(TextureAsset::isValidSize(Format::R8G8B8A8_UNORM, 0, 4));
  }
}
