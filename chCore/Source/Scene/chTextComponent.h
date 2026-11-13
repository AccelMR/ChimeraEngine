/************************************************************************/
/**
 * @file chTextComponent.h
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Component that draws a text at its GameObject.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chComponent.h"
#include "chLinearColor.h"
#include "chNumberFormat.h"
#include "chSpriteItem.h"
#include "chTextLayout.h"
#include "chTypeTraits.h"
#include "chVector2.h"

namespace chEngineSDK {
class Font;
class FontAsset;

/**
 * Exists to draw a label, a counter or a dialog line in the YZ plane of a 2D game, facing -X
 * along its GameObject's right and up axes (rotation around X turns it, the up axis' length
 * scales it; non-uniform scale is not kept). A text that fits one glyph run (at most 16
 * characters, no line break, no color tags, inside the wrap width) is drawn as one, so a
 * counter costs one instance; a longer one is laid out once and drawn glyph by glyph.
 *
 * Setters only mark what changed: the next update lays the text out again when the text or
 * the layout changed, and builds its sprites again when the look, the placement or the
 * font's atlas changed. Frames without changes cost a few compares. Without a font, or
 * with one whose asset is missing, it uses Font::getDefault().
 */
class CH_CORE_EXPORT TextComponent : public Component
{
 public:
  TextComponent();

  explicit TextComponent(StringView text, const SPtr<FontAsset>& font = nullptr);

  void
  setText(StringView text);

  NODISCARD FORCEINLINE const String&
  getText() const noexcept { return m_text; }

  /**
   * Writes value with the number format; the text is laid out again only when the written
   * number differs from the one shown, so calling it every frame is fine.
   */
  void
  setNumber(double value);

  void
  setNumberFormat(const NumberFormatDesc& format);

  NODISCARD FORCEINLINE const NumberFormatDesc&
  getNumberFormat() const noexcept { return m_numberFormat; }

  /**
   * Null uses the default font.
   */
  void
  setFont(const SPtr<FontAsset>& font);

  NODISCARD FORCEINLINE const SPtr<FontAsset>&
  getFont() const noexcept { return m_font; }

  /**
   * Kept when the asset is missing, so saving again keeps the reference.
   */
  NODISCARD FORCEINLINE const UUID&
  getFontId() const noexcept { return m_fontId; }

  /**
   * The font asset with that UUID, loaded; null uses the default font. An id with no asset
   * is kept (saving keeps the reference) and draws with the default font.
   */
  void
  setFontId(const UUID& fontId);

  /**
   * Height of the font from its lowest to its highest point, in world units before the
   * GameObject's scale.
   */
  void
  setSize(float size);

  NODISCARD FORCEINLINE float
  getSize() const noexcept { return m_size; }

  void
  setColor(const LinearColor& color);

  NODISCARD FORCEINLINE const LinearColor&
  getColor() const noexcept { return m_color; }

  void
  setAlign(TextAlign align);

  NODISCARD FORCEINLINE TextAlign
  getAlign() const noexcept { return m_align; }

  /**
   * Lines break at this width (world units before scale); 0 never breaks.
   */
  void
  setWrapWidth(float width);

  NODISCARD FORCEINLINE float
  getWrapWidth() const noexcept { return m_wrapWidth; }

  void
  setLineSpacing(float spacing);

  NODISCARD FORCEINLINE float
  getLineSpacing() const noexcept { return m_lineSpacing; }

  void
  setLetterSpacing(float spacing);

  NODISCARD FORCEINLINE float
  getLetterSpacing() const noexcept { return m_letterSpacing; }

  /**
   * Point of the text at the GameObject: (0, 0) bottom left, (1, 1) top right.
   */
  void
  setPivot(const Vector2& pivot);

  NODISCARD FORCEINLINE const Vector2&
  getPivot() const noexcept { return m_pivot; }

  void
  setMonospaceDigits(bool bMonospace);

  NODISCARD FORCEINLINE bool
  isMonospaceDigits() const noexcept { return m_bMonospaceDigits; }

  /**
   * Whether <color=#rrggbb> tags are read; with them off, '<' is plain text.
   */
  void
  setRichText(bool bRichText);

  NODISCARD FORCEINLINE bool
  isRichText() const noexcept { return m_bRichText; }

  void
  setSorting(int16 sortingLayer, int16 order);

  void
  setSortingLayer(int16 sortingLayer);

  void
  setOrder(int16 order);

  NODISCARD FORCEINLINE int16
  getSortingLayer() const noexcept { return m_sortingLayer; }

  NODISCARD FORCEINLINE int16
  getOrder() const noexcept { return m_order; }

  void
  setBlendMode(SpriteBlendMode blendMode);

  NODISCARD FORCEINLINE SpriteBlendMode
  getBlendMode() const noexcept { return m_blendMode; }

  /**
   * SDF fonts only: width in pixels of the font's raster size, 0 for none.
   */
  void
  setOutline(float width, const LinearColor& color);

  void
  setOutlineWidth(float width);

  void
  setOutlineColor(const LinearColor& color);

  NODISCARD FORCEINLINE float
  getOutlineWidth() const noexcept { return m_outlineWidth; }

  NODISCARD FORCEINLINE const LinearColor&
  getOutlineColor() const noexcept { return m_outlineColor; }

  /**
   * Drawn when the color's alpha is above zero; offset in world units.
   */
  void
  setShadow(const LinearColor& color, const Vector2& offset);

  void
  setShadowColor(const LinearColor& color);

  void
  setShadowOffset(const Vector2& offset);

  NODISCARD FORCEINLINE const LinearColor&
  getShadowColor() const noexcept { return m_shadowColor; }

  NODISCARD FORCEINLINE const Vector2&
  getShadowOffset() const noexcept { return m_shadowOffset; }

  /**
   * True when the shown text is drawn as glyph runs, false when glyph by glyph.
   */
  NODISCARD FORCEINLINE bool
  isDrawnAsGlyphRuns() const noexcept { return m_bGlyphRuns; }

  /**
   * Width and height of the shown text in world units before the GameObject's scale, as of
   * the last update.
   */
  NODISCARD FORCEINLINE const Vector2&
  getTextSize() const noexcept { return m_textSize; }

  void
  update(float deltaTime) override;

  NODISCARD bool
  getWorldBounds(AABox& outBounds) const override;

  void
  serialize(DataStream& stream) const override;

  NODISCARD bool
  deserialize(DataStream& stream) override;

  void
  collectReferences(Vector<UUID>& outReferences) const override;

  static constexpr uint32 SERIALIZATION_VERSION = 1;
  // Longer texts are cut when saved; only a broken file stores more.
  static constexpr uint32 MAX_TEXT_LENGTH = 64 * 1024;

 protected:
  void
  onRegister() override;

  void
  onUnregister() override;

  void
  onTransformChanged() override;

 private:
  NODISCARD Font*
  getShownFont() const;

  /**
   * Builds the text's sprites or runs into its scene slot, laying it out again first when
   * bLayout. Clears the dirty flags.
   */
  void
  rebuild(bool bLayout);

  SPtr<FontAsset> m_font;
  UUID m_fontId = UUID::null();
  String m_text;
  NumberFormatDesc m_numberFormat{};
  float m_size = 1.0f;
  LinearColor m_color = LinearColor::WHITE;
  TextAlign m_align = TextAlign::Left;
  float m_wrapWidth = 0.0f;
  float m_lineSpacing = 1.0f;
  float m_letterSpacing = 0.0f;
  Vector2 m_pivot{0.5f, 0.5f};
  bool m_bMonospaceDigits = false;
  bool m_bRichText = true;
  int16 m_sortingLayer = 0;
  int16 m_order = 0;
  SpriteBlendMode m_blendMode = SpriteBlendMode::Alpha;
  float m_outlineWidth = 0.0f;
  LinearColor m_outlineColor = LinearColor::BLACK;
  LinearColor m_shadowColor = LinearColor::TRANSPARENT_BLACK;
  Vector2 m_shadowOffset{0.05f, -0.05f};

  TextLayout m_layout;
  uint32 m_textId = 0;
  bool m_bLayoutDirty = true;
  bool m_bLookDirty = true;
  bool m_bGlyphRuns = false;
  Vector2 m_textSize = Vector2::ZERO;
  const Font* m_shownFont = nullptr;
  uint32 m_shownRevision = 0;
};

DECLARE_TYPE_TRAITS(TextComponent)

} // namespace chEngineSDK
