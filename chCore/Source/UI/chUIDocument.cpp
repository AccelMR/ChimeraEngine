/************************************************************************/
/**
 * @file chUIDocument.cpp
 * @author Swampertor
 * @date 2026/11/26
 * @brief A widget tree saved as a document, with the registry that rebuilds it.
 */
/************************************************************************/
#include "chUIDocument.h"

#include "chAssetFile.h"
#include "chAssetManager.h"
#include "chFileStream.h"
#include "chFontAsset.h"
#include "chTextureAsset.h"
#include "chTypeTraits.h"

#include "chUIBorder.h"
#include "chUIBoxSlot.h"
#include "chUIButton.h"
#include "chUICanvasPanel.h"
#include "chUICanvasPanelSlot.h"
#include "chUICheckBox.h"
#include "chUIContentWidget.h"
#include "chUIEditableTextBox.h"
#include "chUIHorizontalBox.h"
#include "chUIImage.h"
#include "chUIOverlay.h"
#include "chUIOverlaySlot.h"
#include "chUIPanelWidget.h"
#include "chUIScrollBox.h"
#include "chUIScrollBoxSlot.h"
#include "chUISizeBox.h"
#include "chUISlider.h"
#include "chUITextBlock.h"
#include "chUIVerticalBox.h"

namespace chEngineSDK {

namespace {

// Above any real document; only a broken file asks for more.
constexpr uint32 kMaxWidgetCount = 65536;
constexpr uint32 kMaxDepth = 256;
constexpr uint32 kMaxTypeNameLength = 128;
constexpr uint32 kMaxNameLength = 256;
constexpr uint32 kMaxTextLength = 1024 * 1024;

template <typename T>
void
writeValue(DataStream& stream, const T& value)
{
  stream.write(&value, sizeof(T));
}

template <typename T>
NODISCARD bool
readValue(DataStream& stream, T& outValue)
{
  return AssetFile::readValue(stream, outValue);
}

void
writeBool(DataStream& stream, bool value)
{
  writeValue<uint8>(stream, value ? 1 : 0);
}

NODISCARD bool
readBool(DataStream& stream, bool& outValue)
{
  uint8 value = 0;
  if (!readValue(stream, value) || value > 1) {
    return false;
  }
  outValue = value != 0;
  return true;
}

template <typename TEnum>
void
writeEnum(DataStream& stream, TEnum value)
{
  writeValue<uint8>(stream, static_cast<uint8>(value));
}

template <typename TEnum>
NODISCARD bool
readEnum(DataStream& stream, TEnum& outValue)
{
  uint8 value = 0;
  if (!readValue(stream, value) || value >= static_cast<uint8>(TEnum::COUNT)) {
    return false;
  }
  outValue = static_cast<TEnum>(value);
  return true;
}

void
writeUUID(DataStream& stream, const UUID& uuid)
{
  stream.write(&uuid, sizeof(UUID));
}

NODISCARD bool
readUUID(DataStream& stream, UUID& outUuid)
{
  return AssetFile::readValue(stream, outUuid);
}

void
writeVector2(DataStream& stream, const Vector2& vector)
{
  writeValue(stream, vector.x);
  writeValue(stream, vector.y);
}

NODISCARD bool
readVector2(DataStream& stream, Vector2& outVector)
{
  return readValue(stream, outVector.x) && readValue(stream, outVector.y);
}

void
writeMargin(DataStream& stream, const UIMargin& margin)
{
  writeValue(stream, margin.left);
  writeValue(stream, margin.top);
  writeValue(stream, margin.right);
  writeValue(stream, margin.bottom);
}

NODISCARD bool
readMargin(DataStream& stream, UIMargin& outMargin)
{
  return readValue(stream, outMargin.left) && readValue(stream, outMargin.top)
         && readValue(stream, outMargin.right) && readValue(stream, outMargin.bottom);
}

void
writeColor(DataStream& stream, const LinearColor& color)
{
  stream.write(color.getData(), 4 * sizeof(float));
}

NODISCARD bool
readColor(DataStream& stream, LinearColor& outColor)
{
  float values[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  if (!AssetFile::readBytes(stream, values, sizeof(values))) {
    return false;
  }
  outColor = LinearColor(values[0], values[1], values[2], values[3]);
  return true;
}

void
writeTransform(DataStream& stream, const UIRenderTransform& transform)
{
  writeVector2(stream, transform.translation);
  writeValue(stream, transform.angle);
  writeVector2(stream, transform.scale);
  writeVector2(stream, transform.pivot);
}

NODISCARD bool
readTransform(DataStream& stream, UIRenderTransform& outTransform)
{
  return readVector2(stream, outTransform.translation) && readValue(stream, outTransform.angle)
         && readVector2(stream, outTransform.scale) && readVector2(stream, outTransform.pivot);
}

void
addUniqueReference(Vector<UUID>& references, const UUID& reference)
{
  if (reference.isNull()) {
    return;
  }
  for (const UUID& existing : references) {
    if (existing == reference) {
      return;
    }
  }
  references.push_back(reference);
}

NODISCARD const TextureAsset*
resolveTexture(const UUID& textureId)
{
  if (textureId.isNull() || !AssetManager::isStarted()) {
    return nullptr;
  }

  const SPtr<IAsset> asset = AssetManager::instance().getAsset(textureId);
  const SPtr<TextureAsset> texture = asset ? asset->as<TextureAsset>() : nullptr;
  if (!texture) {
    return nullptr;
  }
  if (!texture->isLoaded()) {
    AssetManager::instance().syncLoadAsset(texture);
  }
  return texture.get();
}

// -- Widget properties ------------------------------------------------------

void
writeTextBlock(const UIWidget& widget, DataStream& stream)
{
  const UITextBlock& text = static_cast<const UITextBlock&>(widget);
  AssetFile::writeString(stream, text.getText());
  writeUUID(stream, text.getFontId());
  writeValue(stream, text.getFontSize());
  writeEnum(stream, text.getAlign());
  writeBool(stream, text.getWrapText());
  writeMargin(stream, text.getMargin());
  writeValue(stream, text.getLineSpacing());
  writeValue(stream, text.getLetterSpacing());
  writeBool(stream, text.getMonospaceDigits());
  writeBool(stream, text.getRichText());
}

NODISCARD bool
readTextBlock(UIWidget& widget, DataStream& stream)
{
  UITextBlock& text = static_cast<UITextBlock&>(widget);

  String value;
  if (!AssetFile::readString(stream, value, kMaxTextLength)) {
    return false;
  }
  text.setText(value);

  UUID fontId;
  float fontSize = 0.0f;
  TextAlign align = TextAlign::Left;
  bool wrap = false;
  UIMargin margin;
  float lineSpacing = 0.0f;
  float letterSpacing = 0.0f;
  bool monospace = false;
  bool richText = false;
  if (!readUUID(stream, fontId) || !readValue(stream, fontSize) || !readEnum(stream, align)
      || !readBool(stream, wrap) || !readMargin(stream, margin)
      || !readValue(stream, lineSpacing) || !readValue(stream, letterSpacing)
      || !readBool(stream, monospace) || !readBool(stream, richText)) {
    return false;
  }

  text.setFontId(fontId);
  text.setFontSize(fontSize);
  text.setAlign(align);
  text.setWrapText(wrap);
  text.setMargin(margin);
  text.setLineSpacing(lineSpacing);
  text.setLetterSpacing(letterSpacing);
  text.setMonospaceDigits(monospace);
  text.setRichText(richText);
  return true;
}

void
collectTextReferences(const UIWidget& widget, Vector<UUID>& outReferences)
{
  addUniqueReference(outReferences, static_cast<const UITextBlock&>(widget).getFontId());
}

void
writeImage(const UIWidget& widget, DataStream& stream)
{
  const UIImage& image = static_cast<const UIImage&>(widget);
  const TextureAsset* texture = image.getTexture();
  writeUUID(stream, texture ? texture->getUUID() : UUID::null());
  writeEnum(stream, image.getFilter());
  writeVector2(stream, image.getSourcePosition());
  writeVector2(stream, image.getSourceSize());
  writeBool(stream, image.getFlipX());
  writeBool(stream, image.getFlipY());
  writeEnum(stream, image.getBlendMode());
}

NODISCARD bool
readImage(UIWidget& widget, DataStream& stream)
{
  UIImage& image = static_cast<UIImage&>(widget);

  UUID textureId;
  SpriteFilter filter = SpriteFilter::Linear;
  Vector2 sourcePosition;
  Vector2 sourceSize;
  bool flipX = false;
  bool flipY = false;
  SpriteBlendMode blendMode = SpriteBlendMode::Alpha;
  if (!readUUID(stream, textureId) || !readEnum(stream, filter)
      || !readVector2(stream, sourcePosition) || !readVector2(stream, sourceSize)
      || !readBool(stream, flipX) || !readBool(stream, flipY)
      || !readEnum(stream, blendMode)) {
    return false;
  }

  image.setTexture(resolveTexture(textureId));
  image.setFilter(filter);
  image.setSourcePosition(sourcePosition);
  image.setSourceSize(sourceSize);
  image.setFlipX(flipX);
  image.setFlipY(flipY);
  image.setBlendMode(blendMode);
  return true;
}

void
collectImageReferences(const UIWidget& widget, Vector<UUID>& outReferences)
{
  const TextureAsset* texture = static_cast<const UIImage&>(widget).getTexture();
  if (texture) {
    addUniqueReference(outReferences, texture->getUUID());
  }
}

void
writeButton(const UIWidget& widget, DataStream& stream)
{
  const UIButton& button = static_cast<const UIButton&>(widget);
  writeColor(stream, button.getNormalColor());
  writeColor(stream, button.getHoveredColor());
  writeColor(stream, button.getPressedColor());
  writeColor(stream, button.getDisabledColor());
}

NODISCARD bool
readButton(UIWidget& widget, DataStream& stream)
{
  UIButton& button = static_cast<UIButton&>(widget);
  LinearColor normal;
  LinearColor hovered;
  LinearColor pressed;
  LinearColor disabled;
  if (!readColor(stream, normal) || !readColor(stream, hovered) || !readColor(stream, pressed)
      || !readColor(stream, disabled)) {
    return false;
  }
  button.setNormalColor(normal);
  button.setHoveredColor(hovered);
  button.setPressedColor(pressed);
  button.setDisabledColor(disabled);
  return true;
}

void
writeBorder(const UIWidget& widget, DataStream& stream)
{
  const UIBorder& border = static_cast<const UIBorder&>(widget);
  writeColor(stream, border.getBrushColor());
  writeMargin(stream, border.getPadding());
}

NODISCARD bool
readBorder(UIWidget& widget, DataStream& stream)
{
  UIBorder& border = static_cast<UIBorder&>(widget);
  LinearColor brush;
  UIMargin padding;
  if (!readColor(stream, brush) || !readMargin(stream, padding)) {
    return false;
  }
  border.setBrushColor(brush);
  border.setPadding(padding);
  return true;
}

void
writeSizeBox(const UIWidget& widget, DataStream& stream)
{
  const UISizeBox& sizeBox = static_cast<const UISizeBox&>(widget);
  writeValue(stream, sizeBox.getWidthOverride());
  writeValue(stream, sizeBox.getHeightOverride());
}

NODISCARD bool
readSizeBox(UIWidget& widget, DataStream& stream)
{
  UISizeBox& sizeBox = static_cast<UISizeBox&>(widget);
  float width = 0.0f;
  float height = 0.0f;
  if (!readValue(stream, width) || !readValue(stream, height)) {
    return false;
  }
  sizeBox.setWidthOverride(width);
  sizeBox.setHeightOverride(height);
  return true;
}

void
writeCheckBox(const UIWidget& widget, DataStream& stream)
{
  const UICheckBox& checkBox = static_cast<const UICheckBox&>(widget);
  writeBool(stream, checkBox.isChecked());
  writeColor(stream, checkBox.getNormalColor());
  writeColor(stream, checkBox.getHoveredColor());
  writeColor(stream, checkBox.getPressedColor());
  writeColor(stream, checkBox.getDisabledColor());
  writeColor(stream, checkBox.getCheckMarkColor());
  writeValue(stream, checkBox.getCheckBoxSize());
  writeValue(stream, checkBox.getSpacing());
}

NODISCARD bool
readCheckBox(UIWidget& widget, DataStream& stream)
{
  UICheckBox& checkBox = static_cast<UICheckBox&>(widget);
  bool checked = false;
  LinearColor normal;
  LinearColor hovered;
  LinearColor pressed;
  LinearColor disabled;
  LinearColor checkMark;
  float size = 0.0f;
  float spacing = 0.0f;
  if (!readBool(stream, checked) || !readColor(stream, normal) || !readColor(stream, hovered)
      || !readColor(stream, pressed) || !readColor(stream, disabled)
      || !readColor(stream, checkMark) || !readValue(stream, size)
      || !readValue(stream, spacing)) {
    return false;
  }
  checkBox.setIsChecked(checked, false);
  checkBox.setNormalColor(normal);
  checkBox.setHoveredColor(hovered);
  checkBox.setPressedColor(pressed);
  checkBox.setDisabledColor(disabled);
  checkBox.setCheckMarkColor(checkMark);
  checkBox.setCheckBoxSize(size);
  checkBox.setSpacing(spacing);
  return true;
}

void
writeSlider(const UIWidget& widget, DataStream& stream)
{
  const UISlider& slider = static_cast<const UISlider&>(widget);
  writeValue(stream, slider.getValue());
  writeValue(stream, slider.getMinValue());
  writeValue(stream, slider.getMaxValue());
  writeValue(stream, slider.getStep());
  writeEnum(stream, slider.getOrientation());
  writeColor(stream, slider.getTrackColor());
  writeColor(stream, slider.getFillColor());
  writeColor(stream, slider.getThumbColor());
  writeColor(stream, slider.getDisabledColor());
}

NODISCARD bool
readSlider(UIWidget& widget, DataStream& stream)
{
  UISlider& slider = static_cast<UISlider&>(widget);
  float value = 0.0f;
  float minValue = 0.0f;
  float maxValue = 0.0f;
  float step = 0.0f;
  UIOrientation orientation = UIOrientation::Horizontal;
  LinearColor track;
  LinearColor fill;
  LinearColor thumb;
  LinearColor disabled;
  if (!readValue(stream, value) || !readValue(stream, minValue) || !readValue(stream, maxValue)
      || !readValue(stream, step) || !readEnum(stream, orientation)
      || !readColor(stream, track) || !readColor(stream, fill) || !readColor(stream, thumb)
      || !readColor(stream, disabled)) {
    return false;
  }
  slider.setMinValue(minValue);
  slider.setMaxValue(maxValue);
  slider.setStep(step);
  slider.setOrientation(orientation);
  slider.setValue(value, false);
  slider.setTrackColor(track);
  slider.setFillColor(fill);
  slider.setThumbColor(thumb);
  slider.setDisabledColor(disabled);
  return true;
}

void
writeEditableTextBox(const UIWidget& widget, DataStream& stream)
{
  const UIEditableTextBox& text = static_cast<const UIEditableTextBox&>(widget);
  AssetFile::writeString(stream, text.getText());
  AssetFile::writeString(stream, text.getHintText());
  writeUUID(stream, text.getFontId());
  writeValue(stream, text.getFontSize());
  writeMargin(stream, text.getPadding());
  writeValue(stream, text.getMaxLength());
  writeBool(stream, text.isReadOnly());
  writeBool(stream, text.isPassword());
  writeColor(stream, text.getTextColor());
  writeColor(stream, text.getHintColor());
  writeColor(stream, text.getBackgroundColor());
  writeColor(stream, text.getCaretColor());
}

NODISCARD bool
readEditableTextBox(UIWidget& widget, DataStream& stream)
{
  UIEditableTextBox& text = static_cast<UIEditableTextBox&>(widget);

  String value;
  String hint;
  UUID fontId;
  float fontSize = 0.0f;
  UIMargin padding;
  int32 maxLength = 0;
  bool readOnly = false;
  bool password = false;
  LinearColor textColor;
  LinearColor hintColor;
  LinearColor backgroundColor;
  LinearColor caretColor;
  if (!AssetFile::readString(stream, value, kMaxTextLength)
      || !AssetFile::readString(stream, hint, kMaxTextLength) || !readUUID(stream, fontId)
      || !readValue(stream, fontSize) || !readMargin(stream, padding)
      || !readValue(stream, maxLength) || !readBool(stream, readOnly)
      || !readBool(stream, password) || !readColor(stream, textColor)
      || !readColor(stream, hintColor) || !readColor(stream, backgroundColor)
      || !readColor(stream, caretColor)) {
    return false;
  }

  text.setText(value);
  text.setHintText(hint);
  text.setFontId(fontId);
  text.setFontSize(fontSize);
  text.setPadding(padding);
  text.setMaxLength(maxLength);
  text.setReadOnly(readOnly);
  text.setPassword(password);
  text.setTextColor(textColor);
  text.setHintColor(hintColor);
  text.setBackgroundColor(backgroundColor);
  text.setCaretColor(caretColor);
  return true;
}

void
collectEditableTextBoxReferences(const UIWidget& widget, Vector<UUID>& outReferences)
{
  addUniqueReference(outReferences, static_cast<const UIEditableTextBox&>(widget).getFontId());
}

void
writeScrollBox(const UIWidget& widget, DataStream& stream)
{
  writeValue(stream, static_cast<const UIScrollBox&>(widget).getScrollStep());
}

NODISCARD bool
readScrollBox(UIWidget& widget, DataStream& stream)
{
  float step = 0.0f;
  if (!readValue(stream, step)) {
    return false;
  }
  static_cast<UIScrollBox&>(widget).setScrollStep(step);
  return true;
}

// -- Slot properties --------------------------------------------------------

void
writeCanvasPanelSlot(const UIPanelSlot& slot, DataStream& stream)
{
  const UICanvasPanelSlot& canvas = static_cast<const UICanvasPanelSlot&>(slot);
  writeVector2(stream, canvas.getAnchorMin());
  writeVector2(stream, canvas.getAnchorMax());
  writeVector2(stream, canvas.getAlignment());
  writeVector2(stream, canvas.getPosition());
  writeVector2(stream, canvas.getSize());
}

NODISCARD bool
readCanvasPanelSlot(UIPanelSlot& slot, DataStream& stream)
{
  UICanvasPanelSlot& canvas = static_cast<UICanvasPanelSlot&>(slot);
  Vector2 anchorMin;
  Vector2 anchorMax;
  Vector2 alignment;
  Vector2 position;
  Vector2 size;
  if (!readVector2(stream, anchorMin) || !readVector2(stream, anchorMax)
      || !readVector2(stream, alignment) || !readVector2(stream, position)
      || !readVector2(stream, size)) {
    return false;
  }
  canvas.setAnchors(anchorMin, anchorMax);
  canvas.setAlignment(alignment);
  canvas.setPosition(position);
  canvas.setSize(size);
  return true;
}

void
writeBoxSlot(const UIPanelSlot& slot, DataStream& stream)
{
  const UIBoxSlot& box = static_cast<const UIBoxSlot&>(slot);
  writeEnum(stream, box.getOrientation());
  writeMargin(stream, box.getPadding());
  writeEnum(stream, box.getSizeRule());
  writeValue(stream, box.getFillWeight());
  writeEnum(stream, box.getHorizontalAlignment());
  writeEnum(stream, box.getVerticalAlignment());
}

NODISCARD bool
readBoxSlot(UIPanelSlot& slot, DataStream& stream)
{
  UIBoxSlot& box = static_cast<UIBoxSlot&>(slot);
  UIOrientation orientation = UIOrientation::Vertical;
  UIMargin padding;
  UISizeRule sizeRule = UISizeRule::Auto;
  float fillWeight = 0.0f;
  UIHorizontalAlignment horizontal = UIHorizontalAlignment::Fill;
  UIVerticalAlignment vertical = UIVerticalAlignment::Fill;
  if (!readEnum(stream, orientation) || !readMargin(stream, padding)
      || !readEnum(stream, sizeRule) || !readValue(stream, fillWeight)
      || !readEnum(stream, horizontal) || !readEnum(stream, vertical)) {
    return false;
  }
  box.setOrientation(orientation);
  box.setPadding(padding);
  box.setSizeRule(sizeRule);
  box.setFillWeight(fillWeight);
  box.setHorizontalAlignment(horizontal);
  box.setVerticalAlignment(vertical);
  return true;
}

void
writeOverlaySlot(const UIPanelSlot& slot, DataStream& stream)
{
  const UIOverlaySlot& overlay = static_cast<const UIOverlaySlot&>(slot);
  writeMargin(stream, overlay.getPadding());
  writeEnum(stream, overlay.getHorizontalAlignment());
  writeEnum(stream, overlay.getVerticalAlignment());
}

NODISCARD bool
readOverlaySlot(UIPanelSlot& slot, DataStream& stream)
{
  UIOverlaySlot& overlay = static_cast<UIOverlaySlot&>(slot);
  UIMargin padding;
  UIHorizontalAlignment horizontal = UIHorizontalAlignment::Fill;
  UIVerticalAlignment vertical = UIVerticalAlignment::Fill;
  if (!readMargin(stream, padding) || !readEnum(stream, horizontal)
      || !readEnum(stream, vertical)) {
    return false;
  }
  overlay.setPadding(padding);
  overlay.setHorizontalAlignment(horizontal);
  overlay.setVerticalAlignment(vertical);
  return true;
}

// -- Registry ---------------------------------------------------------------

template <typename TWidget>
NODISCARD UIWidgetTypeDesc
makeWidgetDesc(const ANSICHAR* typeName, UIWidgetTypeDesc::WriteFunc writeProperties,
               UIWidgetTypeDesc::ReadFunc readProperties,
               UIWidgetTypeDesc::ReferencesFunc collectReferences)
{
  UIWidgetTypeDesc desc;
  desc.typeName = typeName;
  desc.create = []() -> UniquePtr<UIWidget> { return chMakeUnique<TWidget>(); };
  desc.writeProperties = std::move(writeProperties);
  desc.readProperties = std::move(readProperties);
  desc.collectReferences = std::move(collectReferences);

  if constexpr (std::is_base_of_v<UIPanelWidget, TWidget>) {
    desc.attachChild = [](UIWidget& parent, UniquePtr<UIWidget> child,
                          UniquePtr<UIPanelSlot> slot) -> UIWidget* {
      return static_cast<UIPanelWidget&>(parent).addChild(std::move(child), std::move(slot));
    };
    desc.maxChildren = kMaxWidgetCount;
  }
  else if constexpr (std::is_base_of_v<UIContentWidget, TWidget>) {
    desc.attachChild = [](UIWidget& parent, UniquePtr<UIWidget> child,
                          UniquePtr<UIPanelSlot> slot) -> UIWidget* {
      return static_cast<UIContentWidget&>(parent).setContent(std::move(child),
                                                              std::move(slot));
    };
    desc.maxChildren = 1;
  }
  return desc;
}

template <typename TSlot>
NODISCARD UIPanelSlotTypeDesc
makeSlotDesc(const ANSICHAR* typeName, UIPanelSlotTypeDesc::WriteFunc writeProperties,
             UIPanelSlotTypeDesc::ReadFunc readProperties)
{
  UIPanelSlotTypeDesc desc;
  desc.typeName = typeName;
  desc.create = []() -> UniquePtr<UIPanelSlot> { return chMakeUnique<TSlot>(); };
  desc.writeProperties = std::move(writeProperties);
  desc.readProperties = std::move(readProperties);
  return desc;
}

void
registerBuiltinTypes(UIDocumentRegistry& registry)
{
  const UIWidgetTypeDesc::WriteFunc noWrite;
  const UIWidgetTypeDesc::ReadFunc noRead;
  const UIWidgetTypeDesc::ReferencesFunc noReferences;

  registry.registerWidget(makeWidgetDesc<UIWidget>("UIWidget", noWrite, noRead, noReferences));
  registry.registerWidget(
      makeWidgetDesc<UIPanelWidget>("UIPanelWidget", noWrite, noRead, noReferences));
  registry.registerWidget(
      makeWidgetDesc<UIContentWidget>("UIContentWidget", noWrite, noRead, noReferences));

  registry.registerWidget(
      makeWidgetDesc<UICanvasPanel>("UICanvasPanel", noWrite, noRead, noReferences));
  registry.registerWidget(
      makeWidgetDesc<UIVerticalBox>("UIVerticalBox", noWrite, noRead, noReferences));
  registry.registerWidget(
      makeWidgetDesc<UIHorizontalBox>("UIHorizontalBox", noWrite, noRead, noReferences));
  registry.registerWidget(
      makeWidgetDesc<UIOverlay>("UIOverlay", noWrite, noRead, noReferences));
  registry.registerWidget(
      makeWidgetDesc<UIScrollBox>("UIScrollBox", writeScrollBox, readScrollBox, noReferences));

  registry.registerWidget(
      makeWidgetDesc<UIBorder>("UIBorder", writeBorder, readBorder, noReferences));
  registry.registerWidget(
      makeWidgetDesc<UISizeBox>("UISizeBox", writeSizeBox, readSizeBox, noReferences));
  registry.registerWidget(
      makeWidgetDesc<UICheckBox>("UICheckBox", writeCheckBox, readCheckBox, noReferences));
  registry.registerWidget(
      makeWidgetDesc<UISlider>("UISlider", writeSlider, readSlider, noReferences));
  registry.registerWidget(makeWidgetDesc<UIEditableTextBox>(
      "UIEditableTextBox", writeEditableTextBox, readEditableTextBox,
      collectEditableTextBoxReferences));
  registry.registerWidget(makeWidgetDesc<UITextBlock>("UITextBlock", writeTextBlock,
                                                      readTextBlock, collectTextReferences));
  registry.registerWidget(
      makeWidgetDesc<UIImage>("UIImage", writeImage, readImage, collectImageReferences));
  registry.registerWidget(
      makeWidgetDesc<UIButton>("UIButton", writeButton, readButton, noReferences));

  const UIPanelSlotTypeDesc::WriteFunc noSlotWrite;
  const UIPanelSlotTypeDesc::ReadFunc noSlotRead;

  registry.registerSlot(makeSlotDesc<UIPanelSlot>("UIPanelSlot", noSlotWrite, noSlotRead));
  registry.registerSlot(makeSlotDesc<UICanvasPanelSlot>(
      "UICanvasPanelSlot", writeCanvasPanelSlot, readCanvasPanelSlot));
  registry.registerSlot(makeSlotDesc<UIBoxSlot>("UIBoxSlot", writeBoxSlot, readBoxSlot));
  registry.registerSlot(
      makeSlotDesc<UIVerticalBoxSlot>("UIVerticalBoxSlot", writeBoxSlot, readBoxSlot));
  registry.registerSlot(
      makeSlotDesc<UIHorizontalBoxSlot>("UIHorizontalBoxSlot", writeBoxSlot, readBoxSlot));
  registry.registerSlot(
      makeSlotDesc<UIScrollBoxSlot>("UIScrollBoxSlot", writeBoxSlot, readBoxSlot));
  registry.registerSlot(
      makeSlotDesc<UIOverlaySlot>("UIOverlaySlot", writeOverlaySlot, readOverlaySlot));
}

// -- Tree helpers -----------------------------------------------------------

struct PendingNavigation {
  UIWidget* widget = nullptr;
  UINavigation direction = UINavigation::Up;
  UINavigationRule rule = UINavigationRule::Escape;
  String targetName;
};

void
writeWidgetCommon(const UIWidget& widget, DataStream& stream)
{
  AssetFile::writeString(stream, widget.getName());
  writeBool(stream, widget.isEnabled());
  writeBool(stream, widget.isFocusable());
  writeEnum(stream, widget.getVisibility());
  writeColor(stream, widget.getColor());
  writeTransform(stream, widget.getRenderTransform());

  for (uint8 index = 0; index < static_cast<uint8>(UINavigation::COUNT); ++index) {
    const UINavigationLink& link = widget.getNavigation(static_cast<UINavigation>(index));
    writeEnum(stream, link.rule);
    AssetFile::writeString(stream,
                           link.target ? StringView(link.target->getName()) : StringView());
  }
}

NODISCARD bool
readWidgetCommon(UIWidget& widget, DataStream& stream, Vector<PendingNavigation>& pending)
{
  String name;
  bool enabled = true;
  bool focusable = true;
  UIVisibility visibility = UIVisibility::Visible;
  LinearColor color;
  UIRenderTransform transform;
  if (!AssetFile::readString(stream, name, kMaxNameLength) || !readBool(stream, enabled)
      || !readBool(stream, focusable) || !readEnum(stream, visibility)
      || !readColor(stream, color) || !readTransform(stream, transform)) {
    return false;
  }

  widget.setName(name);
  widget.setEnabled(enabled);
  widget.setFocusable(focusable);
  widget.setVisibility(visibility);
  widget.setColor(color);
  widget.setRenderTransform(transform);

  for (uint8 index = 0; index < static_cast<uint8>(UINavigation::COUNT); ++index) {
    const UINavigation direction = static_cast<UINavigation>(index);
    UINavigationRule rule = UINavigationRule::Escape;
    String targetName;
    if (!readEnum(stream, rule)
        || !AssetFile::readString(stream, targetName, kMaxNameLength)) {
      return false;
    }

    UINavigationLink link;
    link.rule = rule;
    if (rule == UINavigationRule::Explicit && !targetName.empty()) {
      pending.push_back({&widget, direction, rule, std::move(targetName)});
    }
    widget.setNavigation(direction, std::move(link));
  }
  return true;
}

NODISCARD bool
writeWidgetNode(const UIWidget& widget, DataStream& stream, uint32 depth, uint32& count)
{
  if (depth > kMaxDepth || ++count > kMaxWidgetCount) {
    return false;
  }

  UIDocumentRegistry& registry = UIDocumentRegistry::instance();
  const UIWidgetTypeDesc* desc = registry.findWidget(widget.getTypeName());
  if (!desc) {
    return false;
  }

  AssetFile::writeString(stream, desc->typeName);
  writeWidgetCommon(widget, stream);
  if (desc->writeProperties) {
    desc->writeProperties(widget, stream);
  }

  if (depth > 0) {
    const UIPanelSlot* slot = widget.getSlot();
    if (!slot) {
      return false;
    }
    const UIPanelSlotTypeDesc* slotDesc = registry.findSlot(slot->getTypeName());
    if (!slotDesc) {
      return false;
    }
    AssetFile::writeString(stream, slotDesc->typeName);
    if (slotDesc->writeProperties) {
      slotDesc->writeProperties(*slot, stream);
    }
  }

  const SIZE_T childCount = widget.getChildCount();
  if (childCount > desc->maxChildren) {
    return false;
  }
  writeValue<uint32>(stream, static_cast<uint32>(childCount));
  for (SIZE_T index = 0; index < childCount; ++index) {
    const UIWidget* child = widget.getChildAt(index);
    if (!child || !writeWidgetNode(*child, stream, depth + 1, count)) {
      return false;
    }
  }
  return true;
}

NODISCARD UniquePtr<UIWidget>
readWidgetNode(DataStream& stream, uint32 depth, uint32& count,
               Vector<PendingNavigation>& pending, UniquePtr<UIPanelSlot>& outSlot)
{
  if (depth > kMaxDepth || ++count > kMaxWidgetCount) {
    return nullptr;
  }

  UIDocumentRegistry& registry = UIDocumentRegistry::instance();
  String typeName;
  if (!AssetFile::readString(stream, typeName, kMaxTypeNameLength)) {
    return nullptr;
  }
  const UIWidgetTypeDesc* desc = registry.findWidget(typeName);
  if (!desc || !desc->create) {
    return nullptr;
  }

  UniquePtr<UIWidget> widget = desc->create();
  if (!widget || !readWidgetCommon(*widget, stream, pending)
      || (desc->readProperties && !desc->readProperties(*widget, stream))) {
    return nullptr;
  }

  outSlot.reset();
  if (depth > 0) {
    String slotTypeName;
    if (!AssetFile::readString(stream, slotTypeName, kMaxTypeNameLength)) {
      return nullptr;
    }
    const UIPanelSlotTypeDesc* slotDesc = registry.findSlot(slotTypeName);
    if (!slotDesc || !slotDesc->create) {
      return nullptr;
    }
    UniquePtr<UIPanelSlot> slot = slotDesc->create();
    if (!slot || (slotDesc->readProperties && !slotDesc->readProperties(*slot, stream))) {
      return nullptr;
    }
    outSlot = std::move(slot);
  }

  uint32 childCount = 0;
  if (!readValue(stream, childCount) || childCount > desc->maxChildren) {
    return nullptr;
  }
  for (uint32 index = 0; index < childCount; ++index) {
    UniquePtr<UIPanelSlot> childSlot;
    UniquePtr<UIWidget> child = readWidgetNode(stream, depth + 1, count, pending, childSlot);
    if (!child || !desc->attachChild
        || desc->attachChild(*widget, std::move(child), std::move(childSlot)) == nullptr) {
      return nullptr;
    }
  }
  return widget;
}

NODISCARD UIWidget*
findNamed(UIWidget* widget, StringView name)
{
  if (!widget) {
    return nullptr;
  }
  if (widget->getName() == name) {
    return widget;
  }
  const SIZE_T childCount = widget->getChildCount();
  for (SIZE_T index = 0; index < childCount; ++index) {
    if (UIWidget* found = findNamed(widget->getChildAt(index), name)) {
      return found;
    }
  }
  return nullptr;
}

NODISCARD SIZE_T
countWidgets(const UIWidget* widget)
{
  if (!widget) {
    return 0;
  }
  SIZE_T count = 1;
  const SIZE_T childCount = widget->getChildCount();
  for (SIZE_T index = 0; index < childCount; ++index) {
    count += countWidgets(widget->getChildAt(index));
  }
  return count;
}

void
collectWidgetReferences(const UIWidget& widget, Vector<UUID>& outReferences)
{
  const UIWidgetTypeDesc* desc =
      UIDocumentRegistry::instance().findWidget(widget.getTypeName());
  if (desc && desc->collectReferences) {
    desc->collectReferences(widget, outReferences);
  }

  const SIZE_T childCount = widget.getChildCount();
  for (SIZE_T index = 0; index < childCount; ++index) {
    if (const UIWidget* child = widget.getChildAt(index)) {
      collectWidgetReferences(*child, outReferences);
    }
  }
}

} // namespace

/*
 */
UIDocumentRegistry&
UIDocumentRegistry::instance()
{
  static UIDocumentRegistry registry;
  return registry;
}

/*
 */
UIDocumentRegistry::UIDocumentRegistry() { registerBuiltinTypes(*this); }

/*
 */
void
UIDocumentRegistry::registerWidget(UIWidgetTypeDesc desc)
{
  m_widgets[desc.typeName] = std::move(desc);
}

/*
 */
void
UIDocumentRegistry::registerSlot(UIPanelSlotTypeDesc desc)
{
  m_slots[desc.typeName] = std::move(desc);
}

/*
 */
const UIWidgetTypeDesc*
UIDocumentRegistry::findWidget(StringView typeName) const
{
  const auto it = m_widgets.find(String(typeName));
  return it == m_widgets.end() ? nullptr : &it->second;
}

/*
 */
const UIPanelSlotTypeDesc*
UIDocumentRegistry::findSlot(StringView typeName) const
{
  const auto it = m_slots.find(String(typeName));
  return it == m_slots.end() ? nullptr : &it->second;
}

/*
 */
UniquePtr<UIWidget>
UIDocumentRegistry::createWidget(StringView typeName) const
{
  const UIWidgetTypeDesc* desc = findWidget(typeName);
  return desc && desc->create ? desc->create() : nullptr;
}

/*
 */
UniquePtr<UIPanelSlot>
UIDocumentRegistry::createSlot(StringView typeName) const
{
  const UIPanelSlotTypeDesc* desc = findSlot(typeName);
  return desc && desc->create ? desc->create() : nullptr;
}

/*
 */
UIWidget*
UIDocument::setRoot(UniquePtr<UIWidget> root)
{
  m_root = std::move(root);
  return m_root.get();
}

/*
 */
UniquePtr<UIWidget>
UIDocument::takeRoot() noexcept
{
  return std::move(m_root);
}

/*
 */
void
UIDocument::clear()
{
  m_root.reset();
}

/*
 */
UIWidget*
UIDocument::findWidget(StringView name) const
{
  return findNamed(m_root.get(), name);
}

/*
 */
SIZE_T
UIDocument::getWidgetCount() const { return countWidgets(m_root.get()); }

/*
 */
bool
UIDocument::serialize(DataStream& stream) const
{
  writeValue<uint32>(stream, SERIALIZATION_VERSION);
  writeBool(stream, m_root != nullptr);
  if (!m_root) {
    return true;
  }

  uint32 count = 0;
  return writeWidgetNode(*m_root, stream, 0, count);
}

/*
 */
bool
UIDocument::deserialize(DataStream& stream)
{
  uint32 version = 0;
  bool hasRoot = false;
  if (!readValue(stream, version) || version == 0 || version > SERIALIZATION_VERSION
      || !readBool(stream, hasRoot)) {
    return false;
  }

  UniquePtr<UIWidget> root;
  Vector<PendingNavigation> pending;
  uint32 count = 0;
  if (hasRoot) {
    UniquePtr<UIPanelSlot> rootSlot;
    root = readWidgetNode(stream, 0, count, pending, rootSlot);
    if (!root || rootSlot) {
      return false;
    }
  }

  for (const PendingNavigation& entry : pending) {
    if (UIWidget* target = findNamed(root.get(), entry.targetName)) {
      UINavigationLink link;
      link.rule = entry.rule;
      link.target = target;
      entry.widget->setNavigation(entry.direction, std::move(link));
    }
  }

  m_root = std::move(root);
  return true;
}

/*
 */
void
UIDocument::collectReferences(Vector<UUID>& outReferences) const
{
  if (m_root) {
    collectWidgetReferences(*m_root, outReferences);
  }
}

/*
 */
UniquePtr<UIUserWidget>
UIDocument::createUserWidget()
{
  if (!m_root) {
    return nullptr;
  }
  return chMakeUnique<UIDocumentUserWidget>(std::move(m_root));
}

/*
 */
UIDocumentUserWidget::UIDocumentUserWidget(UniquePtr<UIWidget> root)
 : m_pendingRoot(std::move(root))
{
}

/*
 */
void
UIDocumentUserWidget::onInitialized()
{
  if (m_pendingRoot) {
    getMutableWidgetTree().setRoot(std::move(m_pendingRoot));
  }
}

} // namespace chEngineSDK
