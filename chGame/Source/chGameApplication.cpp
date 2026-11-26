/************************************************************************/
/**
 * @file chGameApplication.cpp
 * @author AccelMR
 * @date 2026/10/21
 * @brief
 *  Runtime that plays a project without the editor.
 */
/************************************************************************/
#include "chGameApplication.h"

#include "chAssetManager.h"
#include "chAudioManager.h"
#include "chCameraComponent.h"
#include "chConsoleVariable.h"
#include "chEnginePaths.h"
#include "chEnginePlugins.h"
#include "chEventDispatcherManager.h"
#include "chFont.h"
#include "chIGraphicsAPI.h"
#include "chInputManager.h"
#include "chISwapChain.h"
#include "chLinearColor.h"
#include "chLogger.h"
#include "chProfiler.h"
#include "chScene.h"
#include "chSceneAsset.h"
#include "chSceneManager.h"
#include "chScriptManager.h"
#include "chSceneRenderer.h"
#include "chUIDocumentAsset.h"
#include "chUIHudWidget.h"
#include "chUIRenderer.h"
#include "chUIUserWidget.h"
#include "chUIViewport.h"
#include "chUUID.h"

namespace chEngineSDK {

CH_LOG_DECLARE_STATIC(GameApp, All);

namespace {
ConsoleVariable<String> g_cvarStartupScene(
    "Game.StartupScene",
    "/Game/Maps/Main",
    "Scene the game opens: a virtual path such as /Game/Maps/Main, or a scene name.",
    "Scene");

ConsoleVariable<String> g_cvarHudDocument(
    "Game.HudDocument",
    "",
    "UI document drawn over the scene: a virtual path such as /Game/UI/Hud, or a document "
    "name. Empty uses the sample HUD built in code.",
    "UI");

// Where the view starts when the scene has no camera component.
const Vector3 kDefaultCameraPosition(-5.0f, 0.0f, 0.0f);

// The example projects keep their gameplay actions in the default context. The HUD turns
// it off while it owns the input, so scripts do not read the click, wheel or keys the UI
// consumed.
constexpr StringView kGameplayContext = InputManager::DEFAULT_CONTEXT;
} // namespace

/*
 */
GameApplication::GameApplication() = default;

/*
 */
GameApplication::~GameApplication() = default;

/*
 */
void
GameApplication::onPostInitialize()
{
  // Before the managers: plugins register the components scenes hold and the script
  // bindings the state is made with.
  EnginePlugins::loadEnabled();

  AudioManager::startUp();
  AssetManager::startUp();
  AssetManager& assetManager = AssetManager::instance();
  assetManager.initialize();
  assetManager.lazyLoadAssetsFromDirectory(EnginePaths::getGameAssetDirectory());
  ScriptManager::startUp();
  // Drawing its atlas takes tens of milliseconds, which belong in the load, not in the
  // first frame that shows a text.
  CH_PARAMETER_UNUSED(Font::getDefault());

  SceneManager::startUp();
  m_sceneRenderer = chMakeUnique<SceneRenderer>();

  m_uiRenderer = chMakeUnique<UIRenderer>();
  m_uiViewport = chMakeUnique<UIViewport>();
  updateUiSize();

  // A project replaces the sample HUD by naming its document in Config/Game.ini. A missing
  // or broken document falls back to the sample, so a game still opens with a usable UI.
  UniquePtr<UIUserWidget> hud;
  const String& hudDocument = g_cvarHudDocument.get();
  if (!hudDocument.empty()) {
    const SPtr<IAsset> asset = AssetManager::instance().findAsset(
        AssetTypeTraits<UIDocumentAsset>::getTypeId(), hudDocument);
    const SPtr<UIDocumentAsset> document = asset ? asset->as<UIDocumentAsset>() : nullptr;
    if (document && AssetManager::instance().syncLoadAsset(document)) {
      hud = document->createUserWidget();
    }
    if (!hud) {
      CH_LOG_ERROR(GameApp,
                   "Could not open the HUD document {0}. Set Game.HudDocument in the "
                   "project's Config/Game.ini. Using the sample HUD.",
                   hudDocument);
    }
  }
  if (!hud) {
    hud = chMakeUnique<UIHudWidget>();
  }
  m_hud = m_uiViewport->addUserWidget(std::move(hud));
  CH_ASSERT(m_hud != nullptr);

  // The platform only sends typed characters while text input is enabled; the viewport
  // decides whether the focused widget is a field and the frame loop toggles the state.
  m_textInputEvent = EventDispatcherManager::instance().OnTextInput.connect(
      [this](StringView text) {
        if (m_uiViewport) {
          m_uiViewport->processTextInput(text);
        }
      });

  const ISwapChain& swapChain = *getSwapChain();
  m_camera = Camera(kDefaultCameraPosition, Vector3::ZERO,
                    static_cast<float>(swapChain.getWidth()),
                    static_cast<float>(swapChain.getHeight()));
  openStartupScene();
}

/*
 */
void
GameApplication::openStartupScene()
{
  const String& sceneName = g_cvarStartupScene.get();
  SceneManager& sceneManager = SceneManager::instance();
  if (const SPtr<SceneAsset> asset = AssetManager::instance().findScene(sceneName)) {
    if (const SPtr<Scene> scene = sceneManager.openMap(asset, true).lock()) {
      m_scene = scene;
      CH_LOG_INFO(GameApp, "Playing scene {0}.", sceneName);
      return;
    }
  }

  CH_LOG_ERROR(GameApp,
               "Could not open the startup scene {0} in {1}. Set Game.StartupScene in the "
               "project's Config/Game.ini or pass -Scene=<path>. Showing an empty scene.",
               sceneName, EnginePaths::getProjectDirectory());
  m_scene = chMakeShared<Scene>("Empty", UUID::createRandom());
  m_scene->setPlaying(true);
  // Kept by the manager, so a script could still open another map from it.
  if (!sceneManager.addScene(m_scene) || !sceneManager.setActiveScene(m_scene)) {
    CH_LOG_WARNING(GameApp, "Scripts of the empty scene cannot open other maps.");
  }
}

/*
 */
void
GameApplication::updateUiSize()
{
  if (!m_uiRenderer || !m_uiViewport) {
    return;
  }

  const ISwapChain& swapChain = *getSwapChain();
  const Vector2 size(static_cast<float>(swapChain.getWidth()),
                     static_cast<float>(swapChain.getHeight()));
  if (m_uiRenderer->getViewportSize() == size) {
    return;
  }

  m_uiRenderer->setViewportSize(size);
  m_uiViewport->setViewportSize(size);
}

/*
 */
void
GameApplication::onInput()
{
  if (!m_uiViewport) {
    return;
  }

  // The pointer is processed before the actions are evaluated, so the HUD can turn the
  // gameplay context off before the scripts read it. Actions are processed in update(),
  // after updateActions.
  const InputState& state = InputManager::instance().getState();
  const UIViewport::InputResult result =
      m_uiViewport->processPointer(state.getMousePosition(),
                                   state.isDown(InputKey::MouseLeft),
                                   state.wasPressed(InputKey::MouseLeft),
                                   state.wasReleased(InputKey::MouseLeft),
                                   state.getMouseWheel());

  // Editing keys go to the focused field before the actions run, so an arrow moves the
  // caret instead of the focus. The platform text input follows the focus.
  const bool bTextEditingHandled = m_uiViewport->processTextEditingKeys(state);
  const bool bTextInputWanted = m_uiViewport->hasTextEditorFocus();
  if (InputManager::instance().isTextInputEnabled() != bTextInputWanted) {
    InputManager::instance().setTextInputEnabled(bTextInputWanted);
  }

  // A captured pointer covers the press and the drag, the click covers the release, a
  // handled wheel covers scrolling, bKeyboardCaptured covers typing in an editable box and
  // bTextEditingHandled covers the keys that edited it. Hover alone leaves gameplay alone,
  // as UMG's GameAndUI mode does.
  const bool bUiOwnsInput = result.bPointerCaptureActive || result.bPointerClicked ||
                            result.bWheelHandled || result.bKeyboardCaptured ||
                            bTextEditingHandled;
  if (bUiOwnsInput != m_bGameplayInputGated) {
    m_bGameplayInputGated = bUiOwnsInput;
    InputManager::instance().setContextEnabled(kGameplayContext, !bUiOwnsInput);
  }
}

/*
 */
void
GameApplication::destroyModules()
{
  if (IGraphicsAPI::isStarted()) {
    IGraphicsAPI::instance().waitIdle();
  }

  // The UI holds fonts and GPU sprites, so it goes before the asset and font shutdown below.
  m_textInputEvent.disconnect();
  if (InputManager::isStarted()) {
    InputManager::instance().setTextInputEnabled(false);
  }
  m_uiViewport.reset();
  m_uiRenderer.reset();
  m_hud = nullptr;
  if (m_bGameplayInputGated && InputManager::isStarted()) {
    InputManager::instance().setContextEnabled(kGameplayContext, true);
    m_bGameplayInputGated = false;
  }

  m_sceneRenderer.reset();
  m_scene.reset();
  if (SceneManager::isStarted()) {
    SceneManager::shutDown();
  }
  // Assets hold GPU buffers and textures, so they go while the graphics API still exists.
  if (AssetManager::isStarted()) {
    AssetManager::shutDown();
  }
  // After the assets: a scene asset owns its scene, whose script components run onDestroy
  // when it closes.
  if (ScriptManager::isStarted()) {
    ScriptManager::shutDown();
  }
  // Last: voices keep the clips they play alive, so nothing above needs it.
  if (AudioManager::isStarted()) {
    AudioManager::shutDown();
  }

  WindowedApplication::destroyModules();
}

/*
 */
void
GameApplication::fixedUpdate(const float deltaTime)
{
  SceneManager::instance().fixedUpdate(deltaTime);
}

/*
 */
void
GameApplication::update(const float deltaTime)
{
  const ISwapChain& swapChain = *getSwapChain();
  const float width = static_cast<float>(swapChain.getWidth());
  const float height = static_cast<float>(swapChain.getHeight());
  updateUiSize();
  if (m_uiViewport) {
    // The pointer was already routed in onInput(); the actions are read here, after
    // updateActions evaluated this frame's keys.
    m_uiViewport->processActions(InputManager::instance());
    m_uiViewport->tick(deltaTime);
  }
  // The game fills the window, so mouse positions are already in the game view.
  ScriptManager::instance().setGameView(Vector2::ZERO, Vector2(width, height));
  {
    CH_PROFILE_SCOPE("SceneUpdate");
    SceneManager& sceneManager = SceneManager::instance();
    sceneManager.update(deltaTime);
    // A script may have opened another map, which the update made active.
    SPtr<Scene> active = sceneManager.getActiveScene().lock();
    if (active && active != m_scene) {
      m_scene = std::move(active);
    }
  }
  {
    CH_PROFILE_SCOPE("UpdateTransforms");
    m_scene->updateTransforms();
  }

  // After the transforms, which the camera follows.
  if (const CameraComponent* camera = m_scene->getActiveCamera()) {
    camera->fillCamera(m_camera, width, height);
  }
  else {
    if (!m_bWarnedNoCamera) {
      CH_LOG_WARNING(GameApp, "The scene has no camera component; using a default view.");
      m_bWarnedNoCamera = true;
    }
    m_camera.setViewportSize(width, height);
  }
  AudioManager& audio = AudioManager::instance();
  audio.updateListener(m_camera, deltaTime);
  audio.update();

  m_sceneRenderer->extract(*m_scene, m_camera, LinearColor::BLACK);
}

/*
 */
void
GameApplication::onRender(ICommandList& commandList, const ISwapChain& swapChain)
{
  RenderGraph& graph = m_sceneRenderer->beginFrame();
  const RGTextureHandle backBuffer =
      graph.importTexture("BackBuffer", swapChain.getCurrentTexture(),
                          swapChain.getCurrentTextureView(), ResourceState::Undefined,
                          ResourceState::Present);
  m_sceneRenderer->addScenePasses(backBuffer);

  m_uiViewport->paint(m_uiRenderer->beginFrame());
  m_uiRenderer->addPass(graph, backBuffer);

  m_sceneRenderer->execute(commandList);
}

} // namespace chEngineSDK
