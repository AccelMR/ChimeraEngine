/************************************************************************/
/**
 * @file chFloatingNumberGpu.cpp
 * @author AccelMR
 * @date 2026/11/16
 * @brief
 *  What floatingnumbers.hlsl reads to move the numbers.
 */
/************************************************************************/
#include "chFloatingNumberGpu.h"

#include "chEnginePaths.h"
#include "chSpriteShader.h"

namespace chEngineSDK {

/*
 */
SpriteShader&
FloatingNumberGpu::getShader()
{
  static SpriteShader shader(EnginePaths::getEnginePluginShaderDirectory("chParticles"),
                             "floatingnumbers");
  return shader;
}

} // namespace chEngineSDK
