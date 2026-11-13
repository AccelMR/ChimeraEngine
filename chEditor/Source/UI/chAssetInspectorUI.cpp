/************************************************************************/
/**
 * @file chAssetInspectorUI.cpp
 * @author AccelMR
 * @date 2026/10/20
 * @brief Window that shows what an asset holds.
 */
/************************************************************************/
#include "chAssetInspectorUI.h"

#include "chAlgorithm.h"
#include "chAnimationClip.h"
#include "chAnimationClipAsset.h"
#include "chAnimationPreviewUI.h"
#include "chAssetDragDrop.h"
#include "chAssetManager.h"
#include "chConsoleVariable.h"
#include "chEngineConfig.h"
#include "chFont.h"
#include "chFontAsset.h"
#include "chITexture.h"
#include "chImGuiRenderer.h"
#include "chLogger.h"
#include "chMaterialAsset.h"
#include "chMaterialEditorUI.h"
#include "chMath.h"
#include "chMesh.h"
#include "chModel.h"
#include "chModelAsset.h"
#include "chRotator.h"
#include "chSkeleton.h"
#include "chSkeletonAsset.h"
#include "chSpriteAtlasAsset.h"
#include "chSpriteAtlasEditorUI.h"
#include "chStringUtils.h"
#include "chTextureAsset.h"
#include "chUIHelpers.h"

#include "imgui.h"

#include "../../Content/Fonts/IconsFontAwsome5.h"

namespace chEngineSDK {
CH_LOG_DECLARE_STATIC(AssetInspectorLog, All);

namespace {
ConsoleVariable<bool> g_cvarShowAssetInspector("Editor.ShowAssetInspector",
                                               true,
                                               "Shows the Asset Inspector window.",
                                               "ShowAssetInspector");

// Size the window gets the first time it opens; ImGui remembers it after.
constexpr float kWindowWidth = 420.0f;
constexpr float kWindowHeight = 560.0f;

struct InspectorState
{
  SPtr<IAsset> asset;
  bool bFocus = false;
  // Built when an asset opens, so drawing it every frame walks nothing else: the children
  // of each bone (skeletons only store parents) and the clips that fit a model.
  Vector<Vector<uint32>> boneChildren;
  Vector<SPtr<IAsset>> modelClips;
  uint32 modelVertices = 0;
  uint32 modelTriangles = 0;
};

InspectorState&
getState()
{
  static InspectorState state;
  return state;
}

/*
 * A row that can be dragged like the asset in the Content Browser, and opens it here on a
 * double click.
 */
void
renderAssetRow(const SPtr<IAsset>& asset)
{
  const chUIHelpers::AssetIcon icon = chUIHelpers::UIHelpers::getIconFromAssetType(asset);
  ImGui::PushID(asset.get());
  ImGui::Selectable("##row", false, ImGuiSelectableFlags_AllowDoubleClick);
  AssetDragDrop::source(*asset);
  if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    AssetInspectorUI::open(asset);
  }
  ImGui::SameLine(0.0f, 0.0f);
  ImGui::Text("%s  %s", icon.icon, asset->getName());
  ImGui::PopID();
}

/*
 */
void
renderBone(const Skeleton& skeleton, uint32 bone)
{
  const SkeletonBone& data = skeleton.getBones()[bone];
  const Vector<uint32>& children = getState().boneChildren[bone];
  ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
  if (children.empty()) {
    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
  }
  const bool bOpen =
      ImGui::TreeNodeEx(static_cast<const void*>(&data), flags, "%s", data.name.c_str());
  if (ImGui::BeginItemTooltip()) {
    const Rotator rotator = data.rotation.toRotator();
    ImGui::Text("Index %u", bone);
    ImGui::Text("Position %.3f, %.3f, %.3f", data.position.x, data.position.y,
                data.position.z);
    ImGui::Text("Rotation %.1f, %.1f, %.1f (roll, pitch, yaw)", rotator.roll.valueDegree(),
                rotator.pitch.valueDegree(), rotator.yaw.valueDegree());
    ImGui::Text("Scale %.3f, %.3f, %.3f", data.scale.x, data.scale.y, data.scale.z);
    ImGui::EndTooltip();
  }
  if (!bOpen || children.empty()) {
    return;
  }
  for (const uint32 child : children) {
    renderBone(skeleton, child);
  }
  ImGui::TreePop();
}

/*
 */
void
renderSkeleton(const Skeleton& skeleton)
{
  const Vector<SkeletonBone>& bones = skeleton.getBones();
  ImGui::Text(ICON_FA_BONE "  %u bones", skeleton.getBoneCount());
  if (!ImGui::TreeNodeEx(ICON_FA_SITEMAP "  Bones###Bones", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  for (uint32 bone = 0; bone < bones.size(); ++bone) {
    if (bones[bone].parent == SkeletonBone::NO_PARENT) {
      renderBone(skeleton, bone);
    }
  }
  ImGui::TreePop();
}

/*
 */
void
renderAnimationClip(const AnimationClip& clip)
{
  ImGui::Text("Duration: %.2f s", clip.getDuration());
  ImGui::Text("Keys: %u, %.1f per second", clip.getFrameCount(), clip.getSampleRate());
  if (!clip.getSkeletonId().isNull()) {
    const SPtr<IAsset> skeleton = AssetManager::instance().getAsset(clip.getSkeletonId());
    ImGui::Text(ICON_FA_BONE "  Skeleton: %s", skeleton ? skeleton->getName() : "(missing)");
  }

  const Vector<AnimationTrack>& tracks = clip.getTracks();
  if (!ImGui::TreeNodeEx("##tracks", ImGuiTreeNodeFlags_DefaultOpen, "Tracks (%u)",
                         static_cast<uint32>(tracks.size()))) {
    return;
  }
  ImGui::TextDisabled("Moving channels: P position, R rotation, S scale");
  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int32>(tracks.size()));
  while (clipper.Step()) {
    for (int32 i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
      const AnimationTrack& track = tracks[static_cast<SIZE_T>(i)];
      ImGui::Text("%s%s%s  %s", track.positionCount > 1 ? "P" : "-",
                  track.rotationCount > 1 ? "R" : "-", track.scaleCount > 1 ? "S" : "-",
                  track.nodeName.c_str());
    }
  }
  ImGui::TreePop();
}

/*
 */
void
renderTexture(const ITexture& texture)
{
  ImGui::Text("%u x %u, %u mips, format %u", texture.getWidth(), texture.getHeight(),
              texture.getMipLevels(), static_cast<uint32>(texture.getFormat()));
  const uint64 textureId = ImGuiRenderer::getTextureId(texture);
  if (textureId == 0) {
    return;
  }
  // As wide as the window, keeping the image's proportions.
  const float width = Math::max(ImGui::GetContentRegionAvail().x, 1.0f);
  const float aspect = static_cast<float>(texture.getHeight()) /
                       static_cast<float>(Math::max(texture.getWidth(), 1u));
  ImGui::Image(static_cast<ImTextureID>(textureId), ImVec2(width, width * aspect));
}

/*
 */
void
renderModel(const Model& model)
{
  const InspectorState& state = getState();
  ImGui::Text("Nodes: %u, meshes: %u", model.getNodeCount(), model.getMeshCount());
  ImGui::Text("Vertices: %u, triangles: %u", state.modelVertices, state.modelTriangles);
  if (model.hasSkeleton()) {
    const SPtr<SkeletonAsset>& skeleton = model.getSkeleton();
    if (skeleton) {
      ImGui::TextUnformatted("Skeleton:");
      renderAssetRow(skeleton);
    }
    else {
      ImGui::TextUnformatted(ICON_FA_BONE "  Skeleton: (missing)");
    }
  }

  if (ImGui::TreeNodeEx(ICON_FA_PALETTE "  Materials###Materials",
                        ImGuiTreeNodeFlags_DefaultOpen)) {
    for (const ModelMaterialSlot& slot : model.getMaterialSlots()) {
      if (slot.material) {
        renderAssetRow(slot.material);
      }
      else {
        ImGui::TextDisabled("%s (missing)", slot.name.c_str());
      }
    }
    ImGui::TreePop();
  }

  if (!state.modelClips.empty() &&
      ImGui::TreeNodeEx(ICON_FA_FILM "  Animations###Animations",
                        ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextDisabled("Drag one onto the model in the Object Inspector.");
    for (const SPtr<IAsset>& clip : state.modelClips) {
      renderAssetRow(clip);
    }
    ImGui::TreePop();
  }
}

/*
 * Builds what drawing the asset needs, once per open.
 */
void
prepare(InspectorState& state)
{
  state.boneChildren.clear();
  state.modelClips.clear();
  state.modelVertices = 0;
  state.modelTriangles = 0;

  if (const SPtr<SkeletonAsset> skeletonAsset = state.asset->as<SkeletonAsset>()) {
    if (const SPtr<Skeleton>& skeleton = skeletonAsset->getSkeleton()) {
      const Vector<SkeletonBone>& bones = skeleton->getBones();
      state.boneChildren.assign(bones.size(), {});
      for (uint32 bone = 0; bone < bones.size(); ++bone) {
        if (bones[bone].parent != SkeletonBone::NO_PARENT) {
          state.boneChildren[bones[bone].parent].push_back(bone);
        }
      }
    }
  }

  const SPtr<ModelAsset> modelAsset = state.asset->as<ModelAsset>();
  if (!modelAsset || !modelAsset->getModel()) {
    return;
  }
  const Model& model = *modelAsset->getModel();
  for (const SPtr<Mesh>& mesh : model.getMeshes()) {
    state.modelVertices += mesh->getVertexCount();
    state.modelTriangles += mesh->getIndexCount() / 3;
  }
  // Clips made for the model's skeleton, or for a model without one the clips next to it.
  AnimationPreviewUI::findClips(model.getSkeletonId(), modelAsset->getAssetPath(),
                                state.modelClips);
}

/**
 * The settings being edited for the font shown, applied only on Apply: making the font again
 * draws its glyphs again, which is too slow for every drag of a slider.
 */
struct FontEditState
{
  UUID asset = UUID::null();
  FontSettings settings;
  ANSICHAR preloadCharacters[1024] = "";
};

FontEditState&
getFontEditState()
{
  static FontEditState s_state;
  return s_state;
}

NODISCARD bool
sameSettings(const FontSettings& a, const FontSettings& b)
{
  return a.mode == b.mode && a.rasterSize == b.rasterSize && a.sdfSpread == b.sdfSpread &&
         a.bPreloadAscii == b.bPreloadAscii && a.bPreloadLatin1 == b.bPreloadLatin1 &&
         a.preloadCharacters == b.preloadCharacters;
}

/*
 */
void
renderFont(const SPtr<FontAsset>& asset)
{
  FontEditState& state = getFontEditState();
  if (state.asset != asset->getUUID()) {
    state.asset = asset->getUUID();
    state.settings = asset->getSettings();
    StringUtils::copyToBuffer(state.preloadCharacters, state.settings.preloadCharacters);
  }

  const SPtr<Font>& font = asset->getFont();
  if (font) {
    ImGui::Text("%u glyphs drawn, atlas %u x %u, file %u KB", font->getGlyphCount(),
                font->getAtlasWidth(), font->getAtlasHeight(),
                static_cast<uint32>(font->getFontData().size() / 1024));
  }
  else {
    ImGui::TextDisabled("The file is not a font.");
  }

  ImGui::SeparatorText("Glyphs");
  static constexpr const ANSICHAR* kModes[] = {"SDF (sharp at any size, outlines)",
                                               "Pixel (one size, point filter)"};
  int32 mode = static_cast<int32>(state.settings.mode);
  if (ImGui::Combo("Mode", &mode, kModes, 2)) {
    state.settings.mode = static_cast<FontRenderMode>(mode);
  }
  int32 rasterSize = static_cast<int32>(state.settings.rasterSize);
  if (ImGui::SliderInt("Raster Size", &rasterSize, FontSettings::MIN_RASTER_SIZE,
                       FontSettings::MAX_RASTER_SIZE)) {
    state.settings.rasterSize = static_cast<uint32>(rasterSize);
  }
  ImGui::SetItemTooltip("Pixels the glyphs are drawn at: a pixel font's own size, or the "
                        "detail of an SDF font (48 is plenty)");
  if (state.settings.mode == FontRenderMode::Sdf) {
    int32 spread = static_cast<int32>(state.settings.sdfSpread);
    if (ImGui::SliderInt("SDF Spread", &spread, 1, FontSettings::MAX_SDF_SPREAD)) {
      state.settings.sdfSpread = static_cast<uint32>(spread);
    }
    ImGui::SetItemTooltip("Pixels of distance around each glyph: the widest outline it can "
                          "draw");
  }
  ImGui::Checkbox("Preload ASCII", &state.settings.bPreloadAscii);
  ImGui::Checkbox("Preload Latin-1", &state.settings.bPreloadLatin1);
  if (ImGui::InputText("More Characters", state.preloadCharacters,
                       sizeof(state.preloadCharacters))) {
    state.settings.preloadCharacters = state.preloadCharacters;
  }
  ImGui::SetItemTooltip("Drawn when the font loads, so the first frame that shows them does "
                        "not have to. Any character is drawn when first used anyway.");

  const bool bChanged = !sameSettings(state.settings, asset->getSettings());
  ImGui::BeginDisabled(!bChanged);
  if (ImGui::Button(ICON_FA_CHECK "  Apply")) {
    asset->setSettings(state.settings);
    if (!AssetManager::instance().saveAsset(asset)) {
      CH_LOG_ERROR(AssetInspectorLog, "Could not save font {0}.", asset->getName());
    }
    // Read back, since the font keeps its values in range.
    state.asset = UUID::null();
  }
  ImGui::SameLine();
  if (ImGui::Button("Revert")) {
    state.asset = UUID::null();
  }
  ImGui::EndDisabled();

  if (!font) {
    return;
  }
  ImGui::SeparatorText("Atlas");
  ImGui::TextDisabled("One channel, shown in red; glyphs are added as texts use them.");
  CH_PARAMETER_UNUSED(font->prepareGpuAtlas());
  if (font->getAtlasTexture()) {
    renderTexture(*font->getAtlasTexture());
  }
}

/*
 */
void
renderAsset(const SPtr<IAsset>& asset)
{
  const chUIHelpers::AssetIcon icon = chUIHelpers::UIHelpers::getIconFromAssetType(asset);
  ImGui::Text("%s  %s", icon.icon, asset->getName());
  ImGui::TextDisabled("%s - %s", asset->getTypeName(), asset->getAssetPath());
  ImGui::Separator();

  if (!asset->isLoaded()) {
    ImGui::TextDisabled("The asset could not be loaded.");
    return;
  }
  if (AnimationPreviewUI::canOpen(*asset) &&
      ImGui::Button(ICON_FA_RUNNING "  Open in Animation Preview")) {
    AnimationPreviewUI::open(asset);
  }
  if (const SPtr<ModelAsset> model = asset->as<ModelAsset>()) {
    if (model->getModel()) {
      renderModel(*model->getModel());
    }
  }
  else if (const SPtr<TextureAsset> texture = asset->as<TextureAsset>()) {
    if (texture->getTexture()) {
      renderTexture(*texture->getTexture());
    }
  }
  else if (const SPtr<MaterialAsset> material = asset->as<MaterialAsset>()) {
    MaterialEditorUI::render(material);
  }
  else if (const SPtr<SkeletonAsset> skeleton = asset->as<SkeletonAsset>()) {
    if (skeleton->getSkeleton()) {
      renderSkeleton(*skeleton->getSkeleton());
    }
  }
  else if (const SPtr<AnimationClipAsset> clip = asset->as<AnimationClipAsset>()) {
    if (clip->getClip()) {
      renderAnimationClip(*clip->getClip());
    }
  }
  else if (const SPtr<SpriteAtlasAsset> atlas = asset->as<SpriteAtlasAsset>()) {
    SpriteAtlasEditorUI::render(atlas);
  }
  else if (const SPtr<FontAsset> font = asset->as<FontAsset>()) {
    renderFont(font);
  }
  else {
    ImGui::TextDisabled("Nothing to show for this type.");
  }
}
} // namespace

/*
 */
void
AssetInspectorUI::render()
{
  if (!g_cvarShowAssetInspector.get()) {
    return;
  }

  InspectorState& state = getState();
  ImGui::SetNextWindowSize(ImVec2(kWindowWidth, kWindowHeight), ImGuiCond_FirstUseEver);
  if (state.bFocus) {
    ImGui::SetNextWindowFocus();
    state.bFocus = false;
  }
  bool bVisible = true;
  if (ImGui::Begin(ICON_FA_FILE_ALT "  Asset Inspector###AssetInspector", &bVisible)) {
    if (state.asset) {
      renderAsset(state.asset);
    }
    else {
      ImGui::TextDisabled("Double click an asset in the Content Browser to inspect it.");
    }
  }
  ImGui::End();

  if (!bVisible) {
    setWindowVisible(false);
  }
}

/*
 */
void
AssetInspectorUI::open(const SPtr<IAsset>& asset)
{
  if (!asset) {
    return;
  }
  if (!asset->isLoaded() && !AssetManager::instance().syncLoadAsset(asset)) {
    CH_LOG_ERROR(AssetInspectorLog, "Failed to load asset: {0}", asset->getName());
  }

  InspectorState& state = getState();
  state.asset = asset;
  state.bFocus = true;
  if (asset->isLoaded()) {
    prepare(state);
  }
  if (!isWindowVisible()) {
    setWindowVisible(true);
  }
}

/*
 */
void
AssetInspectorUI::close(const IAsset& asset)
{
  InspectorState& state = getState();
  if (state.asset.get() == &asset) {
    state.asset.reset();
  }
  // Rows of the shown model may point at it too.
  Algorithm::removeAllIf(state.modelClips,
                         [&asset](const SPtr<IAsset>& clip) { return clip.get() == &asset; });
}

/*
 */
bool
AssetInspectorUI::isWindowVisible()
{
  return g_cvarShowAssetInspector.get();
}

/*
 */
void
AssetInspectorUI::setWindowVisible(bool bVisible)
{
  g_cvarShowAssetInspector.set(bVisible, ConsoleVariableSource::Console);
  EngineConfig::setUserValue("Editor", "Editor", "ShowAssetInspector",
                             bVisible ? "true" : "false");
}

/*
 */
void
AssetInspectorUI::shutDown()
{
  InspectorState& state = getState();
  state.asset.reset();
  state.modelClips.clear();
  state.boneChildren.clear();
}

} // namespace chEngineSDK
