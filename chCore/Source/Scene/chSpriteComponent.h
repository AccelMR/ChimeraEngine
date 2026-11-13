/************************************************************************/
/**
 * @file chSpriteComponent.h
 * @author AccelMR
 * @date 2026/11/12
 * @brief
 *  Component that draws a textured quad (a sprite) at its GameObject.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chComponent.h"
#include "chLinearColor.h"
#include "chSpriteItem.h"
#include "chTypeTraits.h"
#include "chUUID.h"
#include "chVector2.h"

namespace chEngineSDK {
class SpriteAtlasAsset;
class TextureAsset;
struct SpriteAtlasFrame;

/**
 * Exists to draw 2D images: it shows part of a texture as a quad that faces -X, with its
 * width along the GameObject's right axis (Y) and its height along its up axis (Z), so a
 * camera looking along +X sees it as a flat image. Its size in world units is the source
 * rectangle in pixels divided by the pixels per unit, times the GameObject's scale. It shows
 * either a texture (with an optional source rectangle and pivot of its own) or one frame of
 * a sprite atlas, whose texture, rectangle and pivot are used instead. While registered it
 * adds one sprite item to the scene and moves it with the GameObject.
 */
class CH_CORE_EXPORT SpriteComponent : public Component
{
 public:
  SpriteComponent() = default;

  /**
   * Null draws a quad of the color alone. Leaves the atlas, if one was shown.
   */
  void
  setTexture(const SPtr<TextureAsset>& texture);

  /**
   * Shows frame (an index into the atlas' frames) of atlas instead of a texture. A null
   * atlas goes back to no texture; a frame past the atlas' frames shows its whole texture.
   */
  void
  setAtlasFrame(const SPtr<SpriteAtlasAsset>& atlas, uint32 frame);

  /**
   * Another frame of the same atlas; cheap enough to call every frame (animations). Does
   * nothing without an atlas.
   */
  void
  setFrame(uint32 frame);

  NODISCARD FORCEINLINE const SPtr<SpriteAtlasAsset>&
  getAtlas() const noexcept { return m_atlas; }

  /**
   * The atlas asked for, kept even when the asset is missing.
   */
  NODISCARD FORCEINLINE const UUID&
  getAtlasId() const noexcept { return m_atlasId; }

  NODISCARD FORCEINLINE uint32
  getFrame() const noexcept { return m_frame; }

#if USING(CH_EDITOR)
  /**
   * Reads the atlas frame again, after the atlas was edited.
   */
  FORCEINLINE void
  refreshAtlasFrame() { updateSpriteItem(); }
#endif // USING(CH_EDITOR)

  NODISCARD FORCEINLINE const SPtr<TextureAsset>&
  getTexture() const noexcept { return m_texture; }

  /**
   * The texture asked for, kept even when the asset is missing, so saving again keeps it.
   */
  NODISCARD FORCEINLINE const UUID&
  getTextureId() const noexcept { return m_textureId; }

  /**
   * The part of the texture shown, in pixels from the top left corner of the image. A zero
   * size shows the whole texture.
   */
  void
  setSourceRect(const Vector2& position, const Vector2& size);

  NODISCARD FORCEINLINE const Vector2&
  getSourcePosition() const noexcept { return m_sourcePosition; }

  NODISCARD FORCEINLINE const Vector2&
  getSourceSize() const noexcept { return m_sourceSize; }

  /**
   * How many texture pixels make one world unit. Kept above MIN_PIXELS_PER_UNIT.
   */
  void
  setPixelsPerUnit(float pixelsPerUnit);

  NODISCARD FORCEINLINE float
  getPixelsPerUnit() const noexcept { return m_pixelsPerUnit; }

  /**
   * The point of the image that stands at the GameObject's position: (0, 0) is the bottom
   * left corner, (1, 1) the top right one.
   */
  void
  setPivot(const Vector2& pivot);

  NODISCARD FORCEINLINE const Vector2&
  getPivot() const noexcept { return m_pivot; }

  /**
   * Multiplies the texture, alpha included.
   */
  void
  setColor(const LinearColor& color);

  NODISCARD FORCEINLINE const LinearColor&
  getColor() const noexcept { return m_color; }

  void
  setFlip(bool bFlipX, bool bFlipY);

  NODISCARD FORCEINLINE bool
  isFlippedX() const noexcept { return m_bFlipX; }

  NODISCARD FORCEINLINE bool
  isFlippedY() const noexcept { return m_bFlipY; }

  /**
   * Sprites draw by layer, then by order inside a layer, lowest first.
   */
  void
  setSorting(int16 sortingLayer, int16 order);

  NODISCARD FORCEINLINE int16
  getSortingLayer() const noexcept { return m_sortingLayer; }

  NODISCARD FORCEINLINE int16
  getOrder() const noexcept { return m_order; }

  void
  setBlendMode(SpriteBlendMode blendMode);

  NODISCARD FORCEINLINE SpriteBlendMode
  getBlendMode() const noexcept { return m_blendMode; }

  void
  setFilter(SpriteFilter filter);

  NODISCARD FORCEINLINE SpriteFilter
  getFilter() const noexcept { return m_filter; }

  /**
   * Width and height in world units before the GameObject's scale. Without a texture and
   * without a source size it is one unit square.
   */
  NODISCARD Vector2
  getSize() const;

  NODISCARD bool
  getWorldBounds(AABox& outBounds) const override;

  void
  serialize(DataStream& stream) const override;

  NODISCARD bool
  deserialize(DataStream& stream) override;

  void
  collectReferences(Vector<UUID>& outReferences) const override;

  static constexpr uint32 SERIALIZATION_VERSION = 1;
  static constexpr uint32 NO_FRAME = 0xFFFFFFFFu;
  static constexpr float DEFAULT_PIXELS_PER_UNIT = 100.0f;
  static constexpr float MIN_PIXELS_PER_UNIT = 0.001f;

 protected:
  void
  onRegister() override;

  void
  onUnregister() override;

  void
  onTransformChanged() override;

 private:
  void
  updateSpriteItem();

  /**
   * The atlas frame shown, null when the sprite shows a texture or the frame does not exist.
   */
  NODISCARD const SpriteAtlasFrame*
  getAtlasFrame() const;

  /**
   * The texture drawn: the atlas' in atlas mode, else the component's.
   */
  NODISCARD const TextureAsset*
  getShownTexture() const;

  SPtr<TextureAsset> m_texture;
  UUID m_textureId = UUID::null();
  SPtr<SpriteAtlasAsset> m_atlas;
  UUID m_atlasId = UUID::null();
  uint32 m_frame = NO_FRAME;
  Vector2 m_sourcePosition = Vector2::ZERO;
  Vector2 m_sourceSize = Vector2::ZERO;
  float m_pixelsPerUnit = DEFAULT_PIXELS_PER_UNIT;
  Vector2 m_pivot{0.5f, 0.5f};
  LinearColor m_color = LinearColor::WHITE;
  int16 m_sortingLayer = 0;
  int16 m_order = 0;
  SpriteBlendMode m_blendMode = SpriteBlendMode::Alpha;
  SpriteFilter m_filter = SpriteFilter::Point;
  bool m_bFlipX = false;
  bool m_bFlipY = false;
  uint32 m_spriteItemId = 0;
};

DECLARE_TYPE_TRAITS(SpriteComponent)

} // namespace chEngineSDK
