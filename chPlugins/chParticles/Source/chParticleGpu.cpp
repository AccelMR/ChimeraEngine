/************************************************************************/
/**
 * @file chParticleGpu.cpp
 * @author AccelMR
 * @date 2026/11/16
 * @brief
 *  What particles.hlsl reads to move and draw the particles.
 */
/************************************************************************/
#include "chParticleGpu.h"

#include "chEnginePaths.h"
#include "chSpriteShader.h"

namespace chEngineSDK {

/*
 */
SpriteShader&
ParticleGpu::getShader()
{
  static SpriteShader shader(EnginePaths::getEnginePluginShaderDirectory("chParticles"),
                             "particles");
  return shader;
}

} // namespace chEngineSDK
