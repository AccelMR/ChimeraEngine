/************************************************************************/
/**
 * @file chRenderFrame.h
 * @author AccelMR
 * @date 2026/10/20
 * @brief The copy of a scene that the renderer draws one frame from.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chCamera.h"
#include "chDebugDraw.h"
#include "chLightItem.h"
#include "chLinearColor.h"
#include "chRenderItem.h"
#include "chPostProcessItem.h"
#include "chSkyLightItem.h"
#include "chSpriteItem.h"

namespace chEngineSDK {

/**
 * Exists so the renderer never reads a Scene: at the end of the update the camera and the
 * scene's render items, lights, sprites, sky light and debug lines are copied here, and the
 * frame is built only from this copy. The game can then change the scene while the copy is
 * drawn;
 * with a render thread, two copies would let the game fill the next frame while the last
 * one is drawn.
 *
 * Every vector keeps its memory between frames, so extracting the same scene again does not
 * allocate. The copies still point at meshes, materials and textures of assets, which must
 * stay alive until the frame is drawn.
 */
class CH_CORE_EXPORT RenderFrame
{
 public:
  RenderFrame() = default;

  // The list of debug line lists points at the members of this frame.
  RenderFrame(const RenderFrame&) = delete;

  RenderFrame&
  operator=(const RenderFrame&) = delete;

  /**
   * Replaces the contents with the scene as it is now (Scene::updateTransforms must have
   * run) seen by camera; extraLines (may be null) are the application's own lines.
   */
  void
  extract(const Scene& scene,
          const Camera& camera,
          const LinearColor& clearColor,
          const DebugDraw* extraLines = nullptr);

  NODISCARD FORCEINLINE const Camera&
  getCamera() const
  {
    return m_camera;
  }

  NODISCARD FORCEINLINE const LinearColor&
  getClearColor() const
  {
    return m_clearColor;
  }

  NODISCARD FORCEINLINE Span<const RenderItem>
  getItems() const
  {
    return m_items;
  }

  /**
   * The same items, for the renderer to write RenderItem::skinnedVertices before the passes
   * that draw them are built.
   */
  NODISCARD FORCEINLINE Span<RenderItem>
  getWritableItems()
  {
    return m_items;
  }

  NODISCARD FORCEINLINE Span<const LightItem>
  getLights() const
  {
    return m_lights;
  }

  /**
   * The sprites in use, in the order of the scene's slots, then those of the scene's
   * SpriteDraws (update, then fixed step).
   */
  NODISCARD FORCEINLINE Span<const SpriteItem>
  getSprites() const
  {
    return m_sprites;
  }

  /**
   * The glyph runs of the scene's SpriteDraws (update, then fixed step).
   */
  NODISCARD FORCEINLINE Span<const GlyphRunItem>
  getGlyphRuns() const
  {
    return m_glyphRuns;
  }

  NODISCARD FORCEINLINE Span<const GlyphRunBatch>
  getGlyphRunBatches() const
  {
    return m_glyphRunBatches;
  }

  NODISCARD FORCEINLINE Span<const CustomSpriteBatch>
  getCustomBatches() const
  {
    return m_customBatches;
  }

  /**
   * The sky light that lights the scene, null when there is none.
   */
  NODISCARD FORCEINLINE const SkyLightItem*
  getSkyLight() const
  {
    return m_bHasSkyLight ? &m_skyLight : nullptr;
  }

  /**
   * The lines of the scene's update and fixed step and the application's.
   */
  /**
   * Passes of every enabled post-process component, in the order they run.
   */
  NODISCARD FORCEINLINE Span<const PostProcessPassItem>
  getPostProcessPasses() const
  {
    return m_postProcessPasses;
  }

  /**
   * The parameter bytes of every pass, one after the other.
   */
  NODISCARD FORCEINLINE Span<const uint8>
  getPostProcessParameters() const
  {
    return m_postProcessParameters;
  }

  NODISCARD Span<const DebugDraw* const>
  getDebugLines() const
  {
    return m_debugLineLists;
  }

  /**
   * Replaces the contents of outItems with the items whose bounds touch the frustum. The
   * pointers are valid until the next extract.
   */
  void
  gatherItems(const Frustum& frustum, Vector<const RenderItem*>& outItems) const;

  /**
   * Replaces the contents of outLights with every directional light and the point and spot
   * lights whose range touches the frustum. Same lifetime as gatherItems.
   */
  void
  gatherLights(const Frustum& frustum, Vector<const LightItem*>& outLights) const;

 private:
  Camera m_camera;
  LinearColor m_clearColor = LinearColor::BLACK;
  Vector<RenderItem> m_items;
  // The skin matrices of every skinned item one after the other.
  Vector<Matrix4> m_skinMatrices;
  Vector<LightItem> m_lights;
  Vector<SpriteItem> m_sprites;
  Vector<GlyphRunItem> m_glyphRuns;
  Vector<GlyphRunBatch> m_glyphRunBatches;
  Vector<CustomSpriteBatch> m_customBatches;
  SkyLightItem m_skyLight;
  bool m_bHasSkyLight = false;
  Vector<PostProcessPassItem> m_postProcessPasses;
  Vector<uint8> m_postProcessParameters;
  DebugDraw m_sceneLines;
  DebugDraw m_fixedSceneLines;
  DebugDraw m_extraLines;
  Array<const DebugDraw*, 3> m_debugLineLists{&m_sceneLines,
                                               &m_fixedSceneLines,
                                               &m_extraLines};
};

} // namespace chEngineSDK
