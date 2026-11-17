/************************************************************************/
/**
 * @file chPostProcessItem.h
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  One post-process pass as the renderer reads it.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chShaderParameters.h"

namespace chEngineSDK {

class ShaderAsset;

/**
 * A pass of a PostProcessComponent copied into the render frame, so the renderer never reads
 * the component. Its parameter bytes live in the frame, at parameterOffset.
 */
struct PostProcessPassItem
{
  ShaderAsset* shader = nullptr;
  PostProcessStage stage = PostProcessStage::AfterTonemap;
  // 0 to 1; below 1 the result is blended with the image the pass read.
  float weight = 1.0f;
  uint32 parameterOffset = 0;
  uint32 parameterSize = 0;
};

} // namespace chEngineSDK
