/************************************************************************/
/**
 * @file chLoadTextureCompressor.cpp
 * @author AccelMR
 * @date 2026/11/20
 * @brief
 *  Entry point of the texture compressor plugin.
 */
/************************************************************************/
#include "chAssetCodecManager.h"
#include "chBcTextureCompressor.h"
#include "chLogger.h"
#include "chPlatformDefines.h"

using namespace chEngineSDK;

CH_LOG_DECLARE_STATIC(TextureCompressorPluginLog, All);

CH_EXTERN CH_PLUGIN_EXPORT void
loadPlugin()
{
  if (!AssetCodecManager::isStarted()) {
    CH_LOG_ERROR(TextureCompressorPluginLog,
                 "AssetCodecManager is not started. Cannot give the texture compressor.");
    return;
  }
  AssetCodecManager::instance().setTextureCompressor(chMakeUnique<BcTextureCompressor>());
}
