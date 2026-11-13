/************************************************************************/
/**
 * @file chSpriteItem.h
 * @author AccelMR
 * @date 2026/11/12
 * @brief
 *  A sprite as the renderer sees it, kept in a flat list by the Scene.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chVector3.h"

namespace chEngineSDK {

/**
 * How a sprite mixes with what is drawn before it. Saved in scenes, so the values are fixed.
 */
enum class SpriteBlendMode : uint8
{
  // color * alpha + below * (1 - alpha)
  Alpha = 0,
  // color * alpha + below, for glows and sparks.
  Additive = 1,
  COUNT
};

/**
 * How a sprite's texture is read. Saved in scenes, so the values are fixed.
 */
enum class SpriteFilter : uint8
{
  // Nearest texel, for pixel art.
  Point = 0,
  Linear = 1,
  COUNT
};

/**
 * How sprite.hlsl reads a sprite's texture, kept in GpuSpriteInstance::textureAndFlags.
 */
enum class SpriteTextureMode : uint32
{
  // The texture's color times the sprite's.
  Color = 0,
  // The red channel is the coverage: the sprite's color with that alpha (pixel fonts).
  Coverage = 1,
  // The red channel is a signed distance field whose edge is at 0.5 (text).
  Sdf = 2
};

/**
 * Exists so the renderer can upload the visible sprites as they are, without converting
 * them: one quad in world space, 64 bytes. Must match the layout read by sprite.hlsl.
 */
struct GpuSpriteInstance
{
  // Bit set in textureAndFlags when the texture is read with the Point filter.
  static constexpr uint32 POINT_FILTER_BIT = 0x80000000u;
  // The bindless index part of textureAndFlags, and its value when there is no texture.
  static constexpr uint32 TEXTURE_INDEX_MASK = 0x000FFFFFu;
  static constexpr uint32 NO_TEXTURE = TEXTURE_INDEX_MASK;
  static constexpr uint32 MODE_SHIFT = 20;
  static constexpr uint32 MODE_MASK = 0x3u;
  // SDF only: how far out the edge moves, in 1/255 of the field (outlines, bold).
  static constexpr uint32 EDGE_SHIFT = 22;
  static constexpr uint32 EDGE_MASK = 0xFFu;

  NODISCARD static constexpr uint32
  makeTextureFlags(uint32 bindlessIndex,
                   bool bPointFilter,
                   SpriteTextureMode mode = SpriteTextureMode::Color,
                   uint32 edgeOffset = 0) noexcept
  {
    return (bindlessIndex & TEXTURE_INDEX_MASK) |
           ((static_cast<uint32>(mode) & MODE_MASK) << MODE_SHIFT) |
           ((edgeOffset & EDGE_MASK) << EDGE_SHIFT) | (bPointFilter ? POINT_FILTER_BIT : 0u);
  }

  Vector3 center;
  // Bindless index of the texture (NO_TEXTURE draws the color alone), its SpriteTextureMode,
  // the SDF edge offset and POINT_FILTER_BIT; see makeTextureFlags.
  uint32 textureAndFlags;
  // Half the width of the quad, along its right side.
  Vector3 halfAxisU;
  // Red and green of the color as halves.
  uint32 colorRG;
  // Half the height of the quad, along its top side.
  Vector3 halfAxisV;
  // Blue and alpha of the color as halves.
  uint32 colorBA;
  // Left, top, width and height of the part of the texture shown, in 0..1 with v = 0 at
  // the top of the image. A negative width or height flips the image.
  float uvRect[4];
};
static_assert(sizeof(GpuSpriteInstance) == 64, "GpuSpriteInstance must match sprite.hlsl");

/**
 * Exists so short texts (numbers above all) cost one instance instead of one per glyph:
 * glyphrun.hlsl draws up to MAX_GLYPHS quads from it, reading each glyph's image from the
 * font's glyph table (Font::prepareGpuGlyphTable). Pen positions are computed on the CPU,
 * kerning and fixed digits included, so the shader has no loop. 128 bytes; must match
 * glyphrun.hlsl.
 */
struct GpuGlyphRun
{
  static constexpr uint32 MAX_GLYPHS = 16;
  static constexpr uint32 COUNT_MASK = 0x1Fu;
  // Pens are stored in 1/PEN_SCALE of the font's height, so a run reaches 64 heights.
  static constexpr float PEN_SCALE = 1024.0f;

  // World position of the run's pivot.
  Vector3 position;
  // Bindless index of the font's glyph table.
  uint32 glyphTableIndex;
  // The atlas, as GpuSpriteInstance::textureAndFlags.
  uint32 textureAndFlags;
  // Height of the font from its lowest to its highest point, in world units.
  float size;
  // From the pivot to the first pen on the baseline, in font heights.
  float offset[2];
  // Cosine and sine of the rotation, as halves.
  uint32 rotation;
  uint32 colorRG;
  uint32 colorBA;
  // Glyphs in the run, at most MAX_GLYPHS.
  uint32 glyphCount;
  // Free for systems that move runs in a shader of their own (start time, velocity...).
  uint32 userData[4];
  // Slots of the font's glyphs.
  uint16 glyphs[MAX_GLYPHS];
  // Pen of each glyph from the first one, in 1/PEN_SCALE of the font's height.
  uint16 pens[MAX_GLYPHS];
};
static_assert(sizeof(GpuGlyphRun) == 128, "GpuGlyphRun must match glyphrun.hlsl");

/**
 * A glyph run as the renderer sorts it with the sprites.
 */
struct GlyphRunItem
{
  GpuGlyphRun run{};
  // Distance from the position that holds every glyph, for culling.
  float radius = 0.0f;
  // From the first pen to the end of the last glyph's advance, in world units.
  float width = 0.0f;
  int16 sortingLayer = 0;
  int16 order = 0;
  SpriteBlendMode blendMode = SpriteBlendMode::Alpha;
};

/**
 * Glyph runs that live in a GPU buffer of their owner, drawn as they are without being
 * copied or culled: for systems that keep thousands of runs and only write the new ones.
 * It sorts with the sprites at depth 0 of its layer and order.
 */
struct GlyphRunBatch
{
  // Bindless index of a storage buffer of GpuGlyphRun.
  uint32 bufferIndex = 0;
  uint32 firstRun = 0;
  uint32 runCount = 0;
  int16 sortingLayer = 0;
  int16 order = 0;
  SpriteBlendMode blendMode = SpriteBlendMode::Alpha;
};

/**
 * Exists so the renderer reads sprites from a flat array instead of walking GameObjects and
 * components. Sprite components add one to their Scene and keep it up to date.
 */
struct SpriteItem
{
  GpuSpriteInstance instance{};
  // Distance from the center that holds every corner, for culling.
  float radius = 0.0f;
  // Sprites draw by layer, then by order inside a layer, lowest first.
  int16 sortingLayer = 0;
  int16 order = 0;
  SpriteBlendMode blendMode = SpriteBlendMode::Alpha;
  // The object the sprite belongs to, so the editor can find it from the sprite.
  GameObject* owner = nullptr;
  // False marks a free slot of the scene.
  bool bInUse = false;
};

/**
 * Exists so a text component's sprites reach the renderer like a sprite component's: it
 * keeps them here, in a slot of its Scene, rebuilt only when the text, its look or its
 * placement change. A text drawn as glyph runs keeps them in runs, one drawn glyph by glyph
 * keeps its sprites.
 */
struct TextItem
{
  // Shadow, outline and fill, as SpriteDraw::makeGlyphRuns gives them.
  static constexpr uint32 MAX_RUNS = 3;

  Vector<SpriteItem> sprites;
  GlyphRunItem runs[MAX_RUNS];
  uint32 runCount = 0;
  // The object the text belongs to.
  GameObject* owner = nullptr;
  // False marks a free slot of the scene.
  bool bInUse = false;
};

} // namespace chEngineSDK
