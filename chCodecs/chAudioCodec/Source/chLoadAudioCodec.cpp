/************************************************************************/
/**
 * @file chLoadAudioCodec.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Entry point of the audio codec plugin.
 */
/************************************************************************/
#include "chAssetCodecManager.h"
#include "chAudioCodec.h"
#include "chLogger.h"
#include "chPlatformDefines.h"

using namespace chEngineSDK;

CH_LOG_DECLARE_STATIC(AudioCodecPluginLog, All);

CH_EXTERN CH_PLUGIN_EXPORT void
loadPlugin()
{
  if (!AssetCodecManager::isStarted()) {
    CH_LOG_ERROR(AudioCodecPluginLog, "AssetCodecManager is not started. Cannot load codecs.");
    return;
  }
  AssetCodecManager::instance().registerCodec<AudioCodec>();
}
