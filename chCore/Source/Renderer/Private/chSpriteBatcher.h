/************************************************************************/
/**
 * @file chSpriteBatcher.h
 * @author AccelMR
 * @date 2026/11/12
 * @brief Culls and sorts the sprites of a frame into what one pass draws.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chSpriteItem.h"

namespace chEngineSDK {
class Frustum;

/**
 * What one draw of a SpriteRun reads.
 */
enum class SpriteRunKind : uint8
{
  // SpriteBatcher::getInstances.
  Sprites,
  // SpriteBatcher::getGlyphRuns.
  GlyphRuns,
  // A GlyphRunBatch's own buffer.
  GlyphRunBatch,
  // A CustomSpriteBatch, drawn with its own shaders.
  Custom
};

/**
 * Instances next to each other that draw with the same blend mode, so one draw call.
 */
struct SpriteRun
{
  SpriteBlendMode blendMode = SpriteBlendMode::Alpha;
  uint32 firstInstance = 0;
  uint32 instanceCount = 0;
  SpriteRunKind kind = SpriteRunKind::Sprites;
  // GlyphRunBatch: the bindless index of its buffer. Custom: the batch's index in the list
  // given to build.
  uint32 bufferIndex = 0;
  // The most glyphs a run of the draw has.
  uint32 maxGlyphs = GpuGlyphRun::MAX_GLYPHS;
};

/**
 * Exists to keep the CPU side of drawing sprites apart from the GPU, so it can be tested:
 * it keeps the sprites a frustum sees, sorts them by layer, order and depth, and lays out
 * their instances in that order with the runs that share a blend mode. Textures never
 * split a run, because the shader reads each instance's texture from the bindless heap.
 *
 * Every vector is a member that keeps its memory, so building the same frame again does
 * not allocate.
 */
class CH_CORE_EXPORT SpriteBatcher
{
 public:
  /**
   * Replaces the instances and runs. depthAxis (unit) breaks ties inside an order: sprites
   * farther along it draw first. Sprites that sort equal keep the order they come in, with
   * sprites before glyph runs, glyph runs before glyph run batches and those before custom
   * batches. Batches are not culled and sort at depth 0.
   */
  void
  build(Span<const SpriteItem> sprites,
        const Frustum& frustum,
        const Vector3& depthAxis,
        Span<const GlyphRunItem> glyphRuns = {},
        Span<const GlyphRunBatch> batches = {},
        Span<const CustomSpriteBatch> customBatches = {});

  NODISCARD FORCEINLINE Span<const GpuSpriteInstance>
  getInstances() const noexcept { return m_instances; }

  NODISCARD FORCEINLINE Span<const GpuGlyphRun>
  getGlyphRuns() const noexcept { return m_glyphRuns; }

  NODISCARD FORCEINLINE Span<const SpriteRun>
  getRuns() const noexcept { return m_runs; }

  /**
   * The key build sorts by, lowest first: layer, then order, then depth (the larger
   * depth, the lower the key).
   */
  NODISCARD static uint64
  makeSortKey(int16 sortingLayer, int16 order, float depth) noexcept;

 private:
  struct SortEntry
  {
    uint64 key;
    uint32 sprite;
  };

  Vector<SortEntry> m_entries;
  Vector<SortEntry> m_scratch;
  Vector<GpuSpriteInstance> m_instances;
  Vector<GpuGlyphRun> m_glyphRuns;
  Vector<SpriteRun> m_runs;
};

} // namespace chEngineSDK
