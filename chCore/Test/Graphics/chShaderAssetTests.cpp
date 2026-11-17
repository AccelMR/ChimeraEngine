/************************************************************************/
/**
 * @file chShaderAssetTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of shader parameters and shader assets.
 */
/************************************************************************/
#include <cstring>

#include "chFileStream.h"
#include "chShaderAsset.h"
#include "chShaderParameters.h"
#include "chStringUtils.h"
#include "chUUID.h"

#include "catch.hpp"

using namespace chEngineSDK;

namespace {

class TestShaderAsset : public ShaderAsset
{
 public:
  using ShaderAsset::ShaderAsset;
  using ShaderAsset::deserialize;
  using ShaderAsset::serialize;
};

NODISCARD AssetMetadata
makeShaderMetadata()
{
  AssetMetadata metadata;
  metadata.uuid = chEngineSDK::UUID::createRandom();
  metadata.assetType = AssetTypeTraits<ShaderAsset>::getTypeId();
  StringUtils::copyToBuffer(metadata.name, "PP_Test");
  return metadata;
}

NODISCARD ShaderAssetData
makeShaderData()
{
  ShaderAssetData data;
  data.stage = PostProcessStage::BeforeTonemap;
  data.parameters.push_back({.name = "intensity",
                             .type = ShaderParameterType::Float,
                             .defaultValue = Vector4(1.5f, 0.0f, 0.0f, 0.0f),
                             .minimum = 0.0f,
                             .maximum = 2.0f,
                             .offset = 0,
                             .tooltip = "How strong"});
  data.parameters.push_back({.name = "tint",
                             .type = ShaderParameterType::Color,
                             .defaultValue = Vector4(1.0f, 0.5f, 0.25f, 1.0f),
                             .minimum = 0.0f,
                             .maximum = 0.0f,
                             .offset = 0,
                             .tooltip = ""});
  data.parameterBufferSize = ShaderParameters::computeLayout(data.parameters);
  data.interfaceVersion = ShaderAsset::INTERFACE_VERSION;
  data.binaries.push_back({.format = "SPIRV",
                           .vertexCode = {},
                           .pixelCode = Vector<uint8>(64, 3)});
  data.binaries.push_back({.format = "DXIL",
                           .vertexCode = Vector<uint8>(16, 5),
                           .pixelCode = Vector<uint8>(80, 9)});
  return data;
}

} // namespace

TEST_CASE("chCore - Shader parameters", "[Graphics]")
{
#if USING(CH_EDITOR)
  SECTION("Declarations are read from comment lines")
  {
    const StringView source =
        "#include \"chPostProcess.hlsli\"\n"
        "// CH_PARAM float intensity 1.0 0 2 \"How strong the effect is\"\n"
        "  //   CH_PARAM color tint 1 0.5 0.25\n"
        "// CH_PARAM float2 offset 3 4\n"
        "// CH_PARAM int steps 4\n"
        "// CH_PARAM bool flip 1\n"
        "// CH_STAGE BeforeTonemap\n"
        "float CH_PARAM_like_name;\n"
        "// CH_PARAMETERS is not a declaration\n";
    ShaderParameters::SourceInfo info;
    String error;
    REQUIRE(ShaderParameters::parseSource(source, info, error));
    REQUIRE(info.stage == PostProcessStage::BeforeTonemap);
    REQUIRE(info.parameters.size() == 5);

    const ShaderParameter& intensity = info.parameters[0];
    REQUIRE(intensity.name == "intensity");
    REQUIRE(intensity.type == ShaderParameterType::Float);
    REQUIRE(intensity.defaultValue.x == 1.0f);
    REQUIRE(intensity.minimum == 0.0f);
    REQUIRE(intensity.maximum == 2.0f);
    REQUIRE(intensity.tooltip == "How strong the effect is");

    // A color with three numbers is opaque.
    const ShaderParameter& tint = info.parameters[1];
    REQUIRE(tint.type == ShaderParameterType::Color);
    REQUIRE(tint.defaultValue == Vector4(1.0f, 0.5f, 0.25f, 1.0f));

    REQUIRE(info.parameters[2].defaultValue == Vector4(3.0f, 4.0f, 0.0f, 0.0f));
    REQUIRE(info.parameters[3].type == ShaderParameterType::Int);
    REQUIRE(info.parameters[4].defaultValue.x == 1.0f);

    // One 16-byte slot per parameter.
    for (SIZE_T i = 0; i < info.parameters.size(); ++i) {
      REQUIRE(info.parameters[i].offset == i * ShaderParameters::SLOT_SIZE);
    }
    REQUIRE(info.bufferSize == 5 * ShaderParameters::SLOT_SIZE);

    const String hlsl = ShaderParameters::makeHlslStruct(info.parameters);
    REQUIRE(hlsl.find("struct ShaderParams") != String::npos);
    REQUIRE(hlsl.find("  float intensity;\n  float3 _pad0;\n") != String::npos);
    REQUIRE(hlsl.find("  float4 tint;\n  int steps;") == String::npos);
    REQUIRE(hlsl.find("  float4 tint;\n  float2 offset;\n  float2 _pad1;\n") != String::npos);
    REQUIRE(hlsl.find("  int steps;\n  float3 _pad2;\n") != String::npos);
    REQUIRE(hlsl.find("  uint flip;\n  float3 _pad3;\n") != String::npos);
  }

  SECTION("No declarations give an empty struct and the default stage")
  {
    ShaderParameters::SourceInfo info;
    String error;
    REQUIRE(ShaderParameters::parseSource("float4 PSMain() : SV_Target0 { return 1; }", info,
                                          error));
    REQUIRE(info.parameters.empty());
    REQUIRE(info.stage == PostProcessStage::AfterTonemap);
    REQUIRE(info.bufferSize == 0);
    REQUIRE(ShaderParameters::makeHlslStruct(info.parameters).find("_unused") != String::npos);
  }

  SECTION("Mistakes name their line")
  {
    const StringView broken[] = {
        "// CH_PARAM vec3 position\n",
        "// CH_PARAM float\n",
        "// CH_PARAM float 2fast\n",
        "// CH_PARAM float _hidden\n",
        "// CH_PARAM float a 1 0\n",
        "// CH_PARAM float a 1 2 1\n",
        "// CH_PARAM float2 a 1\n",
        "// CH_PARAM float2 a 1 2 0 1\n",
        "// CH_PARAM int a 1.5\n",
        "// CH_PARAM bool a 2\n",
        "// CH_PARAM float a x\n",
        "// CH_PARAM float a 1 \"open\n",
        "// CH_PARAM float a \"tip\" 1\n",
        "// CH_PARAM float a\n// CH_PARAM float A\n",
        "// CH_STAGE Sideways\n",
        "// CH_STAGE AfterTonemap\n// CH_STAGE BeforeTonemap\n",
    };
    for (const StringView source : broken) {
      ShaderParameters::SourceInfo info;
      String error;
      CAPTURE(source);
      REQUIRE_FALSE(ShaderParameters::parseSource(source, info, error));
      REQUIRE(error.find("Line ") == 0);
    }

    String tooMany;
    for (uint32 i = 0; i <= ShaderParameters::MAX_PARAMETERS; ++i) {
      tooMany += StringUtils::format("// CH_PARAM float p{0}\n", i);
    }
    ShaderParameters::SourceInfo info;
    String error;
    REQUIRE_FALSE(ShaderParameters::parseSource(tooMany, info, error));
    REQUIRE(error.find(StringUtils::format("Line {0}", ShaderParameters::MAX_PARAMETERS + 1)) ==
            0);
  }
#endif // USING(CH_EDITOR)

  SECTION("Values are written where the shader reads them")
  {
    Vector<ShaderParameter> parameters(3);
    parameters[0].type = ShaderParameterType::Float3;
    parameters[0].defaultValue = Vector4(1.0f, 2.0f, 3.0f, 0.0f);
    parameters[1].type = ShaderParameterType::Int;
    parameters[1].defaultValue = Vector4(-7.0f, 0.0f, 0.0f, 0.0f);
    parameters[2].type = ShaderParameterType::Bool;
    parameters[2].defaultValue = Vector4(1.0f, 0.0f, 0.0f, 0.0f);
    const uint32 size = ShaderParameters::computeLayout(parameters);
    REQUIRE(size == 48);

    Vector<uint8> buffer(size, 0xFF);
    ShaderParameters::writeDefaults(parameters, buffer);
    float floats[3];
    std::memcpy(floats, buffer.data(), sizeof(floats));
    REQUIRE(floats[0] == 1.0f);
    REQUIRE(floats[2] == 3.0f);
    int32 number = 0;
    std::memcpy(&number, buffer.data() + 16, sizeof(number));
    REQUIRE(number == -7);
    std::memcpy(&number, buffer.data() + 32, sizeof(number));
    REQUIRE(number == 1);

    // A slot past the end of the buffer is not written.
    Vector<uint8> oneSlot(16, 0);
    ShaderParameters::writeValue(parameters[1], Vector4(5.0f, 0.0f, 0.0f, 0.0f), oneSlot);
    REQUIRE(oneSlot[0] == 0);
  }

  SECTION("Type and stage names")
  {
    ShaderParameterType type;
    REQUIRE(ShaderParameters::parseTypeName("color", type));
    REQUIRE(type == ShaderParameterType::Color);
    REQUIRE(ShaderParameters::getTypeName(ShaderParameterType::Float2) == "float2");
    REQUIRE_FALSE(ShaderParameters::parseTypeName("Float", type));
    PostProcessStage stage;
    REQUIRE(ShaderParameters::parseStageName("beforetonemap", stage));
    REQUIRE(stage == PostProcessStage::BeforeTonemap);
    REQUIRE(ShaderParameters::getStageName(PostProcessStage::AfterTonemap) == "AfterTonemap");
  }
}

TEST_CASE("chCore - Shader asset", "[Graphics]")
{
  const AssetMetadata metadata = makeShaderMetadata();
  TestShaderAsset asset(metadata, makeShaderData());
  REQUIRE(asset.findParameter("tint") == 1);
  REQUIRE(asset.findParameter("missing") == ShaderAsset::NOT_FOUND);
  REQUIRE(asset.findBinary("DXIL"));
  REQUIRE_FALSE(asset.findBinary("METAL"));
  // Without a graphics API no GPU shader is made.
  REQUIRE_FALSE(asset.getPixelShader());

  const SPtr<MemoryDataStream> stream = chMakeShared<MemoryDataStream>();
  REQUIRE(asset.serialize(stream));
  const SIZE_T size = stream->size();

  SECTION("Round trip")
  {
    stream->seek(0);
    TestShaderAsset loaded(metadata);
    REQUIRE(loaded.deserialize(stream));
    REQUIRE(loaded.getRevision() != 0);
    REQUIRE(loaded.getKind() == ShaderAssetKind::PostProcess);
    REQUIRE(loaded.getStage() == PostProcessStage::BeforeTonemap);
    REQUIRE(loaded.getParameterBufferSize() == 32);
    REQUIRE(loaded.isInterfaceCurrent());
    REQUIRE(loaded.getParameters().size() == 2);
    const ShaderParameter& intensity = loaded.getParameters()[0];
    REQUIRE(intensity.name == "intensity");
    REQUIRE(intensity.defaultValue.x == 1.5f);
    REQUIRE(intensity.maximum == 2.0f);
    REQUIRE(intensity.tooltip == "How strong");
    REQUIRE(loaded.getParameters()[1].offset == 16);
    REQUIRE(loaded.getParameters()[1].defaultValue == Vector4(1.0f, 0.5f, 0.25f, 1.0f));

    const ShaderBinary* spirv = loaded.findBinary("SPIRV");
    REQUIRE(spirv);
    REQUIRE(spirv->vertexCode.empty());
    REQUIRE(spirv->pixelCode == Vector<uint8>(64, 3));
    const ShaderBinary* dxil = loaded.findBinary("DXIL");
    REQUIRE(dxil);
    REQUIRE(dxil->vertexCode == Vector<uint8>(16, 5));
    REQUIRE(dxil->pixelCode == Vector<uint8>(80, 9));

    // Saving what was read gives the same bytes.
    const SPtr<MemoryDataStream> again = chMakeShared<MemoryDataStream>();
    REQUIRE(loaded.serialize(again));
    REQUIRE(again->size() == size);
  }

  SECTION("A cut file is refused")
  {
    for (const SIZE_T cut : {SIZE_T(10), size / 2, size - 1}) {
      stream->seek(0);
      Vector<uint8> bytes(cut);
      REQUIRE(stream->read(bytes.data(), bytes.size()) == bytes.size());
      const SPtr<MemoryDataStream> part = chMakeShared<MemoryDataStream>();
      REQUIRE(part->write(bytes.data(), bytes.size()) == bytes.size());
      part->seek(0);
      TestShaderAsset loaded(metadata);
      REQUIRE_FALSE(loaded.deserialize(part));
    }
  }

  SECTION("A newer version is refused")
  {
    stream->seek(0);
    Vector<uint8> bytes(size);
    REQUIRE(stream->read(bytes.data(), bytes.size()) == bytes.size());
    const uint32 newer = ShaderAssetHeader::VERSION + 1;
    std::memcpy(bytes.data(), &newer, sizeof(newer));
    const SPtr<MemoryDataStream> changed = chMakeShared<MemoryDataStream>();
    REQUIRE(changed->write(bytes.data(), bytes.size()) == bytes.size());
    changed->seek(0);
    TestShaderAsset loaded(metadata);
    REQUIRE_FALSE(loaded.deserialize(changed));
  }

  SECTION("Code compiled against older engine headers is not current")
  {
    ShaderAssetData data = makeShaderData();
    data.interfaceVersion = ShaderAsset::INTERFACE_VERSION - 1;
    TestShaderAsset stale(metadata, std::move(data));
    REQUIRE_FALSE(stale.isInterfaceCurrent());
    const SPtr<MemoryDataStream> staleStream = chMakeShared<MemoryDataStream>();
    REQUIRE(stale.serialize(staleStream));
    staleStream->seek(0);
    TestShaderAsset loaded(metadata);
    REQUIRE(loaded.deserialize(staleStream));
    REQUIRE_FALSE(loaded.isInterfaceCurrent());
  }

  SECTION("An asset without code is not saved")
  {
    TestShaderAsset empty(metadata, ShaderAssetData());
    REQUIRE_FALSE(empty.serialize(chMakeShared<MemoryDataStream>()));
  }

#if USING(CH_EDITOR)
  SECTION("A new compile replaces the data and moves the revision")
  {
    const uint32 revision = asset.getRevision();
    asset.setCompileError("Line 3: broken");
    REQUIRE(asset.getCompileError() == "Line 3: broken");
    ShaderAssetData data = makeShaderData();
    data.parameters.pop_back();
    asset.setData(std::move(data));
    REQUIRE(asset.getRevision() == revision + 1);
    REQUIRE(asset.getParameters().size() == 1);
    REQUIRE(asset.getCompileError().empty());
  }
#endif // USING(CH_EDITOR)
}
