/************************************************************************/
/**
 * @file chSpriteDraw.h
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Sprites drawn for one frame without GameObjects.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chAngle.h"
#include "chLinearColor.h"
#include "chSpriteItem.h"
#include "chVector2.h"

namespace chEngineSDK {
class Font;
class TextLayout;
class TextureAsset;

/**
 * One quad in the YZ plane (right is +Y, up is +Z), as SpriteDraw::quad takes it. The
 * texture part comes already resolved (SpriteDraw::makeTextureFlags and makeUvRect), so a
 * system that draws many quads of one look computes it once.
 */
struct SpriteQuad
{
  // Y and Z of the point the pivot stands on.
  Vector2 position = Vector2::ZERO;
  // World X; sprites farther along the camera's forward axis draw first within an order.
  float depth = 0.0f;
  // Width and height in world units.
  Vector2 size{1.0f, 1.0f};
  // Counterclockwise on screen.
  Radian rotation = Radian(0.0f);
  // (0, 0) is the bottom left corner, (1, 1) the top right one.
  Vector2 pivot{0.5f, 0.5f};
  LinearColor color = LinearColor::WHITE;
  uint32 textureAndFlags = GpuSpriteInstance::NO_TEXTURE;
  float uvRect[4] = {0.0f, 0.0f, 1.0f, 1.0f};
  int16 sortingLayer = 0;
  int16 order = 0;
  SpriteBlendMode blendMode = SpriteBlendMode::Alpha;
};

/**
 * Where and how SpriteDraw draws a TextLayout, in the YZ plane like SpriteQuad.
 */
struct TextDrawDesc
{
  // Y and Z of the layout's pivot.
  Vector2 position = Vector2::ZERO;
  // World X.
  float depth = 0.0f;
  // Counterclockwise on screen, around the pivot.
  Radian rotation = Radian(0.0f);
  // Times the layout's size.
  float scale = 1.0f;
  int16 sortingLayer = 0;
  int16 order = 0;
  SpriteBlendMode blendMode = SpriteBlendMode::Alpha;
  // SDF fonts only: pixels of the font's raster size the outline reaches past the glyph,
  // up to its FontSettings::sdfSpread; 0 draws none.
  float outlineWidth = 0.0f;
  LinearColor outlineColor = LinearColor::BLACK;
  // A shadow is drawn when its alpha is above zero, moved by shadowOffset in world units
  // (not turned with the text).
  LinearColor shadowColor = LinearColor::TRANSPARENT_BLACK;
  Vector2 shadowOffset{0.05f, -0.05f};
  // Times the alpha of every glyph, its outline and its shadow, for fading a whole text.
  float opacity = 1.0f;
};

/**
 * A short text drawn as one GpuGlyphRun (plus one for the outline and one for the shadow),
 * in the YZ plane like SpriteQuad.
 */
struct GlyphRunDesc
{
  // Y and Z of the pivot.
  Vector2 position = Vector2::ZERO;
  // World X.
  float depth = 0.0f;
  // Counterclockwise on screen, around the pivot.
  Radian rotation = Radian(0.0f);
  // Height of the font from its lowest to its highest point, in world units.
  float size = 1.0f;
  // Point of the run that stands at position: (0, 0) bottom left, (1, 1) top right.
  Vector2 pivot{0.5f, 0.5f};
  LinearColor color = LinearColor::WHITE;
  // Every digit takes the width of the widest, so numbers that change do not move.
  bool bMonospaceDigits = false;
  int16 sortingLayer = 0;
  int16 order = 0;
  SpriteBlendMode blendMode = SpriteBlendMode::Alpha;
  // As in TextDrawDesc.
  float outlineWidth = 0.0f;
  LinearColor outlineColor = LinearColor::BLACK;
  LinearColor shadowColor = LinearColor::TRANSPARENT_BLACK;
  Vector2 shadowOffset{0.05f, -0.05f};
};

/**
 * Exists so systems that draw many short-lived images (bullets, particles, effects) need no
 * GameObject per image: they add sprites every frame and the renderer sorts them with the
 * sprite components, by layer and order. Each Scene has one, cleared when the scene
 * updates, and one for the fixed step, cleared when a fixed step starts, so what a fixed
 * step adds shows on every frame until the next one. The sprites are kept in a vector that
 * keeps its memory after clear(), so adding the same number every frame does not allocate.
 * Main thread only.
 *
 * Sprites added here are not picked by Scene::raycast: they have no object to give back.
 */
class CH_CORE_EXPORT SpriteDraw
{
 public:
  void
  quad(const SpriteQuad& quad);

  /**
   * The fastest way, for callers that build the instance themselves (see sprite.hlsl for
   * what each field means). radius is the distance from the center that holds every
   * corner.
   */
  void
  instance(const GpuSpriteInstance& instance,
           float radius,
           int16 sortingLayer = 0,
           int16 order = 0,
           SpriteBlendMode blendMode = SpriteBlendMode::Alpha);

  /**
   * One sprite per glyph, plus one per glyph for the shadow and for the outline when desc
   * asks for them, drawn behind it. Uploads the font's new glyphs first.
   */
  void
  text(Font& font, const TextLayout& layout, const TextDrawDesc& desc);

  /**
   * The sprites text() adds, appended to outSprites, for callers that keep them between
   * frames. They read the font's atlas as it is at the call: rebuild them when
   * Font::getRevision() changes.
   */
  static void
  appendTextSprites(Font& font,
                    const TextLayout& layout,
                    const TextDrawDesc& desc,
                    Vector<SpriteItem>& outSprites);

  /**
   * The first GpuGlyphRun::MAX_GLYPHS characters of text as one glyph run (line breaks are
   * skipped, there are no color tags), plus the outline and shadow runs behind it. Meant for
   * numbers and labels that change every frame: no layout object, one instance per run.
   */
  void
  glyphRun(Font& font, StringView text, const GlyphRunDesc& desc);

  /**
   * A run built by makeGlyphRun, or by hand.
   */
  void
  glyphRun(const GlyphRunItem& item);

  /**
   * Runs kept in a GPU buffer by their owner, drawn as they are.
   */
  void
  glyphRunBatch(const GlyphRunBatch& batch);

  /**
   * Fills outItem with the fill run of text, without its outline or shadow. False when the
   * font cannot be read or text has no characters. Draws new glyphs into the font and
   * uploads its atlas and glyph table, so main thread only.
   */
  NODISCARD static bool
  makeGlyphRun(Font& font, StringView text, const GlyphRunDesc& desc, GlyphRunItem& outItem);

  NODISCARD FORCEINLINE Span<const GlyphRunItem>
  getGlyphRuns() const noexcept
  {
    return m_glyphRuns;
  }

  NODISCARD FORCEINLINE Span<const GlyphRunBatch>
  getGlyphRunBatches() const noexcept
  {
    return m_glyphRunBatches;
  }

  /**
   * Forgets every sprite but keeps the memory.
   */
  void
  clear() noexcept;

  NODISCARD FORCEINLINE Span<const SpriteItem>
  getSprites() const noexcept
  {
    return m_sprites;
  }

  /**
   * Bindless index of the texture's GPU copy (NO_TEXTURE without one) plus the filter bit.
   */
  NODISCARD static uint32
  makeTextureFlags(const TextureAsset* texture, SpriteFilter filter);

  /**
   * The part of the texture a source rectangle in pixels shows, as sprite.hlsl reads it. A
   * zero size, or a texture without a size yet, gives the whole texture.
   */
  static void
  makeUvRect(const TextureAsset* texture,
             const Vector2& sourcePosition,
             const Vector2& sourceSize,
             bool bFlipX,
             bool bFlipY,
             float (&outUvRect)[4]);

  /**
   * The color as the two words of halves GpuSpriteInstance keeps.
   */
  static void
  packColor(const LinearColor& color, uint32& outRG, uint32& outBA);

 private:
  NODISCARD bool
  hasRoom(SIZE_T used, SIZE_T count);

  Vector<SpriteItem> m_sprites;
  Vector<GlyphRunItem> m_glyphRuns;
  Vector<GlyphRunBatch> m_glyphRunBatches;
  bool m_bWarnedFull = false;
};

} // namespace chEngineSDK
