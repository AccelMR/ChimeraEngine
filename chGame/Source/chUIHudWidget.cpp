/************************************************************************/
/**
 * @file chUIHudWidget.cpp
 * @author Swampertor
 * @date 2026/11/23
 * @brief The sample HUD the game runtime draws over the scene.
 */
/************************************************************************/
#include "chUIHudWidget.h"

#include "chStringUtils.h"
#include "chUIButton.h"
#include "chUICanvasPanel.h"
#include "chUICanvasPanelSlot.h"
#include "chUIImage.h"
#include "chUITextBlock.h"

namespace chEngineSDK {

namespace {

/*
 * Places one canvas child. Every direct child of the HUD root gets a UICanvasPanelSlot
 * because the root is a UICanvasPanel.
 */
void
placeCanvasChild(UIWidget* widget,
                 const Vector2& anchor,
                 const Vector2& alignment,
                 const Vector2& position,
                 const Vector2& size)
{
  UICanvasPanelSlot* slot = static_cast<UICanvasPanelSlot*>(widget->getSlot());
  CH_ASSERT(slot != nullptr);
  slot->setAnchors(anchor, anchor);
  slot->setAlignment(alignment);
  slot->setPosition(position);
  slot->setSize(size);
}

} // namespace

UIHudWidget::UIHudWidget() = default;

UIHudWidget::~UIHudWidget() = default;

/*
 */
void
UIHudWidget::onInitialized()
{
  m_root = getMutableWidgetTree().createRoot<UICanvasPanel>();
  m_root->setName("hud.root");

  UITextBlock* title = static_cast<UITextBlock*>(m_root->addChild(
      chMakeUnique<UITextBlock>()));
  title->setName("hud.title");
  title->setText("Chimera Runtime HUD");
  title->setFontSize(30.0f);
  title->setTextColor(LinearColor::WHITE);
  placeCanvasChild(title, Vector2::ZERO, Vector2::ZERO, {24.0f, 16.0f}, {480.0f, 40.0f});

  UITextBlock* hint = static_cast<UITextBlock*>(m_root->addChild(
      chMakeUnique<UITextBlock>()));
  hint->setName("hud.hint");
  hint->setText("WASD Move   Space Jump   Shift Sprint   Mouse Look");
  hint->setFontSize(16.0f);
  hint->setTextColor({0.75f, 0.78f, 0.82f, 1.0f});
  placeCanvasChild(hint, Vector2::ZERO, Vector2::ZERO, {24.0f, 58.0f}, {540.0f, 22.0f});

  // Two tinted images make the crosshair. They stay fixed to the center of the window
  // because their anchors and alignment are the middle of the parent.
  const LinearColor crosshairColor(1.0f, 1.0f, 1.0f, 0.6f);
  UIImage* horizontal = static_cast<UIImage*>(m_root->addChild(chMakeUnique<UIImage>()));
  horizontal->setName("hud.crosshair.horizontal");
  horizontal->setColor(crosshairColor);
  placeCanvasChild(horizontal, {0.5f, 0.5f}, {0.5f, 0.5f}, Vector2::ZERO, {14.0f, 2.0f});

  UIImage* vertical = static_cast<UIImage*>(m_root->addChild(chMakeUnique<UIImage>()));
  vertical->setName("hud.crosshair.vertical");
  vertical->setColor(crosshairColor);
  placeCanvasChild(vertical, {0.5f, 0.5f}, {0.5f, 0.5f}, Vector2::ZERO, {2.0f, 14.0f});

  m_button = static_cast<UIButton*>(m_root->addChild(chMakeUnique<UIButton>()));
  m_button->setName("hud.button");
  placeCanvasChild(m_button, {1.0f, 1.0f}, {1.0f, 1.0f}, {-24.0f, -24.0f},
                   {190.0f, 48.0f});

  UITextBlock* buttonLabel =
      static_cast<UITextBlock*>(m_button->setContent(chMakeUnique<UITextBlock>()));
  buttonLabel->setName("hud.button.label");
  buttonLabel->setText("Click me");
  buttonLabel->setFontSize(18.0f);
  buttonLabel->setAlign(TextAlign::Center);
  buttonLabel->setMargin({8.0f, 13.0f, 8.0f, 0.0f});
  buttonLabel->setTextColor(LinearColor::WHITE);

  m_counter = static_cast<UITextBlock*>(m_root->addChild(chMakeUnique<UITextBlock>()));
  m_counter->setName("hud.counter");
  m_counter->setFontSize(16.0f);
  m_counter->setAlign(TextAlign::Center);
  m_counter->setTextColor({0.75f, 0.78f, 0.82f, 1.0f});
  placeCanvasChild(m_counter, {1.0f, 1.0f}, {1.0f, 1.0f}, {-24.0f, -84.0f},
                   {190.0f, 24.0f});
  refreshCounter();

  m_button->setOnClicked([this]() {
    ++m_clickCount;
    refreshCounter();
  });
}

/*
 */
void
UIHudWidget::refreshCounter()
{
  if (m_counter != nullptr) {
    m_counter->setText(StringUtils::format("Clicks: {0}", m_clickCount));
  }
}

} // namespace chEngineSDK
