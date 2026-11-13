/************************************************************************/
/**
 * @file chLoadFontCodec.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Entry point of the font codec plugin.
 */
/************************************************************************/
#include "chAssetCodecManager.h"
#include "chFontCodec.h"
#include "chLogger.h"
#include "chPlatformDefines.h"

using namespace chEngineSDK;

CH_LOG_DECLARE_STATIC(FontCodecPluginLog, All);

CH_EXTERN CH_PLUGIN_EXPORT void
loadPlugin()
{
  if (!AssetCodecManager::isStarted()) {
    CH_LOG_ERROR(FontCodecPluginLog, "AssetCodecManager is not started. Cannot load codecs.");
    return;
  }
  AssetCodecManager::instance().registerCodec<FontCodec>();
}
