/************************************************************************/
/**
 * @file chMaterialEditorUI.h
 * @author AccelMR
 * @date 2026/10/20
 * @brief Edits the values and textures of a material or material instance asset.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

namespace chEngineSDK {
class MaterialInterfaceAsset;

/**
 * Exists so the Object Inspector (a model's material slot) and the Asset Inspector edit
 * materials and their instances with the same controls. Edits change the shared asset, so
 * every object and instance that uses it follows, and the asset is saved once each edit
 * ends. An instance shows its parent and a checkbox per value: values it does not set show
 * the parent's in gray.
 */
class MaterialEditorUI
{
 public:
  static void
  render(const SPtr<MaterialInterfaceAsset>& materialAsset);
};

} // namespace chEngineSDK
