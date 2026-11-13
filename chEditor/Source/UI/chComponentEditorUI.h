/************************************************************************/
/**
 * @file chComponentEditorUI.h
 * @author AccelMR
 * @date 2026/11/13
 * @brief Inspector sections of components described for the editor.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

namespace chEngineSDK {
class GameObject;

/**
 * Exists so components of plugins, which cannot draw UI, show in the Object Inspector: each
 * type with a ComponentEditorRegister description gets a section with Enabled, Remove and a
 * row per property or action, and Add Component lists every described type. Every edit is
 * an undo step; actions are not.
 */
class ComponentEditorUI
{
 public:
  static void
  renderComponents(GameObject& gameObject);

  static void
  renderAddComponent(GameObject& gameObject);
};

} // namespace chEngineSDK
