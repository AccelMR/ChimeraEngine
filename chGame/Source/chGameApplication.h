/************************************************************************/
/**
 * @file chGameApplication.h
 * @author AccelMR
 * @date 2026/10/21
 * @brief
 *  Runtime that plays a project without the editor.
 */
/************************************************************************/
#pragma once

#include "chWindowedApplication.h"

#include "chCamera.h"

namespace chEngineSDK {
class Scene;
class SceneRenderer;
class UIRenderer;
class UIViewport;
class UIUserWidget;

/**
 * Exists to play a project without the editor: it opens the project's startup scene, runs
 * it and draws it through the scene's camera. It holds no game logic of its own, so the
 * same executable plays any project; a game brings its behavior in its scene and assets.
 */
class GameApplication : public WindowedApplication
{
 public:
  GameApplication();

  ~GameApplication() override;

 protected:
  void
  onPostInitialize() override;

  void
  destroyModules() override;

  void
  onInput() override;

  void
  fixedUpdate(const float deltaTime) override;

  void
  update(const float deltaTime) override;

  void
  onRender(ICommandList& commandList, const ISwapChain& swapChain) override;

  NODISCARD StringView
  getDefaultApplicationId() const override { return "chimera-game"; }

 private:
  void
  openStartupScene();

  void
  updateUiSize();

  UniquePtr<UIRenderer> m_uiRenderer;
  UniquePtr<UIViewport> m_uiViewport;
  // Owned by m_uiViewport; the sample HUD or the document Game.HudDocument names.
  UIUserWidget* m_hud = nullptr;
  // Platform text input while a UI text field has focus.
  HEvent m_textInputEvent;
  // True while the HUD owns the input and the gameplay context is turned off.
  bool m_bGameplayInputGated = false;

  UniquePtr<SceneRenderer> m_sceneRenderer;
  // The scene manager owns a loaded scene; this keeps the one being played, also the empty
  // scene used when the startup scene is missing, which the scene manager never sees.
  SPtr<Scene> m_scene;
  Camera m_camera;
  bool m_bWarnedNoCamera = false;
};

} // namespace chEngineSDK
