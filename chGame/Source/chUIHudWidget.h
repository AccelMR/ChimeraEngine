/************************************************************************/
/**
 * @file chUIHudWidget.h
 * @author Swampertor
 * @date 2026/11/23
 * @brief The sample HUD the game runtime draws over the scene.
 */
/************************************************************************/
#pragma once

#include "chUIUserWidget.h"

namespace chEngineSDK {
class UICanvasPanel;
class UITextBlock;
class UIButton;

/**
 * The HUD the runtime adds on top of the played scene. It builds its tree in code until UI
 * documents arrive in point 9: a title, a hint line, a crosshair and a button that counts
 * its clicks. The children are anchored, so the HUD follows the window size.
 *
 * This is also the integration sample for UIViewport: GameApplication owns the widget,
 * ticks it, paints it and feeds it input.
 */
class UIHudWidget final : public UIUserWidget
{
 public:
  UIHudWidget();
  ~UIHudWidget() override;

  NODISCARD FORCEINLINE int32
  getClickCount() const noexcept
  {
    return m_clickCount;
  }

  /**
   * The root canvas, for tests and for code that wants to add to the HUD.
   */
  NODISCARD FORCEINLINE UICanvasPanel*
  getRootPanel() const noexcept
  {
    return m_root;
  }

  void
  onInitialized() override;

 private:
  void
  refreshCounter();

  UICanvasPanel* m_root = nullptr;
  UITextBlock* m_counter = nullptr;
  UIButton* m_button = nullptr;
  int32 m_clickCount = 0;
};

} // namespace chEngineSDK
