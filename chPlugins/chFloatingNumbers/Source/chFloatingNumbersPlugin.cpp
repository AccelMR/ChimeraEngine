/************************************************************************/
/**
 * @file chFloatingNumbersPlugin.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Entry point of the floating numbers plugin.
 */
/************************************************************************/
#include "chComponentRegister.h"
#include "chFloatingNumberBindings.h"
#include "chFloatingNumberSystemComponent.h"
#include "chFloatingNumbersEditor.h"
#include "chScriptManager.h"

using namespace chEngineSDK;

/*
 * Called once by EnginePlugins, before the asset, script and scene managers start.
 */
CH_EXTERN CH_PLUGIN_EXPORT void
loadPlugin()
{
  ComponentRegister::registerType<FloatingNumberSystemComponent>();
  ScriptManager::addBindings(&FloatingNumberBindings::registerBindings);
#if USING(CH_EDITOR)
  FloatingNumbersEditor::registerDescriptions();
#endif
}
