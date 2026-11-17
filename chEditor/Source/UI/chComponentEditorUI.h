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

#include "chUUID.h"

namespace chEngineSDK {
class Component;
class GameObject;

/**
 * Draws the part of a component's section that a description cannot hold, such as rows
 * that depend on an asset the component uses. Called after the described entries.
 */
using ComponentSectionFunction = void (*)(Component& component);

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

  /**
   * Gives a described type more rows, drawn by the editor. Replaces an earlier one.
   */
  static void
  setCustomSection(const UUID& typeId, ComponentSectionFunction function);
};

} // namespace chEngineSDK
