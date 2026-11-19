/************************************************************************/
/**
 * @file chPostProcessEditorUI.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief The pass list of a post-process component in the Object Inspector.
 */
/************************************************************************/
#include "chPostProcessEditorUI.h"

#include "chAssetDragDrop.h"
#include "chAssetManager.h"
#include "chEditorScene.h"
#include "chEnginePaths.h"
#include "chFileSystem.h"
#include "chGameObject.h"
#include "chImportDialog.h"
#include "chLogger.h"
#include "chPostProcessComponent.h"
#include "chPropertyWidgetsUI.h"
#include "chShaderAsset.h"
#include "chStringUtils.h"

#if USING(CH_CODECS)
#include "chAssetCodecManager.h"
#endif // USING(CH_CODECS)

#include "imgui.h"

#include "../../Content/Fonts/IconsFontAwsome5.h"

namespace chEngineSDK {

CH_LOG_DECLARE_STATIC(PostProcessEditorLog, All);

namespace {
// Files of /Engine/PostProcess, by name without extension.
constexpr const ANSICHAR* kEngineEffects[] = {"Vignette", "GrayscaleTint", "Pixelate", "CRT"};

// Filled when the Add Pass popup opens, so drawing it does not walk every asset.
Vector<SPtr<IAsset>> g_projectShaders;

NODISCARD ScriptPropertyValue
toPropertyValue(const ShaderParameter& parameter, const Vector4& value)
{
  ScriptPropertyValue result;
  result.components[0] = value.x;
  result.components[1] = value.y;
  result.components[2] = value.z;
  result.components[3] = value.w;
  switch (parameter.type) {
    case ShaderParameterType::Float:
      result.type = ScriptPropertyType::Number;
      result.number = value.x;
      break;
    case ShaderParameterType::Int:
      result.type = ScriptPropertyType::Integer;
      result.integer = static_cast<int64>(value.x);
      break;
    case ShaderParameterType::Bool:
      result.type = ScriptPropertyType::Boolean;
      result.boolean = value.x != 0.0f;
      break;
    case ShaderParameterType::Float2:
      result.type = ScriptPropertyType::Vector2;
      break;
    case ShaderParameterType::Float3:
      result.type = ScriptPropertyType::Vector3;
      break;
    case ShaderParameterType::Float4:
      result.type = ScriptPropertyType::Vector4;
      break;
    case ShaderParameterType::Color:
    case ShaderParameterType::COUNT:
      result.type = ScriptPropertyType::Color;
      break;
  }
  return result;
}

NODISCARD Vector4
fromPropertyValue(const ScriptPropertyValue& value)
{
  switch (value.type) {
    case ScriptPropertyType::Number:
      return Vector4(static_cast<float>(value.number), 0.0f, 0.0f, 0.0f);
    case ScriptPropertyType::Integer:
      return Vector4(static_cast<float>(value.integer), 0.0f, 0.0f, 0.0f);
    case ScriptPropertyType::Boolean:
      return Vector4(value.boolean ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
    default:
      return Vector4(value.components[0], value.components[1], value.components[2],
                     value.components[3]);
  }
}

/**
 * A shader asset dropped on the last item, or null.
 */
NODISCARD SPtr<ShaderAsset>
acceptShaderDrop()
{
  if (!ImGui::BeginDragDropTarget()) {
    return nullptr;
  }
  SPtr<ShaderAsset> shader;
  const SPtr<IAsset> dragged = AssetDragDrop::peekAnyAsset();
  if (dragged && dragged->isTypeOf<ShaderAsset>()) {
    if (const SPtr<IAsset> dropped = AssetDragDrop::acceptAnyAsset()) {
      shader = dropped->as<ShaderAsset>();
    }
  }
  ImGui::EndDragDropTarget();
  return shader;
}

/**
 * The shader picked in the popup: a project shader or an engine effect. Null while none.
 */
NODISCARD SPtr<ShaderAsset>
renderShaderPicker(const ANSICHAR* popupId)
{
  if (!ImGui::BeginPopup(popupId)) {
    return nullptr;
  }
  if (ImGui::IsWindowAppearing()) {
    g_projectShaders.clear();
    AssetManager::instance().getAssetsOfType(AssetTypeTraits<ShaderAsset>::getTypeId(),
                                             g_projectShaders);
  }
  SPtr<ShaderAsset> picked;
  ImGui::SeparatorText("Engine Effects");
  for (const ANSICHAR* effect : kEngineEffects) {
    if (ImGui::Selectable(effect)) {
      picked = PostProcessEditorUI::findOrImportEngineEffect(effect);
    }
  }
  ImGui::SeparatorText("Project Shaders");
  bool bAny = false;
  for (const SPtr<IAsset>& asset : g_projectShaders) {
    bAny = true;
    if (ImGui::Selectable(asset->getName())) {
      picked = asset->as<ShaderAsset>();
    }
    ImGui::SetItemTooltip("%s", asset->getAssetPath());
  }
  if (!bAny) {
    ImGui::TextDisabled("None yet. Import a .hlsl file.");
  }
  if (picked) {
    g_projectShaders.clear();
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndPopup();
  return picked;
}

void
renderParameters(PostProcessComponent& postProcess, uint32 index, GameObject& owner)
{
  const PostProcessPass& pass = postProcess.getPass(index);
  const ShaderAsset& shader = *pass.shader;
  for (const ShaderParameter& parameter : shader.getParameters()) {
    ImGui::PushID(parameter.name.c_str());
    const PostProcessOverride* entry = postProcess.findOverride(index, parameter.name);
    // A button wide for the reset, so the widgets keep one width with or without it.
    const float resetWidth = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetNextItemWidth(ImGui::CalcItemWidth() - resetWidth);
    ScriptPropertyValue value =
        toPropertyValue(parameter, entry ? entry->value : parameter.defaultValue);
    const PropertyWidgetDesc widget{.label = parameter.name.c_str(),
                                    .minimum = parameter.minimum,
                                    .maximum = parameter.maximum};
    if (PropertyWidgetsUI::render(widget, value, owner)) {
      EditorScene::recordChange(owner, "Edit " + parameter.name);
      static_cast<void>(postProcess.setValue(index, parameter.name, fromPropertyValue(value)));
    }
    if (!parameter.tooltip.empty()) {
      ImGui::SetItemTooltip("%s", parameter.tooltip.c_str());
    }
    if (entry) {
      ImGui::SameLine();
      if (ImGui::SmallButton(ICON_FA_UNDO)) {
        EditorScene::recordChange(owner, "Reset " + parameter.name);
        postProcess.resetValue(index, parameter.name);
      }
      ImGui::SetItemTooltip("Back to the shader's default");
    }
    ImGui::PopID();
  }

  // Values kept for parameters the shader no longer has.
  for (const PostProcessOverride& entry : pass.overrides) {
    if (shader.findParameter(entry.name) != ShaderAsset::NOT_FOUND) {
      continue;
    }
    ImGui::PushID(entry.name.c_str());
    ImGui::TextDisabled("%s (not in the shader)", entry.name.c_str());
    ImGui::SameLine();
    const bool bRemove = ImGui::SmallButton(ICON_FA_TRASH);
    ImGui::PopID();
    if (bRemove) {
      EditorScene::recordChange(owner, "Remove " + entry.name);
      postProcess.resetValue(index, entry.name);
      break;
    }
  }
}
} // namespace

/*
 */
void
PostProcessEditorUI::render(Component& component)
{
  PostProcessComponent& postProcess = static_cast<PostProcessComponent&>(component);
  GameObject& owner = *component.getOwner();
  const uint32 count = postProcess.getPassCount();
  ImGui::SeparatorText("Passes");

  uint32 toRemove = count;
  uint32 moveFrom = count;
  uint32 moveTo = count;
  for (uint32 index = 0; index < count; ++index) {
    ImGui::PushID(static_cast<int32>(index));
    const PostProcessPass& pass = postProcess.getPass(index);
    const ShaderAsset* shader = pass.shader.get();

    bool bEnabled = pass.bEnabled;
    if (ImGui::Checkbox("##Enabled", &bEnabled)) {
      EditorScene::recordChange(owner, "Edit Pass Enabled");
      postProcess.setPassEnabled(index, bEnabled);
    }
    ImGui::SetItemTooltip("Runs this pass");
    ImGui::SameLine();
    const bool bOpen =
        ImGui::TreeNodeEx("Pass", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap,
                          "%s", shader ? shader->getName() : "(missing shader)");
    if (const SPtr<ShaderAsset> dropped = acceptShaderDrop()) {
      EditorScene::recordChange(owner, "Change Pass Shader");
      postProcess.setPassShader(index, dropped);
    }

    // Arrows and the trash at the right edge.
    const float buttons = ImGui::GetFrameHeight() * 3.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - buttons);
    ImGui::BeginDisabled(index == 0);
    if (ImGui::SmallButton(ICON_FA_ARROW_UP)) {
      moveFrom = index;
      moveTo = index - 1;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(index + 1 == count);
    if (ImGui::SmallButton(ICON_FA_ARROW_DOWN)) {
      moveFrom = index;
      moveTo = index + 1;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_FA_TRASH)) {
      toRemove = index;
    }

    if (bOpen) {
      if (shader) {
        ImGui::TextDisabled("%s", shader->getStage() == PostProcessStage::BeforeTonemap
                                      ? "Before the tonemap (linear HDR)"
                                      : "After the tonemap (final image)");
      }
      if (ImGui::SmallButton(ICON_FA_EXCHANGE_ALT "  Shader")) {
        ImGui::OpenPopup("ChangeShader");
      }
      ImGui::SetItemTooltip("Pick another shader for this pass; values it shares by name stay");
      if (const SPtr<ShaderAsset> picked = renderShaderPicker("ChangeShader")) {
        EditorScene::recordChange(owner, "Change Pass Shader");
        postProcess.setPassShader(index, picked);
      }

      float weight = pass.weight;
      if (ImGui::SliderFloat("Weight", &weight, 0.0f, 1.0f)) {
        EditorScene::recordChange(owner, "Edit Pass Weight");
        postProcess.setPassWeight(index, weight);
      }
      ImGui::SetItemTooltip("How much of the effect shows; below 1 costs one more draw");

      if (!shader) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                           "The shader is missing; drop one on the pass name.");
      }
      else {
        if (!shader->getCompileError().empty()) {
          ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                             "The last compile failed; see the Asset Inspector.");
        }
        if (!shader->isInterfaceCurrent()) {
          ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                             "Compiled for older engine headers; import it again.");
        }
        renderParameters(postProcess, index, owner);
      }
      ImGui::TreePop();
    }
    ImGui::PopID();
  }

  // After the loop, which reads the list these change.
  if (toRemove < count) {
    EditorScene::recordChange(owner, "Remove Pass");
    postProcess.removePass(toRemove);
  }
  else if (moveFrom < count) {
    EditorScene::recordChange(owner, "Move Pass");
    postProcess.movePass(moveFrom, moveTo);
  }

  ImGui::BeginDisabled(count >= PostProcessComponent::MAX_PASSES);
  if (ImGui::Button(ICON_FA_PLUS "  Add Pass", ImVec2(-FLT_MIN, 0.0f))) {
    ImGui::OpenPopup("AddPass");
  }
  ImGui::EndDisabled();
  ImGui::SetItemTooltip("Or drop a shader here");
  SPtr<ShaderAsset> added = acceptShaderDrop();
  if (const SPtr<ShaderAsset> picked = renderShaderPicker("AddPass")) {
    added = picked;
  }
  if (added) {
    EditorScene::recordChange(owner, "Add Pass");
    static_cast<void>(postProcess.addPass(added));
  }
}

/*
 */
SPtr<ShaderAsset>
PostProcessEditorUI::findOrImportEngineEffect(const StringView effect)
{
  const Path source = FileSystem::absolutePath(EnginePaths::getEngineContentDirectory().join(
      Path(StringUtils::format("PostProcess/{0}.hlsl", effect))));
  AssetManager& assetManager = AssetManager::instance();
  if (const SPtr<IAsset> existing = assetManager.findAssetBySource(source, StringView())) {
    return existing->as<ShaderAsset>();
  }
#if USING(CH_CODECS)
  if (AssetCodecManager::isStarted()) {
    const SPtr<IAsset> imported = AssetCodecManager::instance().importAsset(
        source, StringUtils::format("PP_{0}", effect), Path(ENGINE_EFFECT_FOLDER));
    if (imported) {
      ImportDialog::notifyImported(imported);
      return imported->as<ShaderAsset>();
    }
  }
#endif // USING(CH_CODECS)
  CH_LOG_ERROR(PostProcessEditorLog, "The engine effect {0} could not be imported.", effect);
  return nullptr;
}

} // namespace chEngineSDK
