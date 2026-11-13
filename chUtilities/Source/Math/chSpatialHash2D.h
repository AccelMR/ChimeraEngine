/************************************************************************/
/**
 * @file chSpatialHash2D.h
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Finds the circles and boxes of a 2D world near a point, circle or box.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesUtilities.h"

#include "chVector2.h"

namespace chEngineSDK {

/**
 * Exists so systems with thousands of moving things (bullets, pickups, enemies) can ask what
 * is near something without testing every pair: shapes go into the square cells of a grid
 * they touch, and a query only tests the shapes of the cells it touches. It knows nothing of
 * games: each shape carries a number of the caller's (an index into its own arrays), which
 * queries give back.
 *
 * Meant to be filled again every frame: clear(), the adds, build(), then any number of
 * queries. Every buffer keeps its memory, so filling it the same way again does not
 * allocate. A shape that covers many cells is kept apart and tested by every query instead.
 * Queries are const but share a scratch buffer, so one hash is used from one thread at a
 * time.
 */
class CH_UTILITY_EXPORT SpatialHash2D
{
 public:
  /**
   * cellSize is the side of a cell in world units; about the size of the common shapes
   * works best. Kept above MIN_CELL_SIZE.
   */
  explicit SpatialHash2D(float cellSize = 1.0f);

  /**
   * Also forgets every shape.
   */
  void
  setCellSize(float cellSize);

  NODISCARD FORCEINLINE float
  getCellSize() const noexcept
  {
    return m_cellSize;
  }

  /**
   * Forgets every shape but keeps the memory.
   */
  void
  clear() noexcept;

  void
  addCircle(const Vector2& center, float radius, uint32 userData);

  void
  addBox(const Vector2& minPoint, const Vector2& maxPoint, uint32 userData);

  /**
   * Sorts the shapes into their cells. Call it after the adds and before the queries; a
   * shape added later is only found after the next build.
   */
  void
  build();

  NODISCARD FORCEINLINE uint32
  getShapeCount() const noexcept
  {
    return static_cast<uint32>(m_shapes.size());
  }

  /**
   * Replaces the contents of outUserData with the numbers of the shapes the circle
   * touches, each once. Touching counts.
   */
  void
  queryCircle(const Vector2& center, float radius, Vector<uint32>& outUserData) const;

  void
  queryBox(const Vector2& minPoint,
           const Vector2& maxPoint,
           Vector<uint32>& outUserData) const;

  void
  queryPoint(const Vector2& point, Vector<uint32>& outUserData) const;

  static constexpr float MIN_CELL_SIZE = 1e-4f;
  // A shape over more cells than this is kept apart and tested by every query.
  static constexpr uint32 MAX_CELLS_PER_SHAPE = 64;

 private:
  struct Shape
  {
    Vector2 minPoint;
    Vector2 maxPoint;
    // Unused by boxes.
    Vector2 center;
    float radius;
    bool bCircle;
    uint32 userData;
  };

  struct CellEntry
  {
    uint64 cell;
    uint32 shape;
    // The slot of its cell, found by build.
    uint32 slot;
  };

  struct CellSlot
  {
    uint64 cell;
    uint32 first;
    uint32 count;
    bool bUsed;
  };

  void
  addShape(const Shape& shape);

  NODISCARD const CellSlot*
  findSlot(uint64 cell) const;

  /**
   * Every shape in the cells between the corners, plus the big ones, tested with overlaps.
   */
  template<typename Overlaps>
  void
  query(const Vector2& minPoint,
        const Vector2& maxPoint,
        Overlaps overlaps,
        Vector<uint32>& outUserData) const;

  float m_cellSize = 1.0f;
  float m_inverseCellSize = 1.0f;
  Vector<Shape> m_shapes;
  Vector<CellEntry> m_entries;
  // Shapes over more than MAX_CELLS_PER_SHAPE cells.
  Vector<uint32> m_bigShapes;
  // Open addressing table, a power of two long.
  Vector<CellSlot> m_slots;
  // The shapes of every cell, one cell after the other.
  Vector<uint32> m_cellShapes;
  // For each shape the last query that reported it, so a shape over several cells is
  // reported once.
  mutable Vector<uint32> m_lastQuery;
  mutable uint32 m_queryId = 0;
  // 64 minus the bits of the table size, for the hash of a cell.
  uint32 m_slotShift = 60;
  bool m_bBuilt = false;
};

} // namespace chEngineSDK
