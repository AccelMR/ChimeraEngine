/************************************************************************/
/**
 * @file chSpriteAtlasEditorUI.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief Edits the frames and animations of a sprite atlas.
 */
/************************************************************************/
#include "chSpriteAtlasEditorUI.h"

#include "chAssetDragDrop.h"
#include "chAssetManager.h"
#include "chAssetThumbnails.h"
#include "chEditorScene.h"
#include "chGameObject.h"
#include "chITexture.h"
#include "chImGuiRenderer.h"
#include "chLogger.h"
#include "chMath.h"
#include "chScene.h"
#include "chSpriteAnimatorComponent.h"
#include "chSpriteAtlasAsset.h"
#include "chSpriteComponent.h"
#include "chStringUtils.h"
#include "chTextureAsset.h"

#include "imgui.h"

#include "../../Content/Fonts/IconsFontAwsome5.h"

namespace chEngineSDK {
CH_LOG_DECLARE_STATIC(SpriteAtlasEditorLog, All);

namespace {
constexpr uint32 kNone = SpriteAtlas::NOT_FOUND;
constexpr float kListHeight = 180.0f;
constexpr float kMaxImageHeight = 420.0f;

// In the order of the Method combo.
enum class FrameMethod : int32
{
  Grid,
  CellSize,
  FindSprites
};
constexpr const ANSICHAR* kMethodNames[] = {"Grid", "Cell Size", "Find Sprites"};

struct EditorState
{
  WeakPtr<SpriteAtlasAsset> asset;
  UUID assetId = UUID::null();
  // The copy being edited; it reaches the asset only while valid.
  SpriteAtlas atlas;
  SPtr<TextureAsset> texture;
  String error;
  bool bValid = true;
  bool bNeedsSave = false;
  bool bTextureChanged = false;
  uint32 selectedFrame = kNone;
  uint32 selectedAnimation = kNone;
  float zoom = 1.0f;

  FrameMethod method = FrameMethod::Grid;
  int32 columns = 4;
  int32 rows = 1;
  int32 cellWidth = 32;
  int32 cellHeight = 32;
  int32 margin = 0;
  int32 spacing = 0;
  int32 alphaThreshold = 0;
  int32 padding = 0;
  ANSICHAR prefix[64] = "frame";
  // Made by Generate, waiting for the user to accept replacing the frames.
  Vector<SpriteAtlasFrame> pendingFrames;

  ANSICHAR frameName[SpriteAtlas::MAX_NAME_LENGTH] = "";
  ANSICHAR animationName[SpriteAtlas::MAX_NAME_LENGTH] = "";
  uint32 fillFirst = 0;
  int32 fillCount = 4;
  float fillRate = 10.0f;
  float previewTime = 0.0f;
  bool bPreviewPlaying = true;
};

EditorState&
getState()
{
  static EditorState state;
  return state;
}

/*
 * name when no entry uses it (ignoring case), else name followed by the first free number.
 */
template<typename Entries>
String
makeUniqueName(const Entries& entries, const StringView name)
{
  String candidate(name);
  for (uint32 suffix = 1;; ++suffix) {
    bool bTaken = false;
    for (const auto& entry : entries) {
      bTaken |= StringUtils::equalsIgnoreCase(entry.name, candidate);
    }
    if (!bTaken) {
      return candidate;
    }
    candidate = StringUtils::format("{0}_{1}", name, suffix);
  }
}

void
load(EditorState& state, const SPtr<SpriteAtlasAsset>& asset)
{
  state.asset = asset;
  state.assetId = asset->getUUID();
  state.atlas = asset->getAtlas();
  state.texture = asset->getTexture();
  state.error.clear();
  state.bValid = state.atlas.validate(state.error);
  state.bNeedsSave = false;
  state.bTextureChanged = false;
  state.selectedFrame = state.atlas.getFrames().empty() ? kNone : 0;
  state.selectedAnimation = state.atlas.getAnimations().empty() ? kNone : 0;
  state.pendingFrames.clear();
  state.previewTime = 0.0f;
  StringUtils::copyToBuffer(state.prefix,
                            state.texture ? StringView(state.texture->getName()) : "frame");
}

/*
 * Sprites and animators of the open scene that show the atlas read it again.
 */
void
refreshUsers(const SpriteAtlasAsset& asset)
{
  const SPtr<Scene>& scene = EditorScene::getScene();
  if (!scene) {
    return;
  }
  const auto visit = [&asset](const auto& self, GameObject& gameObject) -> void {
    if (SpriteComponent* sprite = gameObject.getComponent<SpriteComponent>()) {
      if (sprite->getAtlas().get() == &asset) {
        sprite->refreshAtlasFrame();
      }
    }
    SpriteAnimatorComponent* animator = gameObject.getComponent<SpriteAnimatorComponent>();
    if (animator) {
      if (animator->getAtlas().get() == &asset) {
        animator->refreshAtlas();
      }
    }
    for (const SPtr<GameObject>& child : gameObject.getChildren()) {
      self(self, *child);
    }
  };
  for (const SPtr<GameObject>& root : scene->getRootGameObjects()) {
    visit(visit, *root);
  }
}

/*
 * Checks the copy and, when it is valid, gives it to the asset.
 */
void
apply(EditorState& state, SpriteAtlasAsset& asset)
{
  state.error.clear();
  state.bValid = state.atlas.validate(state.error);
  if (!state.bValid) {
    return;
  }
  asset.setAtlas(state.atlas, state.texture);
  refreshUsers(asset);
  state.bNeedsSave = true;
}

/*
 * Steps that show frame go, and the steps of later frames move down by one.
 */
void
removeFrame(EditorState& state, uint32 frame)
{
  Vector<SpriteAtlasFrame>& frames = state.atlas.getFrames();
  frames.erase(frames.begin() + frame);
  for (SpriteAnimation& animation : state.atlas.getAnimations()) {
    Vector<SpriteAnimationFrame>& steps = animation.frames;
    for (SIZE_T step = steps.size(); step-- > 0;) {
      if (steps[step].frame == frame) {
        steps.erase(steps.begin() + static_cast<std::ptrdiff_t>(step));
      }
      else if (steps[step].frame > frame) {
        --steps[step].frame;
      }
    }
  }
  if (state.selectedFrame != kNone && state.selectedFrame >= frames.size()) {
    state.selectedFrame = frames.empty() ? kNone : static_cast<uint32>(frames.size() - 1);
  }
}

/*
 * The new frames replace the old ones; steps past the new frames go.
 */
void
replaceFrames(EditorState& state, Vector<SpriteAtlasFrame> frames)
{
  state.atlas.getFrames() = std::move(frames);
  const SIZE_T count = state.atlas.getFrames().size();
  for (SpriteAnimation& animation : state.atlas.getAnimations()) {
    Vector<SpriteAnimationFrame>& steps = animation.frames;
    for (SIZE_T step = steps.size(); step-- > 0;) {
      if (steps[step].frame >= count) {
        steps.erase(steps.begin() + static_cast<std::ptrdiff_t>(step));
      }
    }
  }
  state.selectedFrame = count > 0 ? 0 : kNone;
}

bool
renderTextureSlot(EditorState& state)
{
  bool bChanged = false;
  const ITexture* gpuTexture = state.texture ? state.texture->getTexture().get() : nullptr;
  const uint64 textureId = gpuTexture ? ImGuiRenderer::getTextureId(*gpuTexture) : 0;
  constexpr float kSlotSize = 48.0f;
  if (textureId != 0) {
    ImGui::Image(static_cast<ImTextureID>(textureId), ImVec2(kSlotSize, kSlotSize));
  }
  else {
    ImGui::Button("##noAtlasTexture", ImVec2(kSlotSize, kSlotSize));
  }
  const auto acceptDrop = [&]() {
    if (!ImGui::BeginDragDropTarget()) {
      return;
    }
    if (SPtr<TextureAsset> dropped = AssetDragDrop::acceptTexture()) {
      state.texture = dropped;
      state.atlas.setTextureId(dropped->getUUID());
      state.bTextureChanged = true;
      bChanged = true;
    }
    ImGui::EndDragDropTarget();
  };
  acceptDrop();
  ImGui::SameLine();
  ImGui::BeginGroup();
  ImGui::TextUnformatted("Texture");
  if (state.texture) {
    ImGui::TextDisabled("%s (%u x %u)", state.texture->getName(), state.texture->getWidth(),
                        state.texture->getHeight());
  }
  else if (!state.atlas.getTextureId().isNull()) {
    ImGui::TextDisabled("Missing texture");
  }
  else {
    ImGui::TextDisabled("None (drop a texture)");
  }
  ImGui::EndGroup();
  acceptDrop();
  return bChanged;
}

/*
 * The texture with every frame drawn over it; a click picks the frame under the mouse.
 */
void
renderImage(EditorState& state)
{
  const ITexture* gpuTexture = state.texture ? state.texture->getTexture().get() : nullptr;
  const uint64 textureId = gpuTexture ? ImGuiRenderer::getTextureId(*gpuTexture) : 0;
  if (textureId == 0) {
    return;
  }
  const float textureWidth = static_cast<float>(gpuTexture->getWidth());
  const float textureHeight = static_cast<float>(gpuTexture->getHeight());
  ImGui::SliderFloat("Zoom", &state.zoom, 0.25f, 16.0f, "%.2fx",
                     ImGuiSliderFlags_Logarithmic);
  const float fit = Math::max(ImGui::GetContentRegionAvail().x - 4.0f, 16.0f) / textureWidth;
  const float scale = fit * state.zoom;
  const ImVec2 imageSize(textureWidth * scale, textureHeight * scale);
  const float childHeight =
      Math::min(imageSize.y, kMaxImageHeight) + ImGui::GetStyle().ScrollbarSize + 4.0f;
  if (!ImGui::BeginChild("##atlasImage", ImVec2(0.0f, childHeight), ImGuiChildFlags_Borders,
                         ImGuiWindowFlags_HorizontalScrollbar)) {
    ImGui::EndChild();
    return;
  }
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  ImGui::Image(static_cast<ImTextureID>(textureId), imageSize);
  const bool bHovered = ImGui::IsItemHovered();
  const bool bClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

  ImDrawList* drawList = ImGui::GetWindowDrawList();
  const Vector<SpriteAtlasFrame>& frames = state.atlas.getFrames();
  const ImU32 frameColor = IM_COL32(255, 255, 255, 140);
  const ImU32 selectedColor = IM_COL32(255, 210, 40, 255);
  const auto toScreen = [&](const Vector2& pixel) {
    return ImVec2(origin.x + pixel.x * scale, origin.y + pixel.y * scale);
  };
  for (uint32 i = 0; i < frames.size(); ++i) {
    if (i != state.selectedFrame) {
      drawList->AddRect(toScreen(frames[i].position),
                        toScreen(frames[i].position + frames[i].size), frameColor);
    }
  }
  if (state.selectedFrame < frames.size()) {
    const SpriteAtlasFrame& frame = frames[state.selectedFrame];
    drawList->AddRect(toScreen(frame.position), toScreen(frame.position + frame.size),
                      selectedColor, 0.0f, 0, 2.0f);
    // The pivot, where the object stands.
    const ImVec2 pivot = toScreen(
        Vector2(frame.position.x + frame.size.x * frame.pivot.x,
                frame.position.y + frame.size.y * (1.0f - frame.pivot.y)));
    drawList->AddCircleFilled(pivot, 3.0f, selectedColor);
  }

  if (bHovered) {
    const ImVec2 mouse = ImGui::GetMousePos();
    const Vector2 pixel((mouse.x - origin.x) / scale, (mouse.y - origin.y) / scale);
    uint32 hovered = kNone;
    for (uint32 i = static_cast<uint32>(frames.size()); i-- > 0;) {
      const SpriteAtlasFrame& frame = frames[i];
      if (pixel.x >= frame.position.x && pixel.y >= frame.position.y &&
          pixel.x < frame.position.x + frame.size.x &&
          pixel.y < frame.position.y + frame.size.y) {
        hovered = i;
        break;
      }
    }
    if (hovered != kNone) {
      ImGui::SetTooltip("%s", frames[hovered].name.c_str());
      if (bClicked) {
        state.selectedFrame = hovered;
      }
    }
  }
  ImGui::EndChild();
}

bool
renderGenerators(EditorState& state)
{
  if (!ImGui::CollapsingHeader(ICON_FA_MAGIC "  Frames From the Image###AtlasGenerate")) {
    return false;
  }
  int32 method = static_cast<int32>(state.method);
  if (ImGui::Combo("Method", &method, kMethodNames, IM_ARRAYSIZE(kMethodNames))) {
    state.method = static_cast<FrameMethod>(method);
  }
  const Span<const uint8> pixels =
      state.texture ? state.texture->getRgba8Pixels() : Span<const uint8>();
  switch (state.method) {
    case FrameMethod::Grid:
      ImGui::DragInt("Columns", &state.columns, 0.1f, 1, 4096, "%d",
                     ImGuiSliderFlags_AlwaysClamp);
      ImGui::DragInt("Rows", &state.rows, 0.1f, 1, 4096, "%d", ImGuiSliderFlags_AlwaysClamp);
      break;
    case FrameMethod::CellSize:
      ImGui::DragInt("Cell Width", &state.cellWidth, 0.2f, 1, 16384, "%d px",
                     ImGuiSliderFlags_AlwaysClamp);
      ImGui::DragInt("Cell Height", &state.cellHeight, 0.2f, 1, 16384, "%d px",
                     ImGuiSliderFlags_AlwaysClamp);
      break;
    case FrameMethod::FindSprites:
      ImGui::SliderInt("Alpha Threshold", &state.alphaThreshold, 0, 254);
      ImGui::SetItemTooltip("Pixels with an alpha above this are part of a sprite");
      ImGui::DragInt("Padding", &state.padding, 0.1f, 0, 64, "%d px",
                     ImGuiSliderFlags_AlwaysClamp);
      if (pixels.empty()) {
        ImGui::TextDisabled("Needs an 8-bit RGBA texture with its pixels in memory.");
      }
      break;
  }
  if (state.method != FrameMethod::FindSprites) {
    ImGui::DragInt("Margin", &state.margin, 0.1f, 0, 4096, "%d px",
                   ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetItemTooltip("Empty pixels around the whole sheet");
    ImGui::DragInt("Spacing", &state.spacing, 0.1f, 0, 4096, "%d px",
                   ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetItemTooltip("Empty pixels between cells");
  }
  ImGui::InputText("Name Prefix", state.prefix, sizeof(state.prefix));

  const bool bCanGenerate =
      state.texture && state.texture->getWidth() > 0 &&
      (state.method != FrameMethod::FindSprites || !pixels.empty());
  ImGui::BeginDisabled(!bCanGenerate);
  bool bChanged = false;
  if (ImGui::Button(ICON_FA_MAGIC "  Generate")) {
    const uint32 width = state.texture->getWidth();
    const uint32 height = state.texture->getHeight();
    const uint32 margin = static_cast<uint32>(state.margin);
    const uint32 spacing = static_cast<uint32>(state.spacing);
    switch (state.method) {
      case FrameMethod::Grid:
        SpriteAtlas::makeGridFrames(width, height, static_cast<uint32>(state.columns),
                                    static_cast<uint32>(state.rows), margin, spacing,
                                    state.prefix, state.pendingFrames);
        break;
      case FrameMethod::CellSize:
        SpriteAtlas::makeCellFrames(width, height, static_cast<uint32>(state.cellWidth),
                                    static_cast<uint32>(state.cellHeight), margin, spacing,
                                    state.prefix, state.pendingFrames);
        break;
      case FrameMethod::FindSprites:
        SpriteAtlas::detectFrames(pixels, width, height,
                                  static_cast<uint8>(state.alphaThreshold),
                                  static_cast<uint32>(state.padding), state.prefix,
                                  state.pendingFrames);
        break;
    }
    if (state.pendingFrames.empty()) {
      CH_LOG_WARNING(SpriteAtlasEditorLog, "No frames fit those settings.");
    }
    else if (state.atlas.getFrames().empty()) {
      replaceFrames(state, std::move(state.pendingFrames));
      bChanged = true;
    }
    else {
      ImGui::OpenPopup("Replace Frames?");
    }
  }
  ImGui::EndDisabled();

  if (ImGui::BeginPopupModal("Replace Frames?", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Replace the %u frames with %u new ones?",
                static_cast<uint32>(state.atlas.getFrames().size()),
                static_cast<uint32>(state.pendingFrames.size()));
    ImGui::TextDisabled("Animation steps past the new frames are removed.");
    if (ImGui::Button("Replace")) {
      replaceFrames(state, std::move(state.pendingFrames));
      state.pendingFrames.clear();
      bChanged = true;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      state.pendingFrames.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  return bChanged;
}

bool
renderFrames(EditorState& state, const SpriteAtlasAsset& asset)
{
  Vector<SpriteAtlasFrame>& frames = state.atlas.getFrames();
  const String header = StringUtils::format(ICON_FA_TH "  Frames ({0})###AtlasFrames",
                                            static_cast<uint32>(frames.size()));
  if (!ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
    return false;
  }
  bool bChanged = false;
  const float rowHeight = ImGui::GetTextLineHeightWithSpacing();
  const float rows = static_cast<float>(Math::max(frames.size(), static_cast<SIZE_T>(1)));
  const float listHeight =
      Math::min(kListHeight, rowHeight * rows + ImGui::GetStyle().FramePadding.y * 2.0f);
  if (ImGui::BeginChild("##frameList", ImVec2(0.0f, listHeight), ImGuiChildFlags_Borders)) {
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int32>(frames.size()));
    while (clipper.Step()) {
      for (int32 row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
        const uint32 i = static_cast<uint32>(row);
        ImGui::PushID(row);
        if (ImGui::Selectable(frames[i].name.c_str(), i == state.selectedFrame)) {
          state.selectedFrame = i;
        }
        if (ImGui::BeginDragDropSource()) {
          AssetDragDrop::setSpriteFramePayload(asset, i);
          ImGui::TextUnformatted(frames[i].name.c_str());
          ImGui::EndDragDropSource();
        }
        ImGui::SetItemTooltip("Drag it onto a sprite in the Object Inspector");
        ImGui::PopID();
      }
    }
  }
  ImGui::EndChild();

  if (ImGui::Button(ICON_FA_PLUS "  Frame")) {
    SpriteAtlasFrame frame;
    if (state.selectedFrame < frames.size()) {
      frame = frames[state.selectedFrame];
    }
    else if (state.texture) {
      frame.size = Vector2(static_cast<float>(state.texture->getWidth()),
                           static_cast<float>(state.texture->getHeight()));
    }
    frame.name = makeUniqueName(frames, state.prefix);
    frames.push_back(std::move(frame));
    state.selectedFrame = static_cast<uint32>(frames.size() - 1);
    bChanged = true;
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(state.selectedFrame >= frames.size());
  if (ImGui::Button(ICON_FA_TRASH "  Delete Frame")) {
    removeFrame(state, state.selectedFrame);
    bChanged = true;
  }
  ImGui::EndDisabled();

  if (state.selectedFrame >= frames.size()) {
    return bChanged;
  }
  SpriteAtlasFrame& frame = frames[state.selectedFrame];
  ImGui::PushID("selectedFrame");
  StringUtils::copyToBuffer(state.frameName, frame.name);
  ImGui::InputText("Name", state.frameName, sizeof(state.frameName));
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    frame.name = state.frameName;
    bChanged = true;
  }
  bChanged |= ImGui::DragFloat2("Position", &frame.position.x, 1.0f, 0.0f, 100000.0f, "%.0f");
  bChanged |= ImGui::DragFloat2("Size", &frame.size.x, 1.0f, 1.0f, 100000.0f, "%.0f");
  bChanged |= ImGui::DragFloat2("Pivot", &frame.pivot.x, 0.01f, -10.0f, 10.0f, "%.2f");
  ImGui::SetItemTooltip("(0, 0) is the bottom left corner, (1, 1) the top right one");
  ImGui::PopID();
  return bChanged;
}

/*
 * The selected animation's steps, a helper that fills them from a run of frames, and a
 * playing preview.
 */
bool
renderAnimationDetails(EditorState& state, const SpriteAtlasAsset& asset)
{
  SpriteAnimation& animation = state.atlas.getAnimations()[state.selectedAnimation];
  const Vector<SpriteAtlasFrame>& frames = state.atlas.getFrames();
  bool bChanged = false;
  ImGui::PushID("selectedAnimation");
  StringUtils::copyToBuffer(state.animationName, animation.name);
  ImGui::InputText("Name", state.animationName, sizeof(state.animationName));
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    animation.name = state.animationName;
    bChanged = true;
  }
  bChanged |= ImGui::Checkbox("Loop", &animation.bLoop);
  ImGui::SameLine();
  ImGui::TextDisabled("%.2f s, %u steps", animation.getDuration(),
                      static_cast<uint32>(animation.frames.size()));

  SIZE_T moveUp = animation.frames.size();
  SIZE_T moveDown = animation.frames.size();
  SIZE_T remove = animation.frames.size();
  for (SIZE_T step = 0; step < animation.frames.size(); ++step) {
    SpriteAnimationFrame& entry = animation.frames[step];
    ImGui::PushID(static_cast<int32>(step));
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
    bChanged |= SpriteAtlasEditorUI::pickFrame("##frame", asset, entry.frame);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
    bChanged |= ImGui::DragFloat("##duration", &entry.duration, 0.005f, 0.001f, 60.0f,
                                 "%.3f s", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_FA_ARROW_UP)) {
      moveUp = step;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_FA_ARROW_DOWN)) {
      moveDown = step;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_FA_TIMES)) {
      remove = step;
    }
    ImGui::PopID();
  }
  Vector<SpriteAnimationFrame>& steps = animation.frames;
  if (moveUp > 0 && moveUp < steps.size()) {
    std::swap(steps[moveUp], steps[moveUp - 1]);
    bChanged = true;
  }
  if (moveDown + 1 < steps.size()) {
    std::swap(steps[moveDown], steps[moveDown + 1]);
    bChanged = true;
  }
  if (remove < steps.size()) {
    steps.erase(steps.begin() + static_cast<std::ptrdiff_t>(remove));
    bChanged = true;
  }

  ImGui::BeginDisabled(frames.empty());
  if (ImGui::Button(ICON_FA_PLUS "  Step")) {
    uint32 frame = state.selectedFrame < frames.size() ? state.selectedFrame : 0;
    if (!steps.empty()) {
      frame = Math::min(steps.back().frame + 1, static_cast<uint32>(frames.size() - 1));
    }
    const float duration = steps.empty() ? 0.1f : steps.back().duration;
    steps.push_back({.frame = frame, .duration = duration});
    bChanged = true;
  }
  ImGui::EndDisabled();

  if (!frames.empty() && ImGui::TreeNode("Fill From Frames")) {
    state.fillFirst = Math::min(state.fillFirst, static_cast<uint32>(frames.size() - 1));
    SpriteAtlasEditorUI::pickFrame("First Frame", asset, state.fillFirst);
    ImGui::DragInt("Count", &state.fillCount, 0.1f, 1, 4096, "%d",
                   ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Frames Per Second", &state.fillRate, 0.1f, 0.1f, 240.0f, "%.1f",
                     ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::Button("Replace Steps")) {
      const uint32 last =
          Math::min(state.fillFirst + static_cast<uint32>(state.fillCount),
                    static_cast<uint32>(frames.size()));
      steps.clear();
      for (uint32 frame = state.fillFirst; frame < last; ++frame) {
        steps.push_back({.frame = frame, .duration = 1.0f / state.fillRate});
      }
      bChanged = true;
    }
    ImGui::TreePop();
  }

  // Preview of what the animation shows now.
  const float duration = animation.getDuration();
  if (!steps.empty() && duration > 0.0f) {
    if (ImGui::SmallButton(state.bPreviewPlaying ? ICON_FA_PAUSE : ICON_FA_PLAY)) {
      state.bPreviewPlaying = !state.bPreviewPlaying;
    }
    if (state.bPreviewPlaying) {
      state.previewTime += ImGui::GetIO().DeltaTime;
      if (!animation.bLoop && state.previewTime > duration + 0.5f) {
        state.previewTime = 0.0f;
      }
    }
    const SpriteAnimationFrame& shown = steps[animation.getStepAt(state.previewTime)];
    SpriteAtlasEditorUI::drawFrame(asset, shown.frame, 128.0f, 128.0f);
  }
  ImGui::PopID();
  return bChanged;
}

bool
renderAnimations(EditorState& state, const SpriteAtlasAsset& asset)
{
  Vector<SpriteAnimation>& animations = state.atlas.getAnimations();
  const String header = StringUtils::format(ICON_FA_FILM "  Animations ({0})###AtlasAnims",
                                            static_cast<uint32>(animations.size()));
  if (!ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
    return false;
  }
  bool bChanged = false;
  for (uint32 i = 0; i < animations.size(); ++i) {
    ImGui::PushID(static_cast<int32>(i));
    if (ImGui::Selectable(animations[i].name.c_str(), i == state.selectedAnimation)) {
      state.selectedAnimation = i;
      state.previewTime = 0.0f;
    }
    ImGui::PopID();
  }
  if (ImGui::Button(ICON_FA_PLUS "  Animation")) {
    SpriteAnimation animation{.name = makeUniqueName(animations, "Animation")};
    if (state.selectedFrame < state.atlas.getFrames().size()) {
      animation.frames.push_back({.frame = state.selectedFrame, .duration = 0.1f});
    }
    animations.push_back(std::move(animation));
    state.selectedAnimation = static_cast<uint32>(animations.size() - 1);
    bChanged = true;
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(state.selectedAnimation >= animations.size());
  if (ImGui::Button(ICON_FA_TRASH "  Delete Animation")) {
    animations.erase(animations.begin() + state.selectedAnimation);
    state.selectedAnimation = kNone;
    bChanged = true;
  }
  ImGui::EndDisabled();

  if (state.selectedAnimation < animations.size()) {
    ImGui::Separator();
    bChanged |= renderAnimationDetails(state, asset);
  }
  return bChanged;
}
} // namespace

/*
 */
void
SpriteAtlasEditorUI::render(const SPtr<SpriteAtlasAsset>& asset)
{
  EditorState& state = getState();
  if (state.assetId != asset->getUUID() || state.asset.lock() != asset) {
    load(state, asset);
  }

  bool bChanged = renderTextureSlot(state);
  if (!state.bValid) {
    ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Not saved: %s", state.error.c_str());
  }
  renderImage(state);
  bChanged |= renderGenerators(state);
  bChanged |= renderFrames(state, *asset);
  bChanged |= renderAnimations(state, *asset);
  if (bChanged) {
    apply(state, *asset);
  }

  // A drag or a typed name is saved once, when it ends.
  if (state.bNeedsSave && !ImGui::IsAnyItemActive()) {
    state.bNeedsSave = false;
    if (!AssetManager::instance().saveAsset(asset)) {
      CH_LOG_ERROR(SpriteAtlasEditorLog, "Failed to save sprite atlas {0}", asset->getName());
    }
    if (state.bTextureChanged) {
      state.bTextureChanged = false;
      AssetThumbnails::regenerate(*asset);
    }
  }
}

/*
 */
void
SpriteAtlasEditorUI::drawFrame(const SpriteAtlasAsset& atlas,
                               uint32 frame,
                               float maxWidth,
                               float maxHeight)
{
  const Vector<SpriteAtlasFrame>& frames = atlas.getAtlas().getFrames();
  const SPtr<TextureAsset>& texture = atlas.getTexture();
  const ITexture* gpuTexture = texture ? texture->getTexture().get() : nullptr;
  const uint64 textureId = gpuTexture ? ImGuiRenderer::getTextureId(*gpuTexture) : 0;
  if (textureId == 0 || frame >= frames.size()) {
    return;
  }
  const SpriteAtlasFrame& shown = frames[frame];
  const float textureWidth = static_cast<float>(gpuTexture->getWidth());
  const float textureHeight = static_cast<float>(gpuTexture->getHeight());
  const float fit = Math::min(maxWidth / Math::max(shown.size.x, 1.0f),
                              maxHeight / Math::max(shown.size.y, 1.0f));
  ImGui::Image(static_cast<ImTextureID>(textureId),
               ImVec2(shown.size.x * fit, shown.size.y * fit),
               ImVec2(shown.position.x / textureWidth, shown.position.y / textureHeight),
               ImVec2((shown.position.x + shown.size.x) / textureWidth,
                      (shown.position.y + shown.size.y) / textureHeight));
}

/*
 */
bool
SpriteAtlasEditorUI::pickFrame(const ANSICHAR* label,
                               const SpriteAtlasAsset& atlas,
                               uint32& frame)
{
  const Vector<SpriteAtlasFrame>& frames = atlas.getAtlas().getFrames();
  const ANSICHAR* preview = frame < frames.size() ? frames[frame].name.c_str() : "(none)";
  if (!ImGui::BeginCombo(label, preview, ImGuiComboFlags_HeightLarge)) {
    return false;
  }
  bool bChanged = false;
  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int32>(frames.size()));
  while (clipper.Step()) {
    for (int32 row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
      const uint32 i = static_cast<uint32>(row);
      ImGui::PushID(row);
      if (ImGui::Selectable(frames[i].name.c_str(), i == frame)) {
        bChanged = i != frame;
        frame = i;
      }
      ImGui::PopID();
    }
  }
  ImGui::EndCombo();
  return bChanged;
}

/*
 */
bool
SpriteAtlasEditorUI::pickAnimation(const ANSICHAR* label,
                                   const SpriteAtlasAsset& atlas,
                                   String& animation)
{
  const ANSICHAR* preview = animation.empty() ? "(none)" : animation.c_str();
  if (!ImGui::BeginCombo(label, preview)) {
    return false;
  }
  bool bChanged = false;
  for (const SpriteAnimation& entry : atlas.getAtlas().getAnimations()) {
    if (ImGui::Selectable(entry.name.c_str(), entry.name == animation)) {
      bChanged = entry.name != animation;
      animation = entry.name;
    }
  }
  ImGui::EndCombo();
  return bChanged;
}

/*
 */
void
SpriteAtlasEditorUI::close(const IAsset& asset)
{
  EditorState& state = getState();
  if (state.assetId == asset.getUUID()) {
    state = EditorState();
  }
}

/*
 */
void
SpriteAtlasEditorUI::shutDown()
{
  getState() = EditorState();
}

} // namespace chEngineSDK
