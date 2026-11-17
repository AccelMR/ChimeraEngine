/************************************************************************/
/**
 * @file chShaderCodec.h
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Codec that compiles a project's HLSL files into shader assets.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chAssetCodec.h"
#include "chShaderAsset.h"
#include "chShaderCompiler.h"
#include "chTypeTraits.h"

namespace chEngineSDK {

/**
 * Exists so a project brings its own shaders without a C++ build: the HLSL file is compiled
 * when it is imported, for every graphics API the engine has, and the asset keeps only the
 * code. Importing a file that was imported before compiles it into the same asset, so the
 * editor can call it again whenever the file changes; when that compile fails, the asset
 * keeps its code and holds the error.
 */
class ShaderCodec : public IAssetCodec
{
 public:
  NODISCARD UUID
  getCodecType() const override
  {
    static const UUID codecType = UUID::createFromName("ShaderCodec");
    return codecType;
  }

  NODISCARD Vector<UUID>
  getSupportedAssetTypes() const override
  {
    return {AssetTypeTraits<ShaderAsset>::getTypeId()};
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
  const Vector<String> m_extensions = {"hlsl"};
  ShaderCompiler m_compiler;
};
DECLARE_ASSET_TYPE(ShaderCodec);

} // namespace chEngineSDK
