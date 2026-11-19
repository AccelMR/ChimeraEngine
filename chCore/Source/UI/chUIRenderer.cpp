/************************************************************************/
/**
 * @file chUIRenderer.cpp
 * @author Swampertor
 * @date 2026/11/19
 * @brief Draws the UI of a frame over the scene.
 */
/************************************************************************/
#include "chUIRenderer.h"

#include "chMath.h"
#include "chSpritePass.h"

namespace chEngineSDK {

/*
 */
UIRenderer::UIRenderer() : m_spritePass(chMakeUnique<SpritePass>())
{
}

/*
 */
UIRenderer::~UIRenderer() = default;

/*
 */
void
UIRenderer::setViewportSize(const Vector2& size)
{
  if (m_drawList.getViewportSize() == size) {
    return;
  }
  m_drawList.setViewportSize(size);

  // A zero-size viewport would make the aspect ratio and the projection invalid; the UI is
  // not drawn then anyway, so a 1x1 camera is enough.
  const float width = Math::max(size.x, 1.0f);
  const float height = Math::max(size.y, 1.0f);
  const float halfHeight = height * 0.5f;

  m_camera.setViewportSize(width, height);
  m_camera.setProjectionType(CameraProjectionType::Orthographic);
  // Looking along +X: world Y is screen right and world Z screen up, so UI (x, y) becomes
  // (x, height - y). Ten units away leaves room for the near plane.
  m_camera.setPosition({-10.0f, 0.0f, halfHeight});
  m_camera.lookAt({0.0f, 0.0f, halfHeight});
  m_camera.setOrthographicSize(halfHeight);
  m_camera.setClipPlanes(0.1f, 100.0f);
}

/*
 */
UIDrawList&
UIRenderer::beginFrame()
{
  m_drawList.clear();
  return m_drawList;
}

/*
 */
void
UIRenderer::addPass(RenderGraph& graph, RGTextureHandle output, LoadOp loadOp)
{
  if (m_drawList.isEmpty()) {
    return;
  }

  m_spritePass->addPass(graph,
                        m_camera,
                        m_drawList.getSprites(),
                        m_drawList.getGlyphRuns(),
                        m_drawList.getGlyphRunBatches(),
                        m_drawList.getCustomBatches(),
                        LinearColor::TRANSPARENT_BLACK,
                        output,
                        loadOp,
                        true,
                        "UISprites");
}

/*
 */
uint32
UIRenderer::getLastInstanceCount() const noexcept
{
  return m_spritePass->getLastInstanceCount();
}

/*
 */
uint32
UIRenderer::getLastGlyphRunCount() const noexcept
{
  return m_spritePass->getLastGlyphRunCount();
}

} // namespace chEngineSDK
