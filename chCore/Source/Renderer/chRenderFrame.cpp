/************************************************************************/
/**
 * @file chRenderFrame.cpp
 * @author AccelMR
 * @date 2026/10/20
 * @brief The copy of a scene that the renderer draws one frame from.
 */
/************************************************************************/
#include "chRenderFrame.h"

#include "chMesh.h"
#include "chRenderCulling.h"
#include "chScene.h"

namespace chEngineSDK {

/*
 */
void
RenderFrame::extract(const Scene& scene,
                     const Camera& camera,
                     const LinearColor& clearColor,
                     const DebugDraw* extraLines)
{
  m_camera = camera;
  m_clearColor = clearColor;

  // Free slots are left out, so the renderer walks only what draws. The skin matrices are
  // copied too, because the component may change them while the frame is drawn.
  m_items.clear();
  m_skinMatrices.clear();
  const Span<const RenderItem> slots = scene.getRenderItemSlots();
  for (SIZE_T slot = 0; slot < slots.size(); ++slot) {
    const RenderItem& item = slots[slot];
    if (!item.mesh) {
      continue;
    }
    RenderItem& copy = m_items.emplace_back(item);
    copy.sceneSlot = static_cast<uint32>(slot);
    copy.skinnedVertices = nullptr;
    if (item.skinMatrices) {
      m_skinMatrices.insert(m_skinMatrices.end(), item.skinMatrices,
                            item.skinMatrices + item.mesh->getSkin().boneNodes.size());
    }
  }
  // Pointed at only now, since the copies above may move the matrices.
  SIZE_T firstSkinMatrix = 0;
  for (RenderItem& item : m_items) {
    if (item.skinMatrices) {
      item.skinMatrices = m_skinMatrices.data() + firstSkinMatrix;
      firstSkinMatrix += item.mesh->getSkin().boneNodes.size();
    }
  }
  m_lights.clear();
  for (const LightItem& light : scene.getLightSlots()) {
    if (light.bInUse) {
      m_lights.push_back(light);
    }
  }

  // One pass of 80-byte copies; the instance inside each is already what the GPU reads.
  const Span<const SpriteItem> frameSprites = scene.getSpriteDraw().getSprites();
  const Span<const SpriteItem> fixedSprites = scene.getFixedSpriteDraw().getSprites();
  m_sprites.clear();
  m_sprites.reserve(scene.getSpriteCount() + frameSprites.size() + fixedSprites.size());
  for (const SpriteItem& sprite : scene.getSpriteSlots()) {
    if (sprite.bInUse) {
      m_sprites.push_back(sprite);
    }
  }
  m_sprites.insert(m_sprites.end(), frameSprites.begin(), frameSprites.end());
  m_sprites.insert(m_sprites.end(), fixedSprites.begin(), fixedSprites.end());

  const SpriteDraw& frameDraw = scene.getSpriteDraw();
  const SpriteDraw& fixedDraw = scene.getFixedSpriteDraw();
  m_glyphRuns.assign(frameDraw.getGlyphRuns().begin(), frameDraw.getGlyphRuns().end());
  m_glyphRuns.insert(
      m_glyphRuns.end(), fixedDraw.getGlyphRuns().begin(), fixedDraw.getGlyphRuns().end());
  for (const TextItem& text : scene.getTextSlots()) {
    if (!text.bInUse) {
      continue;
    }
    m_sprites.insert(m_sprites.end(), text.sprites.begin(), text.sprites.end());
    m_glyphRuns.insert(m_glyphRuns.end(), text.runs, text.runs + text.runCount);
  }
  m_glyphRunBatches.assign(frameDraw.getGlyphRunBatches().begin(),
                           frameDraw.getGlyphRunBatches().end());
  m_glyphRunBatches.insert(m_glyphRunBatches.end(),
                           fixedDraw.getGlyphRunBatches().begin(),
                           fixedDraw.getGlyphRunBatches().end());

  const SkyLightItem* skyLight = scene.getActiveSkyLight();
  m_bHasSkyLight = skyLight != nullptr;
  if (skyLight) {
    m_skyLight = *skyLight;
  }

  m_sceneLines = scene.getDebugDraw();
  m_fixedSceneLines = scene.getFixedDebugDraw();
  if (extraLines) {
    m_extraLines = *extraLines;
  }
  else {
    m_extraLines.clear();
  }
}

/*
 */
void
RenderFrame::gatherItems(const Frustum& frustum, Vector<const RenderItem*>& outItems) const
{
  RenderCulling::gatherItems(m_items, frustum, outItems);
}

/*
 */
void
RenderFrame::gatherLights(const Frustum& frustum, Vector<const LightItem*>& outLights) const
{
  RenderCulling::gatherLights(m_lights, frustum, outLights);
}

} // namespace chEngineSDK
