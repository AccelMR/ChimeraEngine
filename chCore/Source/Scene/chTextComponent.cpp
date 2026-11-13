/************************************************************************/
/**
 * @file chTextComponent.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Component that draws a text at its GameObject.
 */
/************************************************************************/
#include "chTextComponent.h"

#include "chAssetFile.h"
#include "chAssetManager.h"
#include "chBox.h"
#include "chFileStream.h"
#include "chFont.h"
#include "chFontAsset.h"
#include "chGameObject.h"
#include "chMath.h"
#include "chMatrix4.h"
#include "chScene.h"
#include "chSceneManager.h"
#include "chSpriteDraw.h"

namespace chEngineSDK {
namespace {
constexpr uint8 kMonospaceDigitsBit = 0x1u;
constexpr uint8 kRichTextBit = 0x2u;
constexpr uint8 kTrimZerosBit = 0x4u;

struct TextFileData
{
  UUID font;
  float size;
  float color[4];
  float wrapWidth;
  float lineSpacing;
  float letterSpacing;
  float pivot[2];
  float outlineWidth;
  float outlineColor[4];
  float shadowColor[4];
  float shadowOffset[2];
  int16 sortingLayer;
  int16 order;
  uint8 align;
  uint8 blendMode;
  uint8 flags;
  // The number format.
  uint8 notation;
  int8 decimals;
  int8 plainDecimals;
  uint8 padding[2];
  double plainBelow;
};
static_assert(sizeof(TextFileData) == 120, "TextFileData must have no padding");

NODISCARD SPtr<FontAsset>
findFont(const UUID& id)
{
  if (id.isNull() || !AssetManager::isStarted()) {
    return nullptr;
  }
  const SPtr<IAsset> asset = AssetManager::instance().getAsset(id);
  return asset ? asset->as<FontAsset>() : nullptr;
}

NODISCARD LinearColor
toColor(const float (&values)[4])
{
  return LinearColor(values[0], values[1], values[2], values[3]);
}
} // namespace

/*
 */
TextComponent::TextComponent()
{
  setUpdateFlags(ComponentUpdate::Frame);
  setUpdatesWhileEditing(true);
}

/*
 */
TextComponent::TextComponent(StringView text, const SPtr<FontAsset>& font)
  : TextComponent()
{
  m_text = String(text);
  m_font = font;
  m_fontId = font ? font->getUUID() : UUID::null();
}

/*
 */
void
TextComponent::setText(StringView text)
{
  if (text != m_text) {
    m_text.assign(text.data(), text.size());
    m_bLayoutDirty = true;
  }
}

/*
 */
void
TextComponent::setNumber(double value)
{
  ANSICHAR buffer[NumberFormat::MAX_CHARS];
  setText(NumberFormat::format(buffer, value, m_numberFormat));
}

/*
 */
void
TextComponent::setNumberFormat(const NumberFormatDesc& format)
{
  m_numberFormat = format;
}

/*
 */
void
TextComponent::setFont(const SPtr<FontAsset>& font)
{
  m_font = font;
  m_fontId = font ? font->getUUID() : UUID::null();
  m_bLayoutDirty = true;
}

/*
 */
void
TextComponent::setFontId(const UUID& fontId)
{
  SPtr<FontAsset> font = findFont(fontId);
  if (font && !font->isLoaded() && !AssetManager::instance().syncLoadAsset(font)) {
    font.reset();
  }
  m_font = std::move(font);
  m_fontId = fontId;
  m_bLayoutDirty = true;
}

/*
 */
void
TextComponent::setSize(float size)
{
  m_size = size;
  m_bLayoutDirty = true;
}

/*
 */
void
TextComponent::setColor(const LinearColor& color)
{
  // The layout keeps a color per glyph, for the tags.
  m_color = color;
  m_bLayoutDirty = true;
}

/*
 */
void
TextComponent::setAlign(TextAlign align)
{
  m_align = align;
  m_bLayoutDirty = true;
}

/*
 */
void
TextComponent::setWrapWidth(float width)
{
  m_wrapWidth = Math::max(width, 0.0f);
  m_bLayoutDirty = true;
}

/*
 */
void
TextComponent::setLineSpacing(float spacing)
{
  m_lineSpacing = spacing;
  m_bLayoutDirty = true;
}

/*
 */
void
TextComponent::setLetterSpacing(float spacing)
{
  m_letterSpacing = spacing;
  m_bLayoutDirty = true;
}

/*
 */
void
TextComponent::setPivot(const Vector2& pivot)
{
  m_pivot = pivot;
  m_bLayoutDirty = true;
}

/*
 */
void
TextComponent::setMonospaceDigits(bool bMonospace)
{
  m_bMonospaceDigits = bMonospace;
  m_bLayoutDirty = true;
}

/*
 */
void
TextComponent::setRichText(bool bRichText)
{
  m_bRichText = bRichText;
  m_bLayoutDirty = true;
}

/*
 */
void
TextComponent::setSorting(int16 sortingLayer, int16 order)
{
  m_sortingLayer = sortingLayer;
  m_order = order;
  m_bLookDirty = true;
}

/*
 */
void
TextComponent::setSortingLayer(int16 sortingLayer)
{
  setSorting(sortingLayer, m_order);
}

/*
 */
void
TextComponent::setOrder(int16 order)
{
  setSorting(m_sortingLayer, order);
}

/*
 */
void
TextComponent::setBlendMode(SpriteBlendMode blendMode)
{
  m_blendMode = blendMode;
  m_bLookDirty = true;
}

/*
 */
void
TextComponent::setOutline(float width, const LinearColor& color)
{
  m_outlineWidth = Math::max(width, 0.0f);
  m_outlineColor = color;
  m_bLookDirty = true;
}

/*
 */
void
TextComponent::setOutlineWidth(float width)
{
  setOutline(width, m_outlineColor);
}

/*
 */
void
TextComponent::setOutlineColor(const LinearColor& color)
{
  setOutline(m_outlineWidth, color);
}

/*
 */
void
TextComponent::setShadowColor(const LinearColor& color)
{
  setShadow(color, m_shadowOffset);
}

/*
 */
void
TextComponent::setShadowOffset(const Vector2& offset)
{
  setShadow(m_shadowColor, offset);
}

/*
 */
void
TextComponent::setShadow(const LinearColor& color, const Vector2& offset)
{
  m_shadowColor = color;
  m_shadowOffset = offset;
  m_bLookDirty = true;
}

/*
 */
void
TextComponent::update(float deltaTime)
{
  CH_PARAMETER_UNUSED(deltaTime);
  if (!isRegistered()) {
    return;
  }
  const Font* font = getShownFont();
  const bool bFontChanged = font != m_shownFont;
  if (m_bLayoutDirty || bFontChanged || m_bLookDirty ||
      (font && font->getRevision() != m_shownRevision)) {
    rebuild(m_bLayoutDirty || bFontChanged);
  }
}

/*
 */
bool
TextComponent::getWorldBounds(AABox& outBounds) const
{
  if (!isRegistered()) {
    return false;
  }
  const TextItem& text = getOwner()->getScene()->getText(m_textId);
  bool bHasBounds = false;
  const auto add = [&outBounds, &bHasBounds](const Vector3& minimum, const Vector3& maximum) {
    if (bHasBounds) {
      outBounds += AABox(minimum, maximum);
    }
    else {
      outBounds = AABox(minimum, maximum);
      bHasBounds = true;
    }
  };
  for (uint32 i = 0; i < text.runCount; ++i) {
    const Vector3 extent(text.runs[i].radius, text.runs[i].radius, text.runs[i].radius);
    add(text.runs[i].run.position - extent, text.runs[i].run.position + extent);
  }
  for (const SpriteItem& sprite : text.sprites) {
    const Vector3 extent =
        sprite.instance.halfAxisU.getAbs() + sprite.instance.halfAxisV.getAbs();
    add(sprite.instance.center - extent, sprite.instance.center + extent);
  }
  return bHasBounds;
}

/*
 */
void
TextComponent::serialize(DataStream& stream) const
{
  const uint32 version = SERIALIZATION_VERSION;
  const TextFileData data{
      .font = m_fontId,
      .size = m_size,
      .color = {m_color.r, m_color.g, m_color.b, m_color.a},
      .wrapWidth = m_wrapWidth,
      .lineSpacing = m_lineSpacing,
      .letterSpacing = m_letterSpacing,
      .pivot = {m_pivot.x, m_pivot.y},
      .outlineWidth = m_outlineWidth,
      .outlineColor = {m_outlineColor.r, m_outlineColor.g, m_outlineColor.b, m_outlineColor.a},
      .shadowColor = {m_shadowColor.r, m_shadowColor.g, m_shadowColor.b, m_shadowColor.a},
      .shadowOffset = {m_shadowOffset.x, m_shadowOffset.y},
      .sortingLayer = m_sortingLayer,
      .order = m_order,
      .align = static_cast<uint8>(m_align),
      .blendMode = static_cast<uint8>(m_blendMode),
      .flags = static_cast<uint8>((m_bMonospaceDigits ? kMonospaceDigitsBit : 0u) |
                                  (m_bRichText ? kRichTextBit : 0u) |
                                  (m_numberFormat.bTrimZeros ? kTrimZerosBit : 0u)),
      .notation = static_cast<uint8>(m_numberFormat.notation),
      .decimals = static_cast<int8>(
          Math::clamp(m_numberFormat.decimals, 0, NumberFormat::MAX_DECIMALS)),
      .plainDecimals = static_cast<int8>(
          Math::clamp(m_numberFormat.plainDecimals, 0, NumberFormat::MAX_DECIMALS)),
      .padding = {0, 0},
      .plainBelow = m_numberFormat.plainBelow};
  stream.write(&version, sizeof(version));
  stream.write(&data, sizeof(data));
  AssetFile::writeString(stream,
                         StringView(m_text).substr(0, static_cast<SIZE_T>(MAX_TEXT_LENGTH)));
}

/*
 */
bool
TextComponent::deserialize(DataStream& stream)
{
  uint32 version = 0;
  TextFileData data;
  String text;
  if (!AssetFile::readValue(stream, version) || version != SERIALIZATION_VERSION ||
      !AssetFile::readValue(stream, data) ||
      data.align >= static_cast<uint8>(TextAlign::COUNT) ||
      data.blendMode >= static_cast<uint8>(SpriteBlendMode::COUNT) ||
      data.notation >= static_cast<uint8>(NumberNotation::COUNT) ||
      !Math::isFinite(data.size) || !Math::isFinite(data.wrapWidth) ||
      !AssetFile::readString(stream, text, MAX_TEXT_LENGTH)) {
    return false;
  }

  m_text = std::move(text);
  m_size = data.size;
  m_color = toColor(data.color);
  m_wrapWidth = Math::max(data.wrapWidth, 0.0f);
  m_lineSpacing = data.lineSpacing;
  m_letterSpacing = data.letterSpacing;
  m_pivot = Vector2(data.pivot[0], data.pivot[1]);
  m_outlineWidth = Math::max(data.outlineWidth, 0.0f);
  m_outlineColor = toColor(data.outlineColor);
  m_shadowColor = toColor(data.shadowColor);
  m_shadowOffset = Vector2(data.shadowOffset[0], data.shadowOffset[1]);
  m_sortingLayer = data.sortingLayer;
  m_order = data.order;
  m_align = static_cast<TextAlign>(data.align);
  m_blendMode = static_cast<SpriteBlendMode>(data.blendMode);
  m_bMonospaceDigits = (data.flags & kMonospaceDigitsBit) != 0;
  m_bRichText = (data.flags & kRichTextBit) != 0;
  m_numberFormat = {.notation = static_cast<NumberNotation>(data.notation),
                    .decimals = data.decimals,
                    .bTrimZeros = (data.flags & kTrimZerosBit) != 0,
                    .plainBelow = data.plainBelow,
                    .plainDecimals = data.plainDecimals};

  m_font = findFont(data.font);
  m_fontId = data.font;
  if (!m_font && !data.font.isNull()) {
    CH_LOG_WARNING(SceneManagerLog, "A text lost its font {0}; it uses the default one.",
                   data.font);
  }
  m_bLayoutDirty = true;
  return true;
}

/*
 */
void
TextComponent::collectReferences(Vector<UUID>& outReferences) const
{
  if (!m_fontId.isNull()) {
    outReferences.push_back(m_fontId);
  }
}

/*
 */
void
TextComponent::onRegister()
{
  m_textId = getOwner()->getScene()->addText(getOwner());
  rebuild(true);
}

/*
 */
void
TextComponent::onUnregister()
{
  getOwner()->getScene()->removeText(m_textId);
  m_shownFont = nullptr;
}

/*
 */
void
TextComponent::onTransformChanged()
{
  rebuild(m_bLayoutDirty || getShownFont() != m_shownFont);
}

/*
 */
Font*
TextComponent::getShownFont() const
{
  if (m_font && m_font->getFont()) {
    return m_font->getFont().get();
  }
  return Font::getDefault().get();
}

/*
 */
void
TextComponent::rebuild(bool bLayout)
{
  if (!isRegistered()) {
    return;
  }
  TextItem& item = getOwner()->getScene()->getText(m_textId);
  item.sprites.clear();
  item.runCount = 0;
  m_bLayoutDirty = false;
  m_bLookDirty = false;

  Font* font = getShownFont();
  m_shownFont = font;
  if (!font || m_text.empty()) {
    m_textSize = Vector2::ZERO;
    m_layout.clear();
    return;
  }

  // Rows of the world matrix are the object's axes in world space, scale included, and
  // the last row is its position.
  const Matrix4& world = getOwner()->getTransform().getWorldMatrix();
  const Vector3 right(world.at(1, 0), world.at(1, 1), world.at(1, 2));
  const Vector3 up(world.at(2, 0), world.at(2, 1), world.at(2, 2));
  const Vector2 position(world.at(3, 1), world.at(3, 2));
  const float depth = world.at(3, 0);
  const float scale = up.magnitude();
  const Radian rotation(Math::atan2(right.z, right.y));

  if (bLayout) {
    m_bGlyphRuns = SpriteDraw::fitsGlyphRun(m_text, m_bRichText);
  }
  if (m_bGlyphRuns) {
    const GlyphRunDesc desc{.position = position,
                            .depth = depth,
                            .rotation = rotation,
                            .size = m_size * scale,
                            .pivot = m_pivot,
                            .color = m_color,
                            .bMonospaceDigits = m_bMonospaceDigits,
                            .letterSpacing = m_letterSpacing * scale,
                            .sortingLayer = m_sortingLayer,
                            .order = m_order,
                            .blendMode = m_blendMode,
                            .outlineWidth = m_outlineWidth,
                            .outlineColor = m_outlineColor,
                            .shadowColor = m_shadowColor,
                            .shadowOffset = m_shadowOffset};
    item.runCount = SpriteDraw::makeGlyphRuns(*font, m_text, desc, item.runs);
    const float width = item.runCount > 0 ? item.runs[item.runCount - 1].width : 0.0f;
    // A run is one line, so one wider than the wrap width needs the layout.
    if (m_wrapWidth > 0.0f && width > m_wrapWidth * scale) {
      item.runCount = 0;
      m_bGlyphRuns = false;
      bLayout = true;
    }
    else {
      m_textSize = Vector2(scale > 0.0f ? width / scale : 0.0f, m_size);
      m_layout.clear();
    }
  }
  if (!m_bGlyphRuns) {
    if (bLayout || m_layout.getGlyphs().empty()) {
      m_layout.build(*font,
                     m_text,
                     {.size = m_size,
                      .wrapWidth = m_wrapWidth,
                      .align = m_align,
                      .lineSpacing = m_lineSpacing,
                      .letterSpacing = m_letterSpacing,
                      .pivot = m_pivot,
                      .bMonospaceDigits = m_bMonospaceDigits,
                      .bRichText = m_bRichText,
                      .color = m_color});
    }
    m_textSize = m_layout.getSize();
    SpriteDraw::appendTextSprites(*font,
                                  m_layout,
                                  {.position = position,
                                   .depth = depth,
                                   .rotation = rotation,
                                   .scale = scale,
                                   .sortingLayer = m_sortingLayer,
                                   .order = m_order,
                                   .blendMode = m_blendMode,
                                   .outlineWidth = m_outlineWidth,
                                   .outlineColor = m_outlineColor,
                                   .shadowColor = m_shadowColor,
                                   .shadowOffset = m_shadowOffset},
                                  item.sprites);
  }
  // Read after building, which may give the atlas its first texture.
  m_shownRevision = font->getRevision();
}

} // namespace chEngineSDK
