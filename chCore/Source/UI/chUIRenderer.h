/************************************************************************/
/**
 * @file chUIRenderer.h
 * @author Swampertor
 * @date 2026/11/19
 * @brief Draws the UI of a frame over the scene.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chCamera.h"
#include "chGraphicsTypes.h"
#include "chRenderGraph.h"
#include "chUIDrawList.h"
#include "chVector2.h"

namespace chEngineSDK {
class SpritePass;

/**
 * Exists so the UI does not need to know about sprites, cameras or render passes: it
 * paints into a UIDrawList, and this turns that list into one overlay pass that loads the
 * back buffer and draws the UI on top with encoded colors.
 *
 * The camera is orthographic and looks along +X, so UI space (top-left, +Y down) maps to
 * the sprite plane (right +Y, up +Z) with one subtraction. It is placed ten units in front
 * of the UI and covers exactly the viewport size.
 *
 * Needs a started IGraphicsAPI. Main thread only.
 */
class CH_CORE_EXPORT UIRenderer
{
 public:
  UIRenderer();

  ~UIRenderer();

  UIRenderer(const UIRenderer&) = delete;
  UIRenderer&
  operator=(const UIRenderer&) = delete;
  UIRenderer(UIRenderer&&) = delete;
  UIRenderer&
  operator=(UIRenderer&&) = delete;

  /**
   * Size in pixels of the surface the UI covers; repositions the camera and the draw list.
   */
  void
  setViewportSize(const Vector2& size);

  NODISCARD FORCEINLINE const Vector2&
  getViewportSize() const noexcept
  {
    return m_drawList.getViewportSize();
  }

  /**
   * Clears the draw list for a new frame and gives it to paint into.
   */
  UIDrawList&
  beginFrame();

  NODISCARD FORCEINLINE UIDrawList&
  getDrawList() noexcept
  {
    return m_drawList;
  }

  NODISCARD FORCEINLINE const UIDrawList&
  getDrawList() const noexcept
  {
    return m_drawList;
  }

  /**
   * Adds the overlay pass. loadOp Load keeps the scene in output; Clear clears it to
   * transparent first. output is the back buffer (a UNORM target), so the pass encodes the
   * UI colors itself. Does nothing when the draw list is empty.
   */
  void
  addPass(RenderGraph& graph, RGTextureHandle output, LoadOp loadOp = LoadOp::Load);

  NODISCARD FORCEINLINE const Camera&
  getCamera() const noexcept
  {
    return m_camera;
  }

  /**
   * Instances and glyph runs drawn by the last addPass, for the profiler.
   */
  NODISCARD uint32
  getLastInstanceCount() const noexcept;

  NODISCARD uint32
  getLastGlyphRunCount() const noexcept;

 private:
  UniquePtr<SpritePass> m_spritePass;
  Camera m_camera;
  UIDrawList m_drawList;
};

} // namespace chEngineSDK
