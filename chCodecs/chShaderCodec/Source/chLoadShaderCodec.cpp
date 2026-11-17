/************************************************************************/
/**
 * @file chLoadShaderCodec.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Entry point of the shader codec plugin.
 */
/************************************************************************/
#include "chAssetCodecManager.h"
#include "chLogger.h"
#include "chPlatformDefines.h"
#include "chShaderCodec.h"

using namespace chEngineSDK;

CH_LOG_DECLARE_STATIC(ShaderCodecPluginLog, All);

CH_EXTERN CH_PLUGIN_EXPORT void
loadPlugin()
{
  if (!AssetCodecManager::isStarted()) {
    CH_LOG_ERROR(ShaderCodecPluginLog, "AssetCodecManager is not started. Cannot load codecs.");
    return;
  }
  AssetCodecManager::instance().registerCodec<ShaderCodec>();
}
