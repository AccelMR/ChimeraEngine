/************************************************************************/
/**
 * @file chPostProcessTestHelpers.h
 * @author AccelMR
 * @date 2026/11/17
 * @brief Shader assets made in memory, for post-process tests without a GPU.
 */
/************************************************************************/
#pragma once

#include "chShaderAsset.h"
#include "chStringUtils.h"
#include "chUUID.h"

namespace chEngineSDK {

/**
 * A post-process shader (or another kind) with a float "intensity" (default 1, 0 to 2), a
 * color "tint" (white), an int "steps" (3) and a bool "flip" (off), and code nobody runs.
 */
inline SPtr<ShaderAsset>
makeTestPostProcessShader(const ANSICHAR* name,
                          PostProcessStage stage = PostProcessStage::AfterTonemap,
                          ShaderAssetKind kind = ShaderAssetKind::PostProcess)
{
  AssetMetadata metadata;
  metadata.uuid = chEngineSDK::UUID::createRandom();
  metadata.assetType = AssetTypeTraits<ShaderAsset>::getTypeId();
  StringUtils::copyToBuffer(metadata.name, name);

  ShaderAssetData data;
  data.kind = kind;
  data.stage = stage;
  data.parameters.push_back({.name = "intensity",
                             .type = ShaderParameterType::Float,
                             .defaultValue = Vector4(1.0f, 0.0f, 0.0f, 0.0f),
                             .minimum = 0.0f,
                             .maximum = 2.0f,
                             .offset = 0,
                             .tooltip = ""});
  data.parameters.push_back({.name = "tint",
                             .type = ShaderParameterType::Color,
                             .defaultValue = Vector4(1.0f, 1.0f, 1.0f, 1.0f),
                             .minimum = 0.0f,
                             .maximum = 0.0f,
                             .offset = 0,
                             .tooltip = ""});
  data.parameters.push_back({.name = "steps",
                             .type = ShaderParameterType::Int,
                             .defaultValue = Vector4(3.0f, 0.0f, 0.0f, 0.0f),
                             .minimum = 0.0f,
                             .maximum = 0.0f,
                             .offset = 0,
                             .tooltip = ""});
  data.parameters.push_back({.name = "flip",
                             .type = ShaderParameterType::Bool,
                             .defaultValue = Vector4::ZERO,
                             .minimum = 0.0f,
                             .maximum = 0.0f,
                             .offset = 0,
                             .tooltip = ""});
  data.parameterBufferSize = ShaderParameters::computeLayout(data.parameters);
  data.interfaceVersion = ShaderAsset::INTERFACE_VERSION;
  data.binaries.push_back(
      {.format = "SPIRV", .vertexCode = {}, .pixelCode = Vector<uint8>(16, 1)});
  return chMakeShared<ShaderAsset>(metadata, std::move(data));
}

} // namespace chEngineSDK
