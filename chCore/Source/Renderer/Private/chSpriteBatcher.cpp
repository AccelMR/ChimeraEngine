/************************************************************************/
/**
 * @file chSpriteBatcher.cpp
 * @author AccelMR
 * @date 2026/11/12
 * @brief Culls and sorts the sprites of a frame into what one pass draws.
 */
/************************************************************************/

#include "chSpriteBatcher.h"

#include <bit>

#include "chFrustum.h"
#include "chMath.h"
#include "chProfiler.h"
#include "chShapeOverlap.h"
#include "chSphere.h"

namespace chEngineSDK {
namespace {
constexpr uint32 kRadixBits = 8;
constexpr uint32 kRadixBuckets = 1u << kRadixBits;
constexpr uint32 kKeyBytes = sizeof(uint64);
// What an entry points at is kept in the top bits of its index.
constexpr uint32 kKindShift = 30;
constexpr uint32 kIndexMask = (1u << kKindShift) - 1;
constexpr uint32 kSpriteKind = 0;
constexpr uint32 kGlyphRunKind = 1;
constexpr uint32 kBatchKind = 2;
constexpr uint32 kCustomKind = 3;

/*
 * Unsigned numbers in the same order as the floats, negative ones included.
 */
NODISCARD uint32
toSortableBits(float value) noexcept
{
  const uint32 bits = std::bit_cast<uint32>(value);
  return (bits & 0x80000000u) != 0 ? ~bits : (bits | 0x80000000u);
}

NODISCARD uint64
toSortableBits(int16 value) noexcept
{
  return static_cast<uint64>(static_cast<uint16>(static_cast<int32>(value) + 32768));
}
} // namespace

/*
 */
uint64
SpriteBatcher::makeSortKey(int16 sortingLayer, int16 order, float depth) noexcept
{
  return (toSortableBits(sortingLayer) << 48) | (toSortableBits(order) << 32) |
         static_cast<uint64>(toSortableBits(-depth));
}

/*
 */
void
SpriteBatcher::build(Span<const SpriteItem> sprites,
                     const Frustum& frustum,
                     const Vector3& depthAxis,
                     Span<const GlyphRunItem> glyphRuns,
                     Span<const GlyphRunBatch> batches,
                     Span<const CustomSpriteBatch> customBatches)
{
  CH_PROFILE_SCOPE("BatchSprites");
  m_entries.clear();
  m_instances.clear();
  m_glyphRuns.clear();
  m_runs.clear();

  for (SIZE_T i = 0; i < sprites.size(); ++i) {
    const SpriteItem& sprite = sprites[i];
    if (!ShapeOverlap::frustumSphere(frustum, Sphere(sprite.instance.center, sprite.radius))) {
      continue;
    }
    m_entries.push_back(
        {.key = makeSortKey(sprite.sortingLayer, sprite.order,
                            sprite.instance.center.dot(depthAxis)),
         .sprite = static_cast<uint32>(i)});
  }
  for (SIZE_T i = 0; i < glyphRuns.size(); ++i) {
    const GlyphRunItem& item = glyphRuns[i];
    if (!ShapeOverlap::frustumSphere(frustum, Sphere(item.run.position, item.radius))) {
      continue;
    }
    m_entries.push_back(
        {.key = makeSortKey(item.sortingLayer, item.order, item.run.position.dot(depthAxis)),
         .sprite = static_cast<uint32>(i) | (kGlyphRunKind << kKindShift)});
  }
  for (SIZE_T i = 0; i < batches.size(); ++i) {
    if (batches[i].runCount == 0) {
      continue;
    }
    m_entries.push_back({.key = makeSortKey(batches[i].sortingLayer, batches[i].order, 0.0f),
                         .sprite = static_cast<uint32>(i) | (kBatchKind << kKindShift)});
  }
  for (SIZE_T i = 0; i < customBatches.size(); ++i) {
    const CustomSpriteBatch& batch = customBatches[i];
    m_entries.push_back({.key = makeSortKey(batch.sortingLayer, batch.order, 0.0f),
                         .sprite = static_cast<uint32>(i) | (kCustomKind << kKindShift)});
  }
  const SIZE_T count = m_entries.size();
  if (count == 0) {
    return;
  }

  // Least significant byte first, each pass stable, so equal keys keep the order they came
  // in. Most sprites share a layer and an order, so every byte whose value is the same for
  // all of them is skipped: usually only the four depth bytes are sorted.
  uint32 histograms[kKeyBytes][kRadixBuckets] = {};
  for (const SortEntry& entry : m_entries) {
    for (uint32 byte = 0; byte < kKeyBytes; ++byte) {
      ++histograms[byte][(entry.key >> (byte * kRadixBits)) & (kRadixBuckets - 1)];
    }
  }
  m_scratch.resize(count);
  SortEntry* source = m_entries.data();
  SortEntry* target = m_scratch.data();
  for (uint32 byte = 0; byte < kKeyBytes; ++byte) {
    uint32* histogram = histograms[byte];
    const uint32 firstBucket = (source[0].key >> (byte * kRadixBits)) & (kRadixBuckets - 1);
    if (histogram[firstBucket] == count) {
      continue;
    }
    uint32 offset = 0;
    for (uint32 bucket = 0; bucket < kRadixBuckets; ++bucket) {
      const uint32 bucketCount = histogram[bucket];
      histogram[bucket] = offset;
      offset += bucketCount;
    }
    for (SIZE_T i = 0; i < count; ++i) {
      const uint32 bucket = (source[i].key >> (byte * kRadixBits)) & (kRadixBuckets - 1);
      target[histogram[bucket]++] = source[i];
    }
    std::swap(source, target);
  }

  for (SIZE_T i = 0; i < count; ++i) {
    const uint32 kind = source[i].sprite >> kKindShift;
    const uint32 index = source[i].sprite & kIndexMask;
    if (kind == kBatchKind) {
      const GlyphRunBatch& batch = batches[index];
      m_runs.push_back({.blendMode = batch.blendMode,
                        .firstInstance = batch.firstRun,
                        .instanceCount = batch.runCount,
                        .kind = SpriteRunKind::GlyphRunBatch,
                        .bufferIndex = batch.bufferIndex,
                        .maxGlyphs = Math::clamp(batch.maxGlyphs, 1u, GpuGlyphRun::MAX_GLYPHS)});
      continue;
    }
    if (kind == kCustomKind) {
      const CustomSpriteBatch& batch = customBatches[index];
      m_runs.push_back({.blendMode = batch.blendMode,
                        .firstInstance = 0,
                        .instanceCount = batch.instanceCount,
                        .kind = SpriteRunKind::Custom,
                        .bufferIndex = index});
      continue;
    }

    SpriteBlendMode blendMode;
    SpriteRunKind runKind;
    uint32 instance;
    if (kind == kSpriteKind) {
      const SpriteItem& sprite = sprites[index];
      blendMode = sprite.blendMode;
      runKind = SpriteRunKind::Sprites;
      instance = static_cast<uint32>(m_instances.size());
      m_instances.push_back(sprite.instance);
    }
    else {
      const GlyphRunItem& item = glyphRuns[index];
      blendMode = item.blendMode;
      runKind = SpriteRunKind::GlyphRuns;
      instance = static_cast<uint32>(m_glyphRuns.size());
      m_glyphRuns.push_back(item.run);
    }
    if (m_runs.empty() || m_runs.back().blendMode != blendMode || m_runs.back().kind != runKind) {
      m_runs.push_back({.blendMode = blendMode,
                        .firstInstance = instance,
                        .instanceCount = 0,
                        .kind = runKind});
    }
    ++m_runs.back().instanceCount;
  }
}

} // namespace chEngineSDK
