/************************************************************************/
/**
 * @file chScriptTestHelpers.h
 * @author AccelMR
 * @date 2026/11/17
 * @brief Helpers that the script tests of several topics share.
 */
/************************************************************************/
#pragma once

#include "chLogger.h"
#include "chScriptAsset.h"
#include "chScriptManager.h"
#include "chStringUtils.h"
#include "chUUID.h"

#include "catch.hpp"

namespace chEngineSDK {

/*
 * The component tests need the module; it cannot start twice, so it starts once and stays.
 */
inline ScriptManager&
getTestScriptManager()
{
  if (!ScriptManager::isStarted()) {
    ScriptManager::startUp();
  }
  return ScriptManager::instance();
}

inline SPtr<ScriptAsset>
createTestScriptAsset(const ANSICHAR* name, const ANSICHAR* source)
{
  AssetMetadata metadata;
  metadata.uuid = chEngineSDK::UUID::createRandom();
  metadata.assetType = AssetTypeTraits<ScriptAsset>::getTypeId();
  StringUtils::copyToBuffer(metadata.name, name);
  StringUtils::copyToBuffer(metadata.assetPath, "/Game/Scripts");
  return chMakeShared<ScriptAsset>(metadata, String(source));
}

} // namespace chEngineSDK
