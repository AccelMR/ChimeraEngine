/************************************************************************/
/**
 * @file chSpriteAtlasEditorUI.h
 * @author AccelMR
 * @date 2026/11/13
 * @brief Edits the frames and animations of a sprite atlas.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

namespace chEngineSDK {
class IAsset;
class SpriteAtlasAsset;

/**
 * Exists so a sprite sheet is cut and animated in the editor: the Asset Inspector shows the
 * atlas' image with its frames drawn over it, makes frames from a grid, a cell size or the
 * opaque groups of pixels, edits each frame and the animations, and plays the selected
 * animation. Edits work on a copy that reaches the asset (and every sprite showing it) once
 * it is valid, and the asset is saved once no control is being edited. Frames drag onto
 * sprites. The Object Inspector reuses the frame and animation pickers.
 */
class SpriteAtlasEditorUI
{
 public:
  static void
  render(const SPtr<SpriteAtlasAsset>& atlas);

  /**
   * One frame of the atlas as an image of at most maxWidth by maxHeight pixels, keeping its
   * proportions. Draws nothing while the texture is not on the GPU.
   */
  static void
  drawFrame(const SpriteAtlasAsset& atlas, uint32 frame, float maxWidth, float maxHeight);

  /**
   * A combo of the atlas' frames; true when another was picked.
   */
  static bool
  pickFrame(const ANSICHAR* label, const SpriteAtlasAsset& atlas, uint32& frame);

  /**
   * A combo of the atlas' animations by name; true when another was picked.
   */
  static bool
  pickAnimation(const ANSICHAR* label, const SpriteAtlasAsset& atlas, String& animation);

  /**
   * Forgets the atlas being edited if it is asset, before it is deleted.
   */
  static void
  close(const IAsset& asset);

  /**
   * Drops the atlas being edited, so nothing of it outlives the asset manager.
   */
  static void
  shutDown();
};

} // namespace chEngineSDK
