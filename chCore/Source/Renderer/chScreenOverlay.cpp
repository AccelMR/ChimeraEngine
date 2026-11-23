/************************************************************************/
/**
 * @file chScreenOverlay.cpp
 * @author AccelMR
 * @date 2026/11/23
 * @brief Sprites and text drawn over the final image, in window pixels.
 */
/************************************************************************/
#include "chScreenOverlay.h"

#include "chFont.h"
#include "chTextLayout.h"

namespace chEngineSDK {

/*
 */
bool
ScreenOverlay::isEmpty() const noexcept
{
  return m_draw.getSprites().empty() && m_draw.getGlyphRuns().empty();
}

/*
 */
void
ScreenOverlay::rectangle(const Vector2& position,
                         const Vector2& size,
                         const LinearColor& color,
                         const int16 sortingLayer,
                         const int16 order)
{
  // (0, 1) is the top left corner in the plane, whose up is the screen's up.
  m_draw.quad({.position = toPlane(position),
               .size = size,
               .pivot = Vector2(0.0f, 1.0f),
               .color = color,
               .sortingLayer = sortingLayer,
               .order = order});
}

/*
 */
void
ScreenOverlay::text(Font& font,
                    const TextLayout& layout,
                    const Vector2& position,
                    const int16 sortingLayer,
                    const int16 order)
{
  m_draw.text(font,
              layout,
              {.position = toPlane(position), .sortingLayer = sortingLayer, .order = order});
}

} // namespace chEngineSDK
