/************************************************************************/
/**
 * @file chUIDocumentTests.cpp
 * @author Swampertor
 * @date 2026/11/26
 * @brief Tests of UI document serialization, the registry and the document asset.
 */
/************************************************************************/
#include <cstring>

#include "chAssetFile.h"
#include "chAssetRegister.h"
#include "chFileStream.h"
#include "chStringUtils.h"
#include "chUIDocument.h"
#include "chUIDocumentAsset.h"

#include "chUIBorder.h"
#include "chUIBoxSlot.h"
#include "chUIButton.h"
#include "chUICanvasPanel.h"
#include "chUICanvasPanelSlot.h"
#include "chUICheckBox.h"
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

#include "catch.hpp"

using namespace chEngineSDK;

namespace {

/**
 * A widget the registry does not know, used to prove that saving a document with an
 * unregistered class fails instead of writing a file that cannot be opened.
 */
class UIUnregisteredWidget final : public UIWidget
{
 public:
  NODISCARD const ANSICHAR*
  getTypeName() const noexcept override
  {
    return "UIUnregisteredWidget";
  }
};

const UUID kFontId = UUID::createRandom();

void
setTextProperties(UITextBlock& text, StringView value)
{
  text.setName("Title");
  text.setText(value);
  text.setFontId(kFontId);
  text.setFontSize(22.0f);
  text.setAlign(TextAlign::Center);
  text.setWrapText(true);
  text.setMargin({1.0f, 2.0f, 3.0f, 4.0f});
  text.setLineSpacing(1.5f);
  text.setLetterSpacing(2.0f);
  text.setMonospaceDigits(true);
  text.setRichText(false);
  text.setColor(LinearColor(0.1f, 0.2f, 0.3f, 0.4f));
  text.setEnabled(false);
  text.setFocusable(false);
  text.setVisibility(UIVisibility::Hidden);

  UIRenderTransform transform;
  transform.translation = Vector2(5.0f, 6.0f);
  transform.angle = 0.5f;
  transform.scale = Vector2(2.0f, 3.0f);
  transform.pivot = Vector2(0.25f, 0.75f);
  text.setRenderTransform(transform);
}

void
setBoxSlotProperties(UIVerticalBoxSlot& slot)
{
  slot.setPadding({1.0f, 2.0f, 3.0f, 4.0f});
  slot.setSizeRule(UISizeRule::Fill);
  slot.setFillWeight(2.5f);
  slot.setHorizontalAlignment(UIHorizontalAlignment::Center);
  slot.setVerticalAlignment(UIVerticalAlignment::Bottom);
}

void
setCanvasSlotProperties(UICanvasPanelSlot& slot)
{
  slot.setAnchors({0.1f, 0.2f}, {0.3f, 0.4f});
  slot.setAlignment({0.25f, 0.5f});
  slot.setPosition({7.0f, 8.0f});
  slot.setSize({320.0f, 180.0f});
}

void
buildDocument(UIDocument& document)
{
  UICanvasPanel* canvas =
      static_cast<UICanvasPanel*>(document.setRoot(chMakeUnique<UICanvasPanel>()));
  canvas->setName("Root");

  UIVerticalBox* column =
      static_cast<UIVerticalBox*>(canvas->addChild(chMakeUnique<UIVerticalBox>()));
  column->setName("Column");
  setCanvasSlotProperties(*static_cast<UICanvasPanelSlot*>(column->getSlot()));

  UITextBlock* title =
      static_cast<UITextBlock*>(column->addChild(chMakeUnique<UITextBlock>()));
  setTextProperties(*title, "Score 12");
  setBoxSlotProperties(*static_cast<UIVerticalBoxSlot*>(title->getSlot()));

  UIButton* button = static_cast<UIButton*>(column->addChild(chMakeUnique<UIButton>()));
  button->setName("Play");
  button->setNormalColor(LinearColor(0.2f, 0.3f, 0.4f, 1.0f));
  button->setHoveredColor(LinearColor(0.3f, 0.4f, 0.5f, 1.0f));
  button->setPressedColor(LinearColor(0.1f, 0.2f, 0.3f, 1.0f));
  button->setDisabledColor(LinearColor(0.05f, 0.05f, 0.05f, 0.5f));

  UITextBlock* buttonLabel =
      static_cast<UITextBlock*>(button->setContent(chMakeUnique<UITextBlock>()));
  buttonLabel->setName("PlayLabel");
  buttonLabel->setText("Play");

  UIImage* image = static_cast<UIImage*>(column->addChild(chMakeUnique<UIImage>()));
  image->setName("Icon");
  image->setFilter(SpriteFilter::Point);
  image->setSourcePosition(Vector2(2.0f, 4.0f));
  image->setSourceSize(Vector2(16.0f, 16.0f));
  image->setFlipX(true);
  image->setFlipY(false);
  image->setBlendMode(SpriteBlendMode::Additive);

  UICheckBox* checkBox =
      static_cast<UICheckBox*>(column->addChild(chMakeUnique<UICheckBox>()));
  checkBox->setName("Mute");
  checkBox->setIsChecked(true, false);
  checkBox->setNormalColor(LinearColor(0.1f, 0.1f, 0.1f, 1.0f));
  checkBox->setHoveredColor(LinearColor(0.2f, 0.2f, 0.2f, 1.0f));
  checkBox->setPressedColor(LinearColor(0.05f, 0.05f, 0.05f, 1.0f));
  checkBox->setDisabledColor(LinearColor(0.02f, 0.02f, 0.02f, 0.5f));
  checkBox->setCheckMarkColor(LinearColor(0.9f, 0.8f, 0.7f, 1.0f));
  checkBox->setCheckBoxSize(24.0f);
  checkBox->setSpacing(12.0f);

  UISlider* slider = static_cast<UISlider*>(column->addChild(chMakeUnique<UISlider>()));
  slider->setName("Volume");
  slider->setMinValue(-2.0f);
  slider->setMaxValue(8.0f);
  slider->setStep(0.5f);
  slider->setValue(3.5f, false);
  slider->setOrientation(UIOrientation::Vertical);
  slider->setTrackColor(LinearColor(0.1f, 0.1f, 0.1f, 1.0f));
  slider->setFillColor(LinearColor(0.2f, 0.4f, 0.6f, 1.0f));
  slider->setThumbColor(LinearColor(0.9f, 0.9f, 0.9f, 1.0f));
  slider->setDisabledColor(LinearColor(0.05f, 0.05f, 0.05f, 0.5f));

  UIEditableTextBox* field =
      static_cast<UIEditableTextBox*>(column->addChild(chMakeUnique<UIEditableTextBox>()));
  field->setName("Name");
  field->setText("Player");
  field->setHintText("Name");
  field->setFontId(kFontId);
  field->setFontSize(18.0f);
  field->setPadding({5.0f, 6.0f, 7.0f, 8.0f});
  field->setMaxLength(24);
  field->setReadOnly(true);
  field->setPassword(true);
  field->setTextColor(LinearColor(0.7f, 0.7f, 0.7f, 1.0f));
  field->setHintColor(LinearColor(0.4f, 0.4f, 0.4f, 1.0f));
  field->setBackgroundColor(LinearColor(0.05f, 0.05f, 0.05f, 1.0f));
  field->setCaretColor(LinearColor(0.9f, 0.9f, 0.9f, 1.0f));

  UIBorder* border = static_cast<UIBorder*>(column->addChild(chMakeUnique<UIBorder>()));
  border->setName("Panel");
  border->setBrushColor(LinearColor(0.2f, 0.2f, 0.3f, 0.9f));
  border->setPadding({9.0f, 10.0f, 11.0f, 12.0f});

  UISizeBox* sizeBox = static_cast<UISizeBox*>(border->setContent(chMakeUnique<UISizeBox>()));
  sizeBox->setName("Fixed");
  sizeBox->setWidthOverride(200.0f);
  sizeBox->setHeightOverride(100.0f);

  UIImage* fixedImage = static_cast<UIImage*>(sizeBox->setContent(chMakeUnique<UIImage>()));
  fixedImage->setName("FixedImage");

  UIHorizontalBox* row =
      static_cast<UIHorizontalBox*>(column->addChild(chMakeUnique<UIHorizontalBox>()));
  row->setName("Row");
  UITextBlock* rowText = static_cast<UITextBlock*>(row->addChild(chMakeUnique<UITextBlock>()));
  rowText->setName("RowText");
  rowText->setText("Row");

  UIOverlay* overlay = static_cast<UIOverlay*>(column->addChild(chMakeUnique<UIOverlay>()));
  overlay->setName("Overlay");
  UIImage* overlayImage = static_cast<UIImage*>(overlay->addChild(chMakeUnique<UIImage>()));
  overlayImage->setName("OverlayImage");
  UIOverlaySlot* overlaySlot = static_cast<UIOverlaySlot*>(overlayImage->getSlot());
  overlaySlot->setPadding({13.0f, 14.0f, 15.0f, 16.0f});
  overlaySlot->setHorizontalAlignment(UIHorizontalAlignment::Right);
  overlaySlot->setVerticalAlignment(UIVerticalAlignment::Top);

  UIScrollBox* scroll =
      static_cast<UIScrollBox*>(column->addChild(chMakeUnique<UIScrollBox>()));
  scroll->setName("Scroll");
  scroll->setScrollStep(64.0f);
  UITextBlock* scrollText =
      static_cast<UITextBlock*>(scroll->addChild(chMakeUnique<UITextBlock>()));
  scrollText->setName("ScrollText");
  scrollText->setText("Scroll");
  REQUIRE(scrollText->getSlot() != nullptr);
  REQUIRE(StringView(scrollText->getSlot()->getTypeName()) == "UIScrollBoxSlot");

  title->setNavigation(UINavigation::Right,
                       {UINavigationRule::Explicit, button, Function<UIWidget*()>()});
}

} // namespace

TEST_CASE("chCore - UI document serialization", "[UI]")
{
  UIDocument document;
  buildDocument(document);

  REQUIRE(document.getWidgetCount() > 10);
  REQUIRE(document.findWidget("Title") != nullptr);
  REQUIRE(document.findWidget("Play") != nullptr);
  REQUIRE(document.findWidget("NoSuchWidget") == nullptr);

  MemoryDataStream stream;
  REQUIRE(document.serialize(stream));
  stream.seek(0);

  UIDocument loaded;
  REQUIRE(loaded.deserialize(stream));
  REQUIRE(loaded.getWidgetCount() == document.getWidgetCount());

  UIWidget* root = loaded.getRoot();
  REQUIRE(root != nullptr);
  REQUIRE(StringView(root->getTypeName()) == "UICanvasPanel");
  REQUIRE(root->getName() == "Root");

  UITextBlock* title = static_cast<UITextBlock*>(loaded.findWidget("Title"));
  REQUIRE(title != nullptr);
  REQUIRE(title->getText() == "Score 12");
  REQUIRE(title->getFontId() == kFontId);
  REQUIRE(title->getFontSize() == Approx(22.0f));
  REQUIRE(title->getAlign() == TextAlign::Center);
  REQUIRE(title->getWrapText());
  REQUIRE(title->getMargin().left == Approx(1.0f));
  REQUIRE(title->getMargin().bottom == Approx(4.0f));
  REQUIRE(title->getLineSpacing() == Approx(1.5f));
  REQUIRE(title->getLetterSpacing() == Approx(2.0f));
  REQUIRE(title->getMonospaceDigits());
  REQUIRE_FALSE(title->getRichText());
  REQUIRE(title->getColor().r == Approx(0.1f));
  REQUIRE(title->getColor().a == Approx(0.4f));
  REQUIRE_FALSE(title->isEnabled());
  REQUIRE_FALSE(title->isFocusable());
  REQUIRE(title->getVisibility() == UIVisibility::Hidden);
  REQUIRE(title->getRenderTransform().translation.x == Approx(5.0f));
  REQUIRE(title->getRenderTransform().angle == Approx(0.5f));
  REQUIRE(title->getRenderTransform().scale.y == Approx(3.0f));
  REQUIRE(title->getRenderTransform().pivot.x == Approx(0.25f));

  const UIVerticalBoxSlot* titleSlot = static_cast<const UIVerticalBoxSlot*>(title->getSlot());
  REQUIRE(titleSlot != nullptr);
  REQUIRE(StringView(titleSlot->getTypeName()) == "UIVerticalBoxSlot");
  REQUIRE(titleSlot->getPadding().top == Approx(2.0f));
  REQUIRE(titleSlot->getSizeRule() == UISizeRule::Fill);
  REQUIRE(titleSlot->getFillWeight() == Approx(2.5f));
  REQUIRE(titleSlot->getHorizontalAlignment() == UIHorizontalAlignment::Center);
  REQUIRE(titleSlot->getVerticalAlignment() == UIVerticalAlignment::Bottom);

  UIWidget* play = loaded.findWidget("Play");
  REQUIRE(play != nullptr);
  REQUIRE(StringView(play->getTypeName()) == "UIButton");
  REQUIRE(play->getChildCount() == 1);
  REQUIRE(play->getChildAt(0) != nullptr);
  REQUIRE(StringView(play->getChildAt(0)->getTypeName()) == "UITextBlock");
  REQUIRE(StringView(play->getChildAt(0)->getSlot()->getTypeName()) == "UIPanelSlot");

  UIButton* button = static_cast<UIButton*>(play);
  REQUIRE(button->getNormalColor().g == Approx(0.3f));
  REQUIRE(button->getHoveredColor().b == Approx(0.5f));
  REQUIRE(button->getPressedColor().r == Approx(0.1f));
  REQUIRE(button->getDisabledColor().a == Approx(0.5f));

  const UINavigationLink& link = title->getNavigation(UINavigation::Right);
  REQUIRE(link.rule == UINavigationRule::Explicit);
  REQUIRE(link.target == play);

  UIVerticalBox* column = static_cast<UIVerticalBox*>(loaded.findWidget("Column"));
  REQUIRE(column != nullptr);
  const UICanvasPanelSlot* canvasSlot =
      static_cast<const UICanvasPanelSlot*>(column->getSlot());
  REQUIRE(canvasSlot != nullptr);
  REQUIRE(canvasSlot->getAnchorMin().x == Approx(0.1f));
  REQUIRE(canvasSlot->getAnchorMax().y == Approx(0.4f));
  REQUIRE(canvasSlot->getAlignment().y == Approx(0.5f));
  REQUIRE(canvasSlot->getPosition().x == Approx(7.0f));
  REQUIRE(canvasSlot->getSize().y == Approx(180.0f));

  UIImage* image = static_cast<UIImage*>(loaded.findWidget("Icon"));
  REQUIRE(image != nullptr);
  REQUIRE(image->getTexture() == nullptr);
  REQUIRE(image->getFilter() == SpriteFilter::Point);
  REQUIRE(image->getSourcePosition().x == Approx(2.0f));
  REQUIRE(image->getSourceSize().y == Approx(16.0f));
  REQUIRE(image->getFlipX());
  REQUIRE_FALSE(image->getFlipY());
  REQUIRE(image->getBlendMode() == SpriteBlendMode::Additive);

  UICheckBox* checkBox = static_cast<UICheckBox*>(loaded.findWidget("Mute"));
  REQUIRE(checkBox != nullptr);
  REQUIRE(checkBox->isChecked());
  REQUIRE(checkBox->getCheckBoxSize() == Approx(24.0f));
  REQUIRE(checkBox->getSpacing() == Approx(12.0f));
  REQUIRE(checkBox->getCheckMarkColor().b == Approx(0.7f));

  UISlider* slider = static_cast<UISlider*>(loaded.findWidget("Volume"));
  REQUIRE(slider != nullptr);
  REQUIRE(slider->getValue() == Approx(3.5f));
  REQUIRE(slider->getMinValue() == Approx(-2.0f));
  REQUIRE(slider->getMaxValue() == Approx(8.0f));
  REQUIRE(slider->getStep() == Approx(0.5f));
  REQUIRE(slider->getOrientation() == UIOrientation::Vertical);
  REQUIRE(slider->getFillColor().g == Approx(0.4f));

  UIEditableTextBox* field = static_cast<UIEditableTextBox*>(loaded.findWidget("Name"));
  REQUIRE(field != nullptr);
  REQUIRE(field->getText() == "Player");
  REQUIRE(field->getHintText() == "Name");
  REQUIRE(field->getFontId() == kFontId);
  REQUIRE(field->getFontSize() == Approx(18.0f));
  REQUIRE(field->getPadding().bottom == Approx(8.0f));
  REQUIRE(field->getMaxLength() == 24);
  REQUIRE(field->isReadOnly());
  REQUIRE(field->isPassword());
  REQUIRE(field->getTextColor().r == Approx(0.7f));

  UIBorder* border = static_cast<UIBorder*>(loaded.findWidget("Panel"));
  REQUIRE(border != nullptr);
  REQUIRE(border->getBrushColor().b == Approx(0.3f));
  REQUIRE(border->getPadding().right == Approx(11.0f));

  UISizeBox* sizeBox = static_cast<UISizeBox*>(loaded.findWidget("Fixed"));
  REQUIRE(sizeBox != nullptr);
  REQUIRE(sizeBox->getWidthOverride() == Approx(200.0f));
  REQUIRE(sizeBox->getHeightOverride() == Approx(100.0f));
  REQUIRE(sizeBox->getContent() != nullptr);
  REQUIRE(StringView(sizeBox->getContent()->getTypeName()) == "UIImage");

  UIImage* overlayImage = static_cast<UIImage*>(loaded.findWidget("OverlayImage"));
  REQUIRE(overlayImage != nullptr);
  const UIOverlaySlot* overlaySlot =
      static_cast<const UIOverlaySlot*>(overlayImage->getSlot());
  REQUIRE(overlaySlot != nullptr);
  REQUIRE(overlaySlot->getPadding().left == Approx(13.0f));
  REQUIRE(overlaySlot->getHorizontalAlignment() == UIHorizontalAlignment::Right);
  REQUIRE(overlaySlot->getVerticalAlignment() == UIVerticalAlignment::Top);

  UIScrollBox* scroll = static_cast<UIScrollBox*>(loaded.findWidget("Scroll"));
  REQUIRE(scroll != nullptr);
  REQUIRE(scroll->getScrollStep() == Approx(64.0f));
  REQUIRE(scroll->getChildAt(0) != nullptr);
  REQUIRE(StringView(scroll->getChildAt(0)->getSlot()->getTypeName()) == "UIScrollBoxSlot");

  Vector<UUID> references;
  loaded.collectReferences(references);
  REQUIRE(references.size() == 1);
  REQUIRE(references[0] == kFontId);
}

TEST_CASE("chCore - UI document registry", "[UI]")
{
  UIDocumentRegistry& registry = UIDocumentRegistry::instance();

  const UIWidgetTypeDesc* textType = registry.findWidget("UITextBlock");
  REQUIRE(textType != nullptr);
  REQUIRE(textType->create != nullptr);
  REQUIRE(textType->attachChild == nullptr);

  UniquePtr<UIWidget> text = registry.createWidget("UITextBlock");
  REQUIRE(text != nullptr);
  REQUIRE(StringView(text->getTypeName()) == "UITextBlock");

  UniquePtr<UIWidget> panel = registry.createWidget("UIPanelWidget");
  REQUIRE(panel != nullptr);
  REQUIRE(registry.findWidget("UIPanelWidget")->attachChild != nullptr);

  REQUIRE(registry.createWidget("NoSuchWidget") == nullptr);
  REQUIRE(registry.findWidget("NoSuchWidget") == nullptr);

  const UIPanelSlotTypeDesc* canvasSlot = registry.findSlot("UICanvasPanelSlot");
  REQUIRE(canvasSlot != nullptr);
  REQUIRE(canvasSlot->create != nullptr);
  UniquePtr<UIPanelSlot> slot = registry.createSlot("UICanvasPanelSlot");
  REQUIRE(slot != nullptr);
  REQUIRE(StringView(slot->getTypeName()) == "UICanvasPanelSlot");

  REQUIRE(registry.createSlot("NoSuchSlot") == nullptr);
  REQUIRE(registry.findSlot("NoSuchSlot") == nullptr);
}

TEST_CASE("chCore - UI document rejects broken data", "[UI]")
{
  SECTION("A version this engine does not know")
  {
    MemoryDataStream stream;
    const uint32 version = UIDocument::SERIALIZATION_VERSION + 1;
    stream.write(&version, sizeof(version));
    const uint8 hasRoot = 0;
    stream.write(&hasRoot, sizeof(hasRoot));
    stream.seek(0);

    UIDocument document;
    REQUIRE_FALSE(document.deserialize(stream));
    REQUIRE(document.isEmpty());
  }

  SECTION("A widget class the registry does not know")
  {
    MemoryDataStream stream;
    const uint32 version = UIDocument::SERIALIZATION_VERSION;
    stream.write(&version, sizeof(version));
    const uint8 hasRoot = 1;
    stream.write(&hasRoot, sizeof(hasRoot));
    AssetFile::writeString(stream, "UIUnregisteredWidget");
    stream.seek(0);

    UIDocument document;
    REQUIRE_FALSE(document.deserialize(stream));
  }

  SECTION("A cut file leaves the document unchanged")
  {
    UIDocument document;
    buildDocument(document);
    MemoryDataStream stream;
    REQUIRE(document.serialize(stream));

    Vector<uint8> bytes(stream.size());
    std::memcpy(bytes.data(), stream.getStartPtr(), bytes.size());

    MemoryDataStream cut(bytes.size() / 2);
    cut.write(bytes.data(), bytes.size() / 2);
    cut.seek(0);

    UIDocument target;
    target.setRoot(chMakeUnique<UICanvasPanel>());
    target.getRoot()->setName("Keep");
    REQUIRE_FALSE(target.deserialize(cut));
    REQUIRE(target.findWidget("Keep") != nullptr);
  }

  SECTION("A document with an unregistered widget cannot be saved")
  {
    UIDocument document;
    document.setRoot(chMakeUnique<UIUnregisteredWidget>());
    MemoryDataStream stream;
    REQUIRE_FALSE(document.serialize(stream));
  }
}

TEST_CASE("chCore - UI document asset", "[UI]")
{
  UIDocument document;
  buildDocument(document);

  AssetMetadata metadata;
  metadata.uuid = UUID::createRandom();
  metadata.assetType = AssetTypeTraits<UIDocumentAsset>::getTypeId();
  REQUIRE(StringUtils::copyToBuffer(metadata.name, "Hud"));
  REQUIRE(StringUtils::copyToBuffer(metadata.assetPath, "/Game/UI"));

  UIDocumentAsset asset(metadata);
  REQUIRE(asset.setContent(document));
  REQUIRE_FALSE(asset.getContent().empty());
  REQUIRE(StringView(asset.getFileExtension()) == ".chAss");

  UniquePtr<UIDocument> documentCopy = asset.createDocument();
  REQUIRE(documentCopy != nullptr);
  REQUIRE(documentCopy->findWidget("Title") != nullptr);
  REQUIRE(documentCopy->getWidgetCount() == document.getWidgetCount());

  UniquePtr<UIUserWidget> userWidget = asset.createUserWidget();
  REQUIRE(userWidget != nullptr);
  REQUIRE(userWidget->getRootWidget() == nullptr);
  userWidget->initialize();
  REQUIRE(userWidget->getRootWidget() != nullptr);
  REQUIRE(StringView(userWidget->getRootWidget()->getTypeName()) == "UICanvasPanel");
  REQUIRE(userWidget->findWidgetByName("Title") != nullptr);
  REQUIRE(userWidget->findWidgetByName("OverlayImage") != nullptr);

  // A second instance reads the bytes again and owns different widgets.
  UniquePtr<UIUserWidget> second = asset.createUserWidget();
  REQUIRE(second != nullptr);
  second->initialize();
  REQUIRE(second->getRootWidget() != userWidget->getRootWidget());
  REQUIRE(second->findWidgetByName("Title") != userWidget->findWidgetByName("Title"));

  SECTION("The asset data round-trips through a stream")
  {
    struct TestDocumentAsset final : UIDocumentAsset {
      using UIDocumentAsset::collectReferences;
      using UIDocumentAsset::deserialize;
      using UIDocumentAsset::serialize;
      using UIDocumentAsset::UIDocumentAsset;
    };

    TestDocumentAsset saved(metadata);
    REQUIRE(saved.setContent(document));

    Vector<UUID> references;
    saved.collectReferences(references);
    REQUIRE(references.size() == 1);
    REQUIRE(references[0] == kFontId);

    SPtr<DataStream> stream = chMakeShared<MemoryDataStream>();
    REQUIRE(saved.serialize(stream));
    stream->seek(0);

    TestDocumentAsset loaded(metadata);
    REQUIRE(loaded.deserialize(stream));
    REQUIRE(loaded.getContent().size() == saved.getContent().size());
    REQUIRE(loaded.createDocument() != nullptr);
  }

  SECTION("A broken asset version is rejected")
  {
    struct TestDocumentAsset final : UIDocumentAsset {
      using UIDocumentAsset::deserialize;
      using UIDocumentAsset::serialize;
      using UIDocumentAsset::UIDocumentAsset;
    };

    TestDocumentAsset saved(metadata);
    REQUIRE(saved.setContent(document));

    SPtr<DataStream> stream = chMakeShared<MemoryDataStream>();
    REQUIRE(saved.serialize(stream));
    const uint32 brokenVersion = UIDocumentAsset::SERIALIZATION_VERSION + 1;
    stream->seek(0);
    stream->write(&brokenVersion, sizeof(brokenVersion));
    stream->seek(0);

    TestDocumentAsset loaded(metadata);
    REQUIRE_FALSE(loaded.deserialize(stream));
  }
}

TEST_CASE("chCore - UI document asset registration", "[UI]")
{
  AssetRegister registry;
  registry.registerAssetCreator<UIDocumentAsset>();

  AssetMetadata metadata;
  metadata.uuid = UUID::createRandom();
  metadata.assetType = AssetTypeTraits<UIDocumentAsset>::getTypeId();

  const AssetCreatorFunc creator =
      registry.getAssetCreator(AssetTypeTraits<UIDocumentAsset>::getTypeId());
  REQUIRE(creator != nullptr);
  const SPtr<IAsset> asset = creator(metadata);
  REQUIRE(asset != nullptr);
  REQUIRE(asset->isTypeOf<UIDocumentAsset>());
  REQUIRE(StringView(registry.getAssetTypeName(AssetTypeTraits<UIDocumentAsset>::getTypeId()))
          == "UIDocumentAsset");
}
