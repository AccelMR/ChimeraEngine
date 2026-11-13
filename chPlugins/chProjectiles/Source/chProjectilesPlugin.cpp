/************************************************************************/
/**
 * @file chProjectilesPlugin.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Entry point of the projectiles plugin.
 */
/************************************************************************/
#include "chComponentRegister.h"
#include "chProjectileBindings.h"
#include "chProjectileSystemComponent.h"
#include "chProjectileTargetComponent.h"
#include "chProjectilesEditor.h"
#include "chScriptManager.h"

using namespace chEngineSDK;

/*
 * Called once by EnginePlugins, before the asset, script and scene managers start.
 */
CH_EXTERN CH_PLUGIN_EXPORT void
loadPlugin()
{
  ComponentRegister::registerType<ProjectileSystemComponent>();
  ComponentRegister::registerType<ProjectileTargetComponent>();
  ScriptManager::addBindings(&ProjectileBindings::registerBindings);
#if USING(CH_EDITOR)
  ProjectilesEditor::registerDescriptions();
#endif
}
