/************************************************************************/
/**
 * @file chSpriteShader.cpp
 * @author AccelMR
 * @date 2026/11/16
 * @brief
 *  Shaders of a plugin that draw sprites sorted with the engine's.
 */
/************************************************************************/
#include "chSpriteShader.h"

#include "chAlgorithm.h"
#include "chIGraphicsAPI.h"
#include "chIPipeline.h"
#include "chIShader.h"

namespace chEngineSDK {
namespace {
/*
 * Never deleted: a plugin's SpriteShader is destroyed when the plugin unloads, which can be
 * after the statics of this file.
 */
NODISCARD Vector<SpriteShader*>&
getShaders()
{
  static Vector<SpriteShader*>* shaders = new Vector<SpriteShader*>();
  return *shaders;
}
} // namespace

/*
 */
SpriteShader::SpriteShader(Path directory, String name)
  : m_directory(std::move(directory)),
    m_name(std::move(name))
{
  getShaders().push_back(this);
}

/*
 */
SpriteShader::~SpriteShader()
{
  Algorithm::removeFirst(getShaders(), this);
}

/*
 */
bool
SpriteShader::loadShaders()
{
  if (m_bLoadFailed) {
    return false;
  }
  if (!m_vertexShader) {
    IGraphicsAPI& graphicsAPI = IGraphicsAPI::instance();
    m_vertexShader = graphicsAPI.loadShader(ShaderStage::Vertex, m_name, m_directory);
    m_fragmentShader = graphicsAPI.loadShader(ShaderStage::Fragment, m_name, m_directory);
    if (!m_vertexShader || !m_fragmentShader) {
      m_vertexShader.reset();
      m_fragmentShader.reset();
      m_bLoadFailed = true;
      return false;
    }
  }
  return true;
}

/*
 */
const SPtr<IShader>&
SpriteShader::getVertexShader()
{
  static_cast<void>(loadShaders());
  return m_vertexShader;
}

/*
 */
const IPipeline*
SpriteShader::getPipeline(SpriteBlendMode blendMode, Format colorFormat)
{
  if (!loadShaders()) {
    return nullptr;
  }
  if (colorFormat != m_pipelineFormat) {
    m_pipelines.fill(nullptr);
    m_pipelineFormat = colorFormat;
  }
  const IPipeline*& pipeline = m_pipelines[static_cast<SIZE_T>(blendMode)];
  if (pipeline == nullptr) {
    pipeline = m_pipelineCache
                   .getOrCreate(makePipelineDesc(
                       m_vertexShader, m_fragmentShader, blendMode, colorFormat))
                   .get();
  }
  return pipeline;
}

/*
 */
void
SpriteShader::releaseGpuObjects()
{
  m_pipelines.fill(nullptr);
  m_pipelineFormat = Format::Unknown;
  m_pipelineCache.clear();
  m_vertexShader.reset();
  m_fragmentShader.reset();
  m_bLoadFailed = false;
}

/*
 */
void
SpriteShader::releaseAll()
{
  for (SpriteShader* shader : getShaders()) {
    shader->releaseGpuObjects();
  }
}

/*
 */
GraphicsPipelineDesc
SpriteShader::makePipelineDesc(const SPtr<IShader>& vertexShader,
                               const SPtr<IShader>& fragmentShader,
                               SpriteBlendMode blendMode,
                               Format colorFormat)
{
  GraphicsPipelineDesc desc{.vertexShader = vertexShader,
                            .fragmentShader = fragmentShader,
                            .raster = {.cullMode = CullMode::None},
                            .depth = {.testEnable = false, .writeEnable = false},
                            .colorAttachmentCount = 1};
  desc.colorFormats[0] = colorFormat;
  BlendAttachmentState& blend = desc.blendStates[0];
  blend.enable = true;
  blend.srcColorFactor = BlendFactor::SrcAlpha;
  const bool bAdditive = blendMode == SpriteBlendMode::Additive;
  blend.dstColorFactor = bAdditive ? BlendFactor::One : BlendFactor::OneMinusSrcAlpha;
  blend.srcAlphaFactor = BlendFactor::One;
  blend.dstAlphaFactor = BlendFactor::OneMinusSrcAlpha;
  return desc;
}

} // namespace chEngineSDK
