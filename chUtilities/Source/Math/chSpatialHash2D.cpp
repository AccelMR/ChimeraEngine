/************************************************************************/
/**
 * @file chSpatialHash2D.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Finds the circles and boxes of a 2D world near a point, circle or box.
 */
/************************************************************************/
#include "chSpatialHash2D.h"

#include "chMath.h"

namespace chEngineSDK {
namespace {
// Coordinates are clamped so cell numbers fit an int32 whatever the cell size.
constexpr float kMaxCoordinate = 1e9f;

NODISCARD FORCEINLINE int32
toCell(float value, float inverseCellSize) noexcept
{
  const float scaled =
      Math::max(-kMaxCoordinate, Math::min(value * inverseCellSize, kMaxCoordinate));
  const int32 truncated = static_cast<int32>(scaled);
  return static_cast<float>(truncated) > scaled ? truncated - 1 : truncated;
}

NODISCARD FORCEINLINE uint64
makeCellKey(int32 x, int32 y) noexcept
{
  return (static_cast<uint64>(static_cast<uint32>(x)) << 32) | static_cast<uint32>(y);
}

NODISCARD FORCEINLINE uint32
hashCell(uint64 cell, uint32 shift) noexcept
{
  return static_cast<uint32>((cell * 0x9E3779B97F4A7C15ull) >> shift);
}

NODISCARD FORCEINLINE float
squaredDistanceToBox(const Vector2& point, const Vector2& minPoint, const Vector2& maxPoint)
{
  const float dx = Math::max(minPoint.x - point.x, Math::max(0.0f, point.x - maxPoint.x));
  const float dy = Math::max(minPoint.y - point.y, Math::max(0.0f, point.y - maxPoint.y));
  return dx * dx + dy * dy;
}
} // namespace

/*
 */
SpatialHash2D::SpatialHash2D(float cellSize)
{
  setCellSize(cellSize);
}

/*
 */
void
SpatialHash2D::setCellSize(float cellSize)
{
  m_cellSize = Math::max(cellSize, MIN_CELL_SIZE);
  m_inverseCellSize = 1.0f / m_cellSize;
  clear();
}

/*
 */
void
SpatialHash2D::clear() noexcept
{
  m_shapes.clear();
  m_entries.clear();
  m_bigShapes.clear();
  m_bBuilt = false;
}

/*
 */
void
SpatialHash2D::addCircle(const Vector2& center, float radius, uint32 userData)
{
  const float absoluteRadius = Math::abs(radius);
  addShape({.minPoint = Vector2(center.x - absoluteRadius, center.y - absoluteRadius),
            .maxPoint = Vector2(center.x + absoluteRadius, center.y + absoluteRadius),
            .center = center,
            .radius = absoluteRadius,
            .bCircle = true,
            .userData = userData});
}

/*
 */
void
SpatialHash2D::addBox(const Vector2& minPoint, const Vector2& maxPoint, uint32 userData)
{
  addShape({.minPoint = Vector2(Math::min(minPoint.x, maxPoint.x),
                                Math::min(minPoint.y, maxPoint.y)),
            .maxPoint = Vector2(Math::max(minPoint.x, maxPoint.x),
                                Math::max(minPoint.y, maxPoint.y)),
            .center = Vector2::ZERO,
            .radius = 0.0f,
            .bCircle = false,
            .userData = userData});
}

/*
 */
void
SpatialHash2D::addShape(const Shape& shape)
{
  m_bBuilt = false;
  const uint32 index = static_cast<uint32>(m_shapes.size());
  m_shapes.push_back(shape);

  const int32 firstX = toCell(shape.minPoint.x, m_inverseCellSize);
  const int32 firstY = toCell(shape.minPoint.y, m_inverseCellSize);
  const int32 lastX = toCell(shape.maxPoint.x, m_inverseCellSize);
  const int32 lastY = toCell(shape.maxPoint.y, m_inverseCellSize);
  const int64 cellCount = (static_cast<int64>(lastX) - firstX + 1) *
                          (static_cast<int64>(lastY) - firstY + 1);
  if (cellCount > MAX_CELLS_PER_SHAPE) {
    m_bigShapes.push_back(index);
    return;
  }
  for (int32 y = firstY; y <= lastY; ++y) {
    for (int32 x = firstX; x <= lastX; ++x) {
      m_entries.push_back({.cell = makeCellKey(x, y), .shape = index, .slot = 0});
    }
  }
}

/*
 */
void
SpatialHash2D::build()
{
  // Half full at most, so probes stay short.
  SIZE_T slotCount = 16;
  uint32 slotBits = 4;
  while (slotCount < m_entries.size() * 2) {
    slotCount <<= 1;
    ++slotBits;
  }
  m_slotShift = 64 - slotBits;
  const uint32 mask = static_cast<uint32>(slotCount - 1);
  m_slots.assign(slotCount, CellSlot{.cell = 0, .first = 0, .count = 0, .bUsed = false});

  for (CellEntry& entry : m_entries) {
    uint32 slot = hashCell(entry.cell, m_slotShift);
    while (m_slots[slot].bUsed && m_slots[slot].cell != entry.cell) {
      slot = (slot + 1) & mask;
    }
    CellSlot& cellSlot = m_slots[slot];
    cellSlot.bUsed = true;
    cellSlot.cell = entry.cell;
    ++cellSlot.count;
    entry.slot = slot;
  }

  // The count becomes the write cursor of the second pass.
  uint32 offset = 0;
  for (CellSlot& cellSlot : m_slots) {
    cellSlot.first = offset;
    offset += cellSlot.count;
    cellSlot.count = 0;
  }
  m_cellShapes.resize(m_entries.size());
  for (const CellEntry& entry : m_entries) {
    CellSlot& cellSlot = m_slots[entry.slot];
    m_cellShapes[cellSlot.first + cellSlot.count++] = entry.shape;
  }

  m_lastQuery.assign(m_shapes.size(), 0);
  m_queryId = 0;
  m_bBuilt = true;
}

/*
 */
const SpatialHash2D::CellSlot*
SpatialHash2D::findSlot(uint64 cell) const
{
  const uint32 mask = static_cast<uint32>(m_slots.size() - 1);
  uint32 slot = hashCell(cell, m_slotShift);
  while (m_slots[slot].bUsed) {
    if (m_slots[slot].cell == cell) {
      return &m_slots[slot];
    }
    slot = (slot + 1) & mask;
  }
  return nullptr;
}

/*
 */
template<typename Overlaps>
void
SpatialHash2D::query(const Vector2& minPoint,
                     const Vector2& maxPoint,
                     Overlaps overlaps,
                     Vector<uint32>& outUserData) const
{
  outUserData.clear();
  CH_ASSERT(m_bBuilt && "SpatialHash2D::build must run after the adds and before queries");
  if (!m_bBuilt || m_shapes.empty()) {
    return;
  }
  if (++m_queryId == 0) {
    m_lastQuery.assign(m_shapes.size(), 0);
    m_queryId = 1;
  }

  const auto visit = [&](const uint32 shapeIndex) {
    if (m_lastQuery[shapeIndex] == m_queryId) {
      return;
    }
    m_lastQuery[shapeIndex] = m_queryId;
    const Shape& shape = m_shapes[shapeIndex];
    if (overlaps(shape)) {
      outUserData.push_back(shape.userData);
    }
  };

  const int32 firstX = toCell(minPoint.x, m_inverseCellSize);
  const int32 firstY = toCell(minPoint.y, m_inverseCellSize);
  const int32 lastX = toCell(maxPoint.x, m_inverseCellSize);
  const int32 lastY = toCell(maxPoint.y, m_inverseCellSize);
  const int64 cellCount = (static_cast<int64>(lastX) - firstX + 1) *
                          (static_cast<int64>(lastY) - firstY + 1);
  // A query wider than the world is faster as one walk over every shape.
  if (cellCount > static_cast<int64>(m_shapes.size())) {
    for (uint32 shape = 0; shape < m_shapes.size(); ++shape) {
      visit(shape);
    }
    return;
  }
  for (int32 y = firstY; y <= lastY; ++y) {
    for (int32 x = firstX; x <= lastX; ++x) {
      const CellSlot* slot = findSlot(makeCellKey(x, y));
      if (!slot) {
        continue;
      }
      for (uint32 i = 0; i < slot->count; ++i) {
        visit(m_cellShapes[slot->first + i]);
      }
    }
  }
  for (const uint32 shape : m_bigShapes) {
    visit(shape);
  }
}

/*
 */
void
SpatialHash2D::queryCircle(const Vector2& center,
                           float radius,
                           Vector<uint32>& outUserData) const
{
  const float absoluteRadius = Math::abs(radius);
  query(Vector2(center.x - absoluteRadius, center.y - absoluteRadius),
        Vector2(center.x + absoluteRadius, center.y + absoluteRadius),
        [&center, absoluteRadius](const Shape& shape) {
          if (shape.bCircle) {
            const float reach = shape.radius + absoluteRadius;
            return (shape.center - center).sqrMagnitude() <= reach * reach;
          }
          return squaredDistanceToBox(center, shape.minPoint, shape.maxPoint) <=
                 absoluteRadius * absoluteRadius;
        },
        outUserData);
}

/*
 */
void
SpatialHash2D::queryBox(const Vector2& minPoint,
                        const Vector2& maxPoint,
                        Vector<uint32>& outUserData) const
{
  const Vector2 low(Math::min(minPoint.x, maxPoint.x), Math::min(minPoint.y, maxPoint.y));
  const Vector2 high(Math::max(minPoint.x, maxPoint.x), Math::max(minPoint.y, maxPoint.y));
  query(low, high,
        [&low, &high](const Shape& shape) {
          if (shape.bCircle) {
            return squaredDistanceToBox(shape.center, low, high) <=
                   shape.radius * shape.radius;
          }
          return shape.minPoint.x <= high.x && shape.maxPoint.x >= low.x &&
                 shape.minPoint.y <= high.y && shape.maxPoint.y >= low.y;
        },
        outUserData);
}

/*
 */
void
SpatialHash2D::queryPoint(const Vector2& point, Vector<uint32>& outUserData) const
{
  queryBox(point, point, outUserData);
}

} // namespace chEngineSDK
