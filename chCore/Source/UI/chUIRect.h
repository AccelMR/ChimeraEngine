/************************************************************************/
/**
 * @file chUIRect.h
 * @author Swampertor
 * @date 2026/11/19
 * @brief Rectangles and margins in UI space.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chVector2.h"

namespace chEngineSDK {

/**
 * Space kept free around a widget's content, as in UMG's FMargin: left and top grow the
 * rectangle in, right and bottom shrink it from the far edges.
 */
struct UIMargin
{
  float left = 0.0f;
  float top = 0.0f;
  float right = 0.0f;
  float bottom = 0.0f;

  NODISCARD FORCEINLINE float
  getHorizontal() const noexcept
  {
    return left + right;
  }

  NODISCARD FORCEINLINE float
  getVertical() const noexcept
  {
    return top + bottom;
  }

  NODISCARD FORCEINLINE bool
  isEmpty() const noexcept
  {
    return left == 0.0f && top == 0.0f && right == 0.0f && bottom == 0.0f;
  }
};

/**
 * A rectangle in UI space: position is the top-left corner and size extends right and
 * down. UI space starts at the top-left of the viewport, unlike the renderer's YZ plane,
 * so layout code and the draw list convert only when they hand sprites to SpriteDraw.
 */
struct UIRect
{
  Vector2 position = Vector2::ZERO;
  Vector2 size = Vector2::ZERO;

  NODISCARD FORCEINLINE Vector2
  getCenter() const noexcept
  {
    return position + size * 0.5f;
  }

  NODISCARD FORCEINLINE Vector2
  getMin() const noexcept
  {
    return position;
  }

  NODISCARD FORCEINLINE Vector2
  getMax() const noexcept
  {
    return position + size;
  }

  NODISCARD FORCEINLINE bool
  isEmpty() const noexcept
  {
    return size.x <= 0.0f || size.y <= 0.0f;
  }

  /**
   * True for points on the edges too, so a widget whose edge the pointer is on can still
   * be hit.
   */
  NODISCARD FORCEINLINE bool
  contains(const Vector2& point) const noexcept
  {
    return point.x >= position.x && point.y >= position.y &&
           point.x <= position.x + size.x && point.y <= position.y + size.y;
  }

  /**
   * True when the rectangles share area; rectangles that only touch do not overlap.
   */
  NODISCARD FORCEINLINE bool
  overlaps(const UIRect& other) const noexcept
  {
    return position.x < other.position.x + other.size.x &&
           other.position.x < position.x + size.x &&
           position.y < other.position.y + other.size.y &&
           other.position.y < position.y + size.y;
  }

  /**
   * The rectangle moved in by margin; a margin larger than the rectangle gives a zero
   * size instead of a negative one.
   */
  NODISCARD FORCEINLINE UIRect
  inset(const UIMargin& margin) const noexcept
  {
    const float width = size.x - margin.left - margin.right;
    const float height = size.y - margin.top - margin.bottom;
    return {{position.x + margin.left, position.y + margin.top},
            {width > 0.0f ? width : 0.0f, height > 0.0f ? height : 0.0f}};
  }
};

/**
 * Where a widget was arranged: the rectangle the parent's slot gave it, the scale
 * inherited from the viewport, and the clip it is inside. Panels pass the same geometry
 * to their children after their slot moves it.
 */
struct UIGeometry
{
  Vector2 position = Vector2::ZERO;
  Vector2 size = Vector2::ZERO;
  float scale = 1.0f;
  bool bClipped = false;
  UIRect clipRect;

  NODISCARD FORCEINLINE UIRect
  toRect() const noexcept
  {
    return {position, size};
  }

  NODISCARD FORCEINLINE Vector2
  getCenter() const noexcept
  {
    return position + size * 0.5f;
  }
};

} // namespace chEngineSDK
