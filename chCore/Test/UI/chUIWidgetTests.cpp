/************************************************************************/
/**
 * @file chUIWidgetTests.cpp
 * @author Swampertor
 * @date 2026/11/25
 * @brief Tests of the remaining widgets: boxes, overlay, wrappers, controls, scroll box.
 */
/************************************************************************/
#include "chFont.h"
#include "chInputKey.h"
#include "chInputState.h"
#include "chLinearColor.h"
#include "chUIBoxSlot.h"
#include "chUIBorder.h"
#include "chUIButton.h"
#include "chUICheckBox.h"
#include "chUIDrawList.h"
#include "chUIEditableTextBox.h"
#include "chUIHorizontalBox.h"
#include "chUIImage.h"
#include "chUIOverlay.h"
#include "chUIOverlaySlot.h"
#include "chUIPanelWidget.h"
#include "chUIRect.h"
#include "chUIScrollBox.h"
#include "chUISizeBox.h"
#include "chUISlider.h"
#include "chUIUserWidget.h"
#include "chUIVerticalBox.h"
#include "chUIViewport.h"
#include "chUIWidget.h"
#include "chUIWidgetTree.h"

#include "catch.hpp"
#include "chTextTestHelpers.h"

using namespace chEngineSDK;

namespace {

/**
 * A widget with a fixed desired size and one solid sprite, so layout tests do not depend on
 * fonts or textures and paint tests can count what survived clipping.
 */
class UIDesiredWidget final : public UIWidget
{
 public:
  explicit UIDesiredWidget(const Vector2& desired) : m_desired(desired) {}

  NODISCARD Vector2
  getDesiredSize() const override
  {
    return m_desired;
  }

  void
  paint(UIDrawList& drawList) const override
  {
    drawList.addRect(getRect(), LinearColor::WHITE);
  }

 private:
  Vector2 m_desired;
};

class UIWidgetTestFixedSlot final : public UIPanelSlot
{
 public:
  explicit UIWidgetTestFixedSlot(const UIRect& rect) : m_rect(rect) {}

  void
  arrange(const UIGeometry& parentGeometry, UIGeometry& outChildGeometry) const override
  {
    outChildGeometry = parentGeometry;
    outChildGeometry.position = m_rect.position;
    outChildGeometry.size = m_rect.size;
  }

 private:
  UIRect m_rect;
};

class UIWidgetTestUserWidget final : public UIUserWidget
{
 public:
  NODISCARD UIPanelWidget*
  getPanel() const noexcept
  {
    return m_panel;
  }

  void
  onInitialized() override
  {
    m_panel = getMutableWidgetTree().createRoot<UIPanelWidget>();
    m_panel->setName("root");
  }

 private:
  UIPanelWidget* m_panel = nullptr;
};

class UIScrollTestUserWidget final : public UIUserWidget
{
 public:
  NODISCARD UIScrollBox*
  getBox() const noexcept
  {
    return m_box;
  }

  void
  onInitialized() override
  {
    m_box = getMutableWidgetTree().createRoot<UIScrollBox>();
    m_box->setName("scroll");
  }

 private:
  UIScrollBox* m_box = nullptr;
};

} // namespace

TEST_CASE("chCore - UI boxes lay out auto and fill children", "[UI]")
{
  SECTION("Vertical box stacks auto rows and divides the free space")
  {
    UIVerticalBox box;
    UIGeometry geometry;
    geometry.position = {10.0f, 20.0f};
    geometry.size = {200.0f, 100.0f};
    box.arrange(geometry);

    UIWidget* padded = box.addChild(chMakeUnique<UIDesiredWidget>(Vector2(40.0f, 10.0f)));
    REQUIRE(padded != nullptr);
    REQUIRE(StringView(padded->getSlot()->getTypeName()) == "UIVerticalBoxSlot");
    auto* paddedSlot = static_cast<UIBoxSlot*>(padded->getSlot());
    paddedSlot->setPadding({5.0f, 5.0f, 5.0f, 5.0f});
    paddedSlot->setHorizontalAlignment(UIHorizontalAlignment::Center);

    UIWidget* autoRow = box.addChild(chMakeUnique<UIDesiredWidget>(Vector2(20.0f, 20.0f)));
    REQUIRE(autoRow != nullptr);

    UIWidget* fillTwo = box.addChild(chMakeUnique<UIDesiredWidget>(Vector2::ZERO));
    REQUIRE(fillTwo != nullptr);
    static_cast<UIBoxSlot*>(fillTwo->getSlot())->setSizeRule(UISizeRule::Fill);
    static_cast<UIBoxSlot*>(fillTwo->getSlot())->setFillWeight(2.0f);

    UIWidget* fillOne = box.addChild(chMakeUnique<UIDesiredWidget>(Vector2::ZERO));
    REQUIRE(fillOne != nullptr);
    static_cast<UIBoxSlot*>(fillOne->getSlot())->setSizeRule(UISizeRule::Fill);
    static_cast<UIBoxSlot*>(fillOne->getSlot())->setFillWeight(1.0f);

    box.arrange(geometry);

    // Auto rows take their desired height plus padding; the two Fill rows share 60 px 2:1.
    REQUIRE(padded->getPosition() == Vector2(90.0f, 25.0f));
    REQUIRE(padded->getSize() == Vector2(40.0f, 10.0f));
    REQUIRE(autoRow->getPosition() == Vector2(10.0f, 40.0f));
    REQUIRE(autoRow->getSize() == Vector2(200.0f, 20.0f));
    REQUIRE(fillTwo->getPosition() == Vector2(10.0f, 60.0f));
    REQUIRE(fillTwo->getSize() == Vector2(200.0f, 40.0f));
    REQUIRE(fillOne->getPosition() == Vector2(10.0f, 100.0f));
    REQUIRE(fillOne->getSize() == Vector2(200.0f, 20.0f));

    REQUIRE(box.getDesiredSize() == Vector2(50.0f, 40.0f));
  }

  SECTION("Horizontal box places columns and aligns across the height")
  {
    UIHorizontalBox box;
    UIGeometry geometry;
    geometry.size = {300.0f, 100.0f};
    box.arrange(geometry);

    UIWidget* aligned = box.addChild(chMakeUnique<UIDesiredWidget>(Vector2(50.0f, 20.0f)));
    REQUIRE(aligned != nullptr);
    REQUIRE(StringView(aligned->getSlot()->getTypeName()) == "UIHorizontalBoxSlot");
    auto* alignedSlot = static_cast<UIBoxSlot*>(aligned->getSlot());
    alignedSlot->setPadding({5.0f, 0.0f, 5.0f, 0.0f});
    alignedSlot->setVerticalAlignment(UIVerticalAlignment::Center);

    UIWidget* fillOne = box.addChild(chMakeUnique<UIDesiredWidget>(Vector2::ZERO));
    REQUIRE(fillOne != nullptr);
    static_cast<UIBoxSlot*>(fillOne->getSlot())->setSizeRule(UISizeRule::Fill);
    static_cast<UIBoxSlot*>(fillOne->getSlot())->setFillWeight(1.0f);

    UIWidget* fillThree = box.addChild(chMakeUnique<UIDesiredWidget>(Vector2::ZERO));
    REQUIRE(fillThree != nullptr);
    static_cast<UIBoxSlot*>(fillThree->getSlot())->setSizeRule(UISizeRule::Fill);
    static_cast<UIBoxSlot*>(fillThree->getSlot())->setFillWeight(3.0f);

    box.arrange(geometry);

    REQUIRE(aligned->getPosition() == Vector2(5.0f, 40.0f));
    REQUIRE(aligned->getSize() == Vector2(50.0f, 20.0f));
    REQUIRE(fillOne->getPosition() == Vector2(60.0f, 0.0f));
    REQUIRE(fillOne->getSize() == Vector2(60.0f, 100.0f));
    REQUIRE(fillThree->getPosition() == Vector2(120.0f, 0.0f));
    REQUIRE(fillThree->getSize() == Vector2(180.0f, 100.0f));

    REQUIRE(box.getDesiredSize() == Vector2(60.0f, 20.0f));
  }
}

TEST_CASE("chCore - UI overlay, border and size box", "[UI]")
{
  SECTION("Overlay stacks children and aligns them")
  {
    UIOverlay overlay;
    UIGeometry geometry;
    geometry.size = {100.0f, 60.0f};
    overlay.arrange(geometry);

    UIWidget* fill = overlay.addChild(chMakeUnique<UIDesiredWidget>(Vector2(10.0f, 10.0f)));
    REQUIRE(fill != nullptr);
    REQUIRE(StringView(fill->getSlot()->getTypeName()) == "UIOverlaySlot");

    UIWidget* centered = overlay.addChild(chMakeUnique<UIDesiredWidget>(Vector2(40.0f, 20.0f)));
    REQUIRE(centered != nullptr);
    auto* slot = static_cast<UIOverlaySlot*>(centered->getSlot());
    slot->setHorizontalAlignment(UIHorizontalAlignment::Center);
    slot->setVerticalAlignment(UIVerticalAlignment::Center);

    overlay.arrange(geometry);

    REQUIRE(fill->getPosition() == Vector2::ZERO);
    REQUIRE(fill->getSize() == Vector2(100.0f, 60.0f));
    REQUIRE(centered->getPosition() == Vector2(30.0f, 20.0f));
    REQUIRE(centered->getSize() == Vector2(40.0f, 20.0f));
    REQUIRE(overlay.getDesiredSize() == Vector2(40.0f, 20.0f));

    // The last child is on top and wins the hit.
    REQUIRE(overlay.hitTest({50.0f, 30.0f}) == centered);
  }

  SECTION("Border pads its content and paints only an opaque brush")
  {
    UIBorder border;
    border.setPadding({10.0f, 5.0f, 10.0f, 5.0f});
    border.setBrushColor(LinearColor(1.0f, 0.0f, 0.0f, 1.0f));

    UIWidget* content =
        border.setContent(chMakeUnique<UIDesiredWidget>(Vector2(30.0f, 20.0f)));
    REQUIRE(content != nullptr);
    REQUIRE(border.getDesiredSize() == Vector2(50.0f, 30.0f));

    UIGeometry geometry;
    geometry.size = {100.0f, 60.0f};
    border.arrange(geometry);
    REQUIRE(content->getPosition() == Vector2(10.0f, 5.0f));
    REQUIRE(content->getSize() == Vector2(80.0f, 50.0f));

    UIDrawList drawList({100.0f, 100.0f});
    border.paint(drawList);
    REQUIRE(drawList.getSprites().size() == 1);

    border.setBrushColor(LinearColor(1.0f, 0.0f, 0.0f, 0.0f));
    drawList.clear();
    border.paint(drawList);
    REQUIRE(drawList.isEmpty());
  }

  SECTION("Size box overrides one or both axes and clamps to the room it has")
  {
    UISizeBox sizeBox;
    sizeBox.setWidthOverride(80.0f);
    UIWidget* content =
        sizeBox.setContent(chMakeUnique<UIDesiredWidget>(Vector2(30.0f, 20.0f)));
    REQUIRE(content != nullptr);
    REQUIRE(sizeBox.getDesiredSize() == Vector2(80.0f, 20.0f));

    UIGeometry geometry;
    geometry.size = {100.0f, 60.0f};
    sizeBox.arrange(geometry);
    REQUIRE(content->getSize() == Vector2(80.0f, 60.0f));

    sizeBox.setHeightOverride(30.0f);
    REQUIRE(sizeBox.getDesiredSize() == Vector2(80.0f, 30.0f));
    sizeBox.arrange(geometry);
    REQUIRE(content->getSize() == Vector2(80.0f, 30.0f));

    UIGeometry tight;
    tight.size = {50.0f, 20.0f};
    sizeBox.arrange(tight);
    REQUIRE(content->getSize() == Vector2(50.0f, 20.0f));
    REQUIRE_FALSE(sizeBox.isFocusable());
    REQUIRE(sizeBox.getVisibility() == UIVisibility::SelfHitTestInvisible);
  }
}

TEST_CASE("chCore - UI check box toggles and reports", "[UI]")
{
  UICheckBox checkBox;
  REQUIRE(checkBox.isFocusable());
  REQUIRE_FALSE(checkBox.isChecked());

  checkBox.setContent(chMakeUnique<UIDesiredWidget>(Vector2(40.0f, 20.0f)));
  REQUIRE(checkBox.getDesiredSize() == Vector2(66.0f, 20.0f));

  UIGeometry geometry;
  geometry.size = {66.0f, 20.0f};
  checkBox.arrange(geometry);
  REQUIRE(checkBox.getContent()->getPosition() == Vector2(26.0f, 2.0f));
  REQUIRE(checkBox.getContent()->getSize() == Vector2(40.0f, 16.0f));

  int32 changes = 0;
  bool lastState = false;
  checkBox.setOnCheckStateChanged([&changes, &lastState](bool checked) {
    ++changes;
    lastState = checked;
  });

  checkBox.onPointerClick({10.0f, 10.0f});
  REQUIRE(checkBox.isChecked());
  REQUIRE(changes == 1);
  REQUIRE(lastState);

  checkBox.onSubmit();
  REQUIRE_FALSE(checkBox.isChecked());
  REQUIRE(changes == 2);
  REQUIRE_FALSE(lastState);

  // A silent set does not notify; an unchanged set does not either.
  checkBox.setIsChecked(true, false);
  checkBox.setIsChecked(true, true);
  REQUIRE(changes == 2);

  checkBox.setEnabled(false);
  checkBox.onPointerClick({10.0f, 10.0f});
  checkBox.onSubmit();
  REQUIRE(checkBox.isChecked());
  REQUIRE(changes == 2);
}

TEST_CASE("chCore - UI slider clamps, snaps and follows the pointer", "[UI]")
{
  UISlider slider;
  REQUIRE(slider.isFocusable());
  slider.setMinValue(0.0f);
  slider.setMaxValue(10.0f);
  slider.setStep(1.0f);

  int32 changes = 0;
  float lastValue = -1.0f;
  slider.setOnValueChanged([&changes, &lastValue](float value) {
    ++changes;
    lastValue = value;
  });

  slider.setValue(4.6f);
  REQUIRE(slider.getValue() == 5.0f);
  REQUIRE(changes == 1);
  REQUIRE(lastValue == 5.0f);

  slider.setValue(4.6f);
  REQUIRE(changes == 1);

  slider.setValue(50.0f);
  REQUIRE(slider.getValue() == 10.0f);
  REQUIRE(changes == 2);

  slider.setValue(-3.0f);
  REQUIRE(slider.getValue() == 0.0f);
  REQUIRE(changes == 3);
  REQUIRE(slider.getNormalizedValue() == 0.0f);

  slider.setValue(5.0f, false);
  REQUIRE(slider.getValue() == 5.0f);
  REQUIRE(changes == 3);

  REQUIRE(slider.onScroll(1.0f));
  REQUIRE(slider.getValue() == 6.0f);
  REQUIRE(slider.onNavigation(UINavigation::Right));
  REQUIRE(slider.getValue() == 7.0f);
  REQUIRE(slider.onNavigation(UINavigation::Left));
  REQUIRE(slider.getValue() == 6.0f);

  UIGeometry geometry;
  geometry.size = {160.0f, 20.0f};
  slider.arrange(geometry);
  slider.onPointerDown({80.0f, 10.0f});
  REQUIRE(slider.isDragging());
  REQUIRE(slider.getValue() == 5.0f);

  slider.onPointerMove({155.0f, 10.0f});
  REQUIRE(slider.getValue() == 10.0f);
  slider.onPointerUp({155.0f, 10.0f});
  REQUIRE_FALSE(slider.isDragging());

  slider.onPointerClick({5.0f, 10.0f});
  REQUIRE(slider.getValue() == 0.0f);

  slider.setOrientation(UIOrientation::Vertical);
  REQUIRE(slider.getDesiredSize() == Vector2(20.0f, 160.0f));
  UIGeometry vertical;
  vertical.size = {20.0f, 160.0f};
  slider.arrange(vertical);
  slider.onPointerDown({10.0f, 5.0f});
  REQUIRE(slider.getValue() == 10.0f);
  slider.onPointerUp({10.0f, 5.0f});

  slider.setEnabled(false);
  REQUIRE_FALSE(slider.onScroll(1.0f));
  REQUIRE_FALSE(slider.onNavigation(UINavigation::Right));
  slider.onPointerDown({10.0f, 5.0f});
  REQUIRE_FALSE(slider.isDragging());
}

TEST_CASE("chCore - UI editable text box edits and routes text", "[UI]")
{
  const SPtr<Font> font = makeTestFont();

  SECTION("Caret and editing keys")
  {
    UIEditableTextBox field;
    REQUIRE(field.isFocusable());
    REQUIRE(field.isTextEditor());
    field.setFont(font.get());
    field.setText("abc");
    REQUIRE(field.getCaretIndex() == 3);
    REQUIRE(field.getDesiredSize().x > 0.0f);
    REQUIRE(field.getDesiredSize().y > 0.0f);

    REQUIRE(field.onTextEditingKey(InputKey::Left, false));
    REQUIRE(field.getCaretIndex() == 2);
    REQUIRE(field.onTextEditingKey(InputKey::Home, false));
    REQUIRE(field.getCaretIndex() == 0);
    REQUIRE(field.onTextEditingKey(InputKey::End, false));
    REQUIRE(field.getCaretIndex() == 3);
    REQUIRE_FALSE(field.onTextEditingKey(InputKey::Delete, false));

    REQUIRE(field.onTextEditingKey(InputKey::Left, false));
    REQUIRE(field.onTextEditingKey(InputKey::Delete, false));
    REQUIRE(field.getText() == "ab");
    REQUIRE(field.getCaretIndex() == 2);

    field.setText("abc");
    field.setCaretIndex(0);
    REQUIRE_FALSE(field.onTextEditingKey(InputKey::Backspace, false));
    REQUIRE(field.onTextEditingKey(InputKey::Delete, false));
    REQUIRE(field.getText() == "bc");
  }

  SECTION("Typing, limits, read-only and commit")
  {
    UIEditableTextBox field;
    field.setFont(font.get());

    String changed;
    String committed;
    field.setOnTextChanged([&changed](const String& text) { changed = text; });
    field.setOnTextCommitted([&committed](const String& text) { committed = text; });

    field.setText("hello");
    REQUIRE(changed.empty());
    field.onTextInput("!");
    REQUIRE(field.getText() == "hello!");
    REQUIRE(changed == "hello!");
    field.onSubmit();
    REQUIRE(committed == "hello!");

    // Single line: breaks are dropped.
    field.setText("");
    field.onTextInput("a\nb\rc");
    REQUIRE(field.getText() == "abc");

    field.setText("ab");
    field.setMaxLength(3);
    field.onTextInput("cde");
    REQUIRE(field.getText() == "abc");
    field.onTextInput("d");
    REQUIRE(field.getText() == "abc");

    field.setText("fixed");
    field.setReadOnly(true);
    field.onTextInput("x");
    REQUIRE(field.getText() == "fixed");
    REQUIRE_FALSE(field.onTextEditingKey(InputKey::Backspace, false));
  }

  SECTION("Viewport routes typed text and editing keys to the focused field")
  {
    UIViewport viewport;
    viewport.setViewportSize({400.0f, 300.0f});

    UniquePtr<UIWidgetTestUserWidget> user = chMakeUnique<UIWidgetTestUserWidget>();
    UIWidgetTestUserWidget* rawUser = user.get();
    REQUIRE(viewport.addUserWidget(std::move(user)) == rawUser);

    UIEditableTextBox* field = static_cast<UIEditableTextBox*>(
        rawUser->getPanel()->addChild(chMakeUnique<UIEditableTextBox>(),
                                      chMakeUnique<UIWidgetTestFixedSlot>(
                                          UIRect{{20.0f, 20.0f}, {200.0f, 30.0f}})));
    REQUIRE(field != nullptr);
    field->setFont(font.get());
    rawUser->refreshLayout();

    REQUIRE_FALSE(viewport.hasTextEditorFocus());
    REQUIRE_FALSE(viewport.processTextInput("x"));

    const UIViewport::InputResult press =
        viewport.processPointer({30.0f, 30.0f}, true, true, false, Vector2::ZERO);
    REQUIRE(press.bKeyboardCaptured);
    REQUIRE(viewport.hasTextEditorFocus());

    REQUIRE(viewport.processTextInput("Hi"));
    REQUIRE(field->getText() == "Hi");

    InputState state;
    state.setKey(InputKey::Backspace, true);
    REQUIRE(viewport.processTextEditingKeys(state));
    REQUIRE(field->getText() == "H");

    // A disabled field no longer holds the text editor focus, even though it keeps it.
    field->setEnabled(false);
    REQUIRE_FALSE(viewport.hasTextEditorFocus());
    REQUIRE_FALSE(viewport.processTextInput("x"));
    REQUIRE(field->getText() == "H");
  }
}

TEST_CASE("chCore - UI scroll box scrolls, clips and culls", "[UI]")
{
  SECTION("Content height, offset and clamping")
  {
    UIScrollBox box;
    UIGeometry geometry;
    geometry.size = {100.0f, 60.0f};
    box.arrange(geometry);

    Vector<UIWidget*> children;
    for (int32 index = 0; index < 4; ++index) {
      UIWidget* child = box.addChild(chMakeUnique<UIDesiredWidget>(Vector2(100.0f, 30.0f)));
      REQUIRE(child != nullptr);
      REQUIRE(StringView(child->getSlot()->getTypeName()) == "UIScrollBoxSlot");
      children.push_back(child);
    }

    box.arrange(geometry);

    REQUIRE(box.getDesiredSize() == Vector2(100.0f, 120.0f));
    REQUIRE(box.getContentHeight() == 120.0f);
    REQUIRE(box.getMaxScrollOffset() == 60.0f);
    REQUIRE(children[0]->getPosition() == Vector2(0.0f, 0.0f));
    REQUIRE(children[1]->getPosition() == Vector2(0.0f, 30.0f));
    REQUIRE(children[2]->getPosition() == Vector2(0.0f, 60.0f));
    REQUIRE(children[3]->getPosition() == Vector2(0.0f, 90.0f));

    // Every child carries the box as its clip.
    for (UIWidget* child : children) {
      REQUIRE(child->getGeometry().bClipped);
      REQUIRE(child->getGeometry().clipRect.position == Vector2::ZERO);
      REQUIRE(child->getGeometry().clipRect.size == Vector2(100.0f, 60.0f));
    }

    REQUIRE(box.hitTest({50.0f, 45.0f}) == children[1]);
    // The third row is inside its own rectangle but outside the 60 px view.
    REQUIRE(box.hitTest({50.0f, 75.0f}) == nullptr);
    REQUIRE(box.hitTest({50.0f, 150.0f}) == nullptr);

    box.setScrollOffset(60.0f);
    REQUIRE(box.getScrollOffset() == 60.0f);
    box.arrange(geometry);
    REQUIRE(children[0]->getPosition() == Vector2(0.0f, -60.0f));
    REQUIRE(children[3]->getPosition() == Vector2(0.0f, 30.0f));

    // The first child is inside its rectangle but outside the view, so it is not hit.
    REQUIRE(box.hitTest({50.0f, -45.0f}) == nullptr);
    REQUIRE(box.hitTest({50.0f, 45.0f}) == children[3]);

    box.setScrollOffset(1000.0f);
    REQUIRE(box.getScrollOffset() == 60.0f);

    // A notch away from the user moves the content the other way; it clamps at the top.
    REQUIRE(box.onScroll(1.0f));
    REQUIRE(box.getScrollOffset() == 12.0f);
    REQUIRE(box.onScroll(1.0f));
    REQUIRE(box.getScrollOffset() == 0.0f);
  }

  SECTION("Content that fits does not scroll")
  {
    UIScrollBox box;
    UIGeometry geometry;
    geometry.size = {100.0f, 60.0f};
    box.arrange(geometry);
    box.addChild(chMakeUnique<UIDesiredWidget>(Vector2(100.0f, 30.0f)));
    box.arrange(geometry);

    REQUIRE(box.getMaxScrollOffset() == 0.0f);
    REQUIRE_FALSE(box.onScroll(1.0f));
  }

  SECTION("Painting skips children that are entirely outside the view")
  {
    UIViewport viewport;
    viewport.setViewportSize({100.0f, 100.0f});

    UniquePtr<UIScrollTestUserWidget> user = chMakeUnique<UIScrollTestUserWidget>();
    UIScrollTestUserWidget* rawUser = user.get();
    REQUIRE(viewport.addUserWidget(std::move(user)) == rawUser);

    UIScrollBox* box = rawUser->getBox();
    REQUIRE(box != nullptr);
    const float heights[] = {60.0f, 10.0f, 10.0f, 100.0f};
    for (float height : heights) {
      REQUIRE(box->addChild(chMakeUnique<UIDesiredWidget>(Vector2(100.0f, height))) != nullptr);
    }
    rawUser->refreshLayout();

    UIDrawList drawList({100.0f, 100.0f});
    viewport.paint(drawList);
    // At the top the 100 px view still touches all four rows (0..180).
    REQUIRE(drawList.getSprites().size() == 4);

    box->setScrollOffset(80.0f);
    rawUser->refreshLayout();
    drawList.clear();
    viewport.paint(drawList);
    // Scrolled to the end only the last 100 px row is inside the view.
    REQUIRE(drawList.getSprites().size() == 1);
  }
}

TEST_CASE("chCore - UI auto rows size buttons and images", "[UI]")
{
  // A button is as large as its content, and an image asks for its source rectangle.
  UIButton button;
  button.setContent(chMakeUnique<UIDesiredWidget>(Vector2(60.0f, 24.0f)));
  REQUIRE(button.getDesiredSize() == Vector2(60.0f, 24.0f));

  UIImage image;
  image.setSourceSize({64.0f, 32.0f});
  REQUIRE(image.getDesiredSize() == Vector2(64.0f, 32.0f));

  UIVerticalBox box;
  UIGeometry geometry;
  geometry.size = {200.0f, 100.0f};
  box.arrange(geometry);

  UniquePtr<UIButton> ownedButton = chMakeUnique<UIButton>();
  ownedButton->setContent(chMakeUnique<UIDesiredWidget>(Vector2(60.0f, 24.0f)));
  UIButton* buttonRow = static_cast<UIButton*>(box.addChild(std::move(ownedButton)));
  REQUIRE(buttonRow != nullptr);

  UniquePtr<UIImage> ownedImage = chMakeUnique<UIImage>();
  ownedImage->setSourceSize({64.0f, 32.0f});
  UIImage* imageRow = static_cast<UIImage*>(box.addChild(std::move(ownedImage)));
  REQUIRE(imageRow != nullptr);

  box.arrange(geometry);

  REQUIRE(buttonRow->getPosition() == Vector2(0.0f, 0.0f));
  REQUIRE(buttonRow->getSize() == Vector2(200.0f, 24.0f));
  REQUIRE(imageRow->getPosition() == Vector2(0.0f, 24.0f));
  REQUIRE(imageRow->getSize() == Vector2(200.0f, 32.0f));
  REQUIRE(box.getDesiredSize() == Vector2(64.0f, 56.0f));
}
