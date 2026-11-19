/************************************************************************/
/**
 * @file chPostProcessEditorUI.h
 * @author AccelMR
 * @date 2026/11/17
 * @brief The pass list of a post-process component in the Object Inspector.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

namespace chEngineSDK {
class Component;
class ShaderAsset;

/**
 * Exists because a pass's rows come from its shader's parameters, which no fixed description
 * of the component can hold: each pass is a row with its enabled box, shader, weight and
 * arrows to reorder, opening to a widget per parameter with a reset button on the values it
 * changed. Add Pass lists the project's shaders and the engine's effects, which are imported
 * into /Game/EngineEffects the first time they are picked. Every edit is an undo step.
 */
class PostProcessEditorUI
{
 public:
  static void
  render(Component& component);

  /**
   * The shader asset of an engine effect (a file of /Engine/PostProcess without its
   * extension), imported into the project when it has none yet. Null when it cannot be.
   */
  NODISCARD static SPtr<ShaderAsset>
  findOrImportEngineEffect(StringView effect);

  static constexpr const ANSICHAR* ENGINE_EFFECT_FOLDER = "/Game/EngineEffects";
};

} // namespace chEngineSDK
