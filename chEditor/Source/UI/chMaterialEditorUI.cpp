/************************************************************************/
/**
 * @file chMaterialEditorUI.cpp
 * @author AccelMR
 * @date 2026/10/20
 * @brief Edits the values and textures of a material or material instance asset.
 */
/************************************************************************/
#include "chMaterialEditorUI.h"

#include "chAssetDragDrop.h"
#include "chAssetInspectorUI.h"
#include "chAssetManager.h"
#include "chAssetThumbnails.h"
#include "chITexture.h"
#include "chImGuiRenderer.h"
#include "chLogger.h"
#include "chMaterial.h"
#include "chMaterialAsset.h"
#include "chMaterialInstanceAsset.h"
#include "chTextureAsset.h"
#include "../../Content/Fonts/IconsFontAwsome5.h"

#include "imgui.h"

namespace chEngineSDK {
CH_LOG_DECLARE_STATIC(MaterialEditorLog, All);

namespace {
// In MaterialAlphaMode order.
constexpr const ANSICHAR* kAlphaModeNames[] = {"Opaque", "Mask"};
static_assert(sizeof(kAlphaModeNames) / sizeof(kAlphaModeNames[0]) ==
              static_cast<SIZE_T>(MaterialAlphaMode::COUNT));

/**
 * How one named value of Material is shown.
 */
struct ValueRow
{
  const ANSICHAR* name;
  const ANSICHAR* label;
  float minimum;
  float maximum;
  // A drag instead of a slider, for values without a natural top.
  bool bDrag;
};

constexpr ValueRow kValueRows[] = {
    {.name = "baseColor", .label = "Base Color", .minimum = 0.0f, .maximum = 1.0f,
     .bDrag = false},
    {.name = "metallic", .label = "Metallic", .minimum = 0.0f, .maximum = 1.0f,
     .bDrag = false},
    {.name = "roughness", .label = "Roughness", .minimum = 0.0f, .maximum = 1.0f,
     .bDrag = false},
    // Above one is allowed: emissive light has no upper limit before the tonemap.
    {.name = "emissive", .label = "Emissive", .minimum = 0.0f, .maximum = 0.0f,
     .bDrag = false},
    {.name = "normalScale", .label = "Normal Scale", .minimum = 0.0f, .maximum = 4.0f,
     .bDrag = true},
    {.name = "occlusionStrength", .label = "Occlusion Strength", .minimum = 0.0f,
     .maximum = 1.0f, .bDrag = false},
    {.name = "alphaMode", .label = "Alpha Mode", .minimum = 0.0f, .maximum = 0.0f,
     .bDrag = false},
    {.name = "alphaCutoff", .label = "Alpha Cutoff", .minimum = 0.0f, .maximum = 1.0f,
     .bDrag = false},
    {.name = "twoSided", .label = "Two Sided", .minimum = 0.0f, .maximum = 0.0f,
     .bDrag = false},
};

// Shown under the parent row after a parent was refused, until another is picked.
String g_parentError;
const MaterialInstanceAsset* g_parentErrorOwner = nullptr;

/*
 */
const ANSICHAR*
getTextureLabel(MaterialTexture slot)
{
  switch (slot) {
    case MaterialTexture::BaseColor:
      return "Base Color";
    case MaterialTexture::MetallicRoughness:
      return "Metallic (B) Roughness (G)";
    case MaterialTexture::Normal:
      return "Normal";
    case MaterialTexture::Emissive:
      return "Emissive";
    case MaterialTexture::Occlusion:
      return "Occlusion (R)";
    case MaterialTexture::COUNT:
      break;
  }
  return "";
}

/*
 * Draws the widget of one value. True when it changed; bEditEnded is set when the edit
 * finished this frame (combos and checkboxes finish in the frame they change).
 */
bool
renderValueWidget(const ValueRow& row, Vector4& value, bool& bEditEnded)
{
  const uint32 parameter = Material::findParameter(row.name);
  CH_ASSERT(parameter != Material::NOT_FOUND);
  const MaterialParameterType type = Material::getParameters()[parameter].type;
  bool bChanged = false;
  switch (type) {
    case MaterialParameterType::Color: {
      float color[4] = {value.x, value.y, value.z, value.w};
      if (row.maximum > row.minimum) {
        bChanged = ImGui::ColorEdit4(row.label, color);
      }
      else {
        bChanged = ImGui::ColorEdit3(row.label, color,
                                     ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
      }
      value = Vector4(color[0], color[1], color[2], color[3]);
      bEditEnded = ImGui::IsItemDeactivatedAfterEdit();
      break;
    }
    case MaterialParameterType::Float:
      if (row.bDrag) {
        bChanged = ImGui::DragFloat(row.label, &value.x, 0.01f, row.minimum, row.maximum);
      }
      else {
        bChanged = ImGui::SliderFloat(row.label, &value.x, row.minimum, row.maximum);
      }
      bEditEnded = ImGui::IsItemDeactivatedAfterEdit();
      break;
    case MaterialParameterType::AlphaMode: {
      int32 mode = static_cast<int32>(value.x);
      bChanged = ImGui::Combo(row.label, &mode, kAlphaModeNames,
                              static_cast<int32>(MaterialAlphaMode::COUNT));
      value.x = static_cast<float>(mode);
      bEditEnded = bChanged;
      break;
    }
    case MaterialParameterType::Bool: {
      bool bValue = value.x != 0.0f;
      bChanged = ImGui::Checkbox(row.label, &bValue);
      value.x = bValue ? 1.0f : 0.0f;
      bEditEnded = bChanged;
      break;
    }
    case MaterialParameterType::COUNT:
      break;
  }
  return bChanged;
}

/*
 * Draws the override checkbox of an instance row. True when it was toggled this frame.
 */
bool
renderOverrideCheckbox(bool& bOverridden)
{
  const bool bToggled = ImGui::Checkbox("##override", &bOverridden);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(bOverridden ? "Set by this instance; untick to use the parent's"
                                  : "Taken from the parent; tick to set it here");
  }
  ImGui::SameLine();
  return bToggled;
}

/*
 * Draws one texture slot. True when a texture was dropped or cleared; outTexture is then
 * the new one (null when cleared).
 */
bool
renderTextureWidget(const Material& material,
                    MaterialTexture slot,
                    SPtr<TextureAsset>& outTexture)
{
  const SPtr<TextureAsset>& texture = material.getTexture(slot);
  const ITexture* gpuTexture = material.getGpuTexture(slot);
  const uint64 textureId = gpuTexture ? ImGuiRenderer::getTextureId(*gpuTexture) : 0;
  constexpr float kThumbnailSize = 48.0f;
  if (textureId != 0) {
    ImGui::Image(static_cast<ImTextureID>(textureId), ImVec2(kThumbnailSize, kThumbnailSize));
  }
  else {
    ImGui::Button("##noTexture", ImVec2(kThumbnailSize, kThumbnailSize));
  }

  bool bChanged = false;
  auto acceptDrop = [&bChanged, &outTexture]() {
    if (ImGui::BeginDragDropTarget()) {
      if (SPtr<TextureAsset> dropped = AssetDragDrop::acceptTexture()) {
        outTexture = std::move(dropped);
        bChanged = true;
      }
      ImGui::EndDragDropTarget();
    }
  };
  acceptDrop();

  ImGui::SameLine();
  ImGui::BeginGroup();
  ImGui::TextUnformatted(getTextureLabel(slot));
  ImGui::TextDisabled("%s", texture ? texture->getName() : "None (drop a texture)");
  if (texture && ImGui::SmallButton("Clear")) {
    outTexture.reset();
    bChanged = true;
  }
  ImGui::EndGroup();
  acceptDrop();
  return bChanged;
}

/*
 * The parent row of an instance. True when the parent changed.
 */
bool
renderParent(MaterialInstanceAsset& instance)
{
  const SPtr<MaterialInterfaceAsset> parent = instance.getParentMaterial();
  const ANSICHAR* parentName = "None (drop a material)";
  if (parent) {
    parentName = parent->getName();
  }
  else if (!instance.getParentId().isNull()) {
    parentName = "Missing parent";
  }

  ImGui::TextUnformatted("Parent");
  ImGui::SameLine();
  ImGui::Button(parentName, ImVec2(-FLT_MIN, 0.0f));
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Drop a material or another instance. Values left unticked below "
                      "come from it.");
  }

  bool bChanged = false;
  if (ImGui::BeginDragDropTarget()) {
    if (SPtr<MaterialInterfaceAsset> dropped = AssetDragDrop::acceptMaterial()) {
      if (instance.setParent(dropped)) {
        g_parentError.clear();
        bChanged = true;
      }
      else {
        g_parentError = dropped.get() == &instance
                            ? String("An instance cannot be its own parent.")
                            : "That parent leads back to this instance or makes the chain "
                              "too long.";
        g_parentErrorOwner = &instance;
      }
    }
    ImGui::EndDragDropTarget();
  }

  if (parent) {
    if (ImGui::SmallButton(ICON_FA_EXTERNAL_LINK_ALT " Open Parent")) {
      AssetInspectorUI::open(parent);
    }
    ImGui::SameLine();
  }
  if (!instance.getParentId().isNull() && ImGui::SmallButton("Clear Parent")) {
    instance.setParent(nullptr);
    g_parentError.clear();
    bChanged = true;
  }
  if (g_parentErrorOwner == &instance && !g_parentError.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", g_parentError.c_str());
  }
  ImGui::Separator();
  return bChanged;
}
} // namespace

/*
 */
void
MaterialEditorUI::render(const SPtr<MaterialInterfaceAsset>& materialAsset)
{
  const SPtr<MaterialAsset> material = materialAsset->as<MaterialAsset>();
  const SPtr<MaterialInstanceAsset> instance = materialAsset->as<MaterialInstanceAsset>();
  if (!material && !instance) {
    return;
  }

  // Saved once an edit ends instead of on every frame of a drag.
  bool bSave = false;
  if (instance) {
    bSave |= renderParent(*instance);
  }

  for (const ValueRow& row : kValueRows) {
    const Material& resolved = materialAsset->getMaterial();
    if (StringView(row.name) == "alphaCutoff" &&
        resolved.getAlphaMode() != MaterialAlphaMode::Mask) {
      continue;
    }

    ImGui::PushID(row.name);
    Vector4 value;
    const bool bHasValue = resolved.getValue(row.name, value);
    CH_ASSERT(bHasValue);
    CH_PARAMETER_UNUSED(bHasValue);

    bool bOverridden = true;
    if (instance) {
      bOverridden = instance->getOverrides().findValue(row.name) != nullptr;
      if (renderOverrideCheckbox(bOverridden)) {
        if (bOverridden) {
          instance->setValue(row.name, value);
        }
        else {
          instance->resetValue(row.name);
        }
        bSave = true;
      }
    }

    ImGui::BeginDisabled(!bOverridden);
    bool bEditEnded = false;
    if (renderValueWidget(row, value, bEditEnded)) {
      if (instance) {
        instance->setValue(row.name, value);
      }
      else {
        material->getMaterial().setValue(row.name, value);
        material->notifyMaterialChanged();
      }
    }
    bSave |= bEditEnded;
    ImGui::EndDisabled();
    ImGui::PopID();
  }

  for (uint32 i = 0; i < static_cast<uint32>(MaterialTexture::COUNT); ++i) {
    const MaterialTexture slot = static_cast<MaterialTexture>(i);
    ImGui::PushID(static_cast<int32>(i));

    bool bOverridden = true;
    if (instance) {
      bOverridden = instance->getOverrides().findTexture(slot) != nullptr;
      if (renderOverrideCheckbox(bOverridden)) {
        if (bOverridden) {
          instance->setTexture(slot, materialAsset->getMaterial().getTexture(slot));
        }
        else {
          instance->resetTexture(slot);
        }
        bSave = true;
      }
    }

    ImGui::BeginDisabled(!bOverridden);
    SPtr<TextureAsset> texture;
    if (renderTextureWidget(materialAsset->getMaterial(), slot, texture)) {
      if (instance) {
        instance->setTexture(slot, texture);
      }
      else {
        material->getMaterial().setTexture(slot, texture);
        material->notifyMaterialChanged();
      }
      bSave = true;
    }
    ImGui::EndDisabled();
    ImGui::PopID();
  }

  if (!bSave) {
    return;
  }
  if (!AssetManager::instance().saveAsset(materialAsset)) {
    CH_LOG_ERROR(MaterialEditorLog, "Failed to save material {0}", materialAsset->getName());
  }
  AssetThumbnails::regenerate(*materialAsset);
}

} // namespace chEngineSDK
