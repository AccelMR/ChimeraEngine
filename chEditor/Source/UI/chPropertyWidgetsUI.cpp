/************************************************************************/
/**
 * @file chPropertyWidgetsUI.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief The Inspector widget of each property type.
 */
/************************************************************************/
#include "chPropertyWidgetsUI.h"

#include "chAssetDragDrop.h"
#include "chAssetManager.h"
#include "chEditorScene.h"
#include "chEditorSelection.h"
#include "chGameObject.h"
#include "chMath.h"
#include "chPrefabAsset.h"
#include "chScene.h"
#include "chSceneAsset.h"
#include "chStringUtils.h"
#include "chTextureAsset.h"

#include "imgui.h"

#include "../../Content/Fonts/IconsFontAwsome5.h"

namespace chEngineSDK {
namespace {
// Shared by every string row: only one text field is typed in at a time.
ANSICHAR g_textBuffer[ScriptPropertyUtils::MAX_TEXT_LENGTH + 1] = "";

/*
 * Objects a property points at, so a row does not walk the scene every frame. An entry is
 * looked up again when its object died or left the scene; a missing one is looked up again
 * when the scene's revision changes.
 */
struct ObjectCache {
  const Scene* scene = nullptr;
  uint32 revision = 0;
  UnorderedMap<UUID, WeakPtr<GameObject>> objects;
};

ObjectCache&
getObjectCache()
{
  static ObjectCache cache;
  return cache;
}

SPtr<GameObject>
findObject(const Scene& scene, const UUID& id)
{
  ObjectCache& cache = getObjectCache();
  const uint32 revision = EditorScene::getRevision();
  if (cache.scene != &scene || cache.revision != revision) {
    cache.scene = &scene;
    cache.revision = revision;
    cache.objects.clear();
  }
  WeakPtr<GameObject>& entry = cache.objects[id];
  SPtr<GameObject> gameObject = entry.lock();
  if (gameObject && gameObject->getScene() == &scene) {
    return gameObject;
  }
  gameObject = scene.findGameObject(id);
  entry = gameObject;
  return gameObject;
}

/*
 * True when the value changed.
 */
bool
renderNumberRow(const PropertyWidgetDesc& desc, ScriptPropertyValue& value)
{
  const ANSICHAR* label = desc.label;
  const bool bRange = desc.minimum < desc.maximum;
  if (value.type == ScriptPropertyType::Integer) {
    const int64 minimum = static_cast<int64>(desc.minimum);
    const int64 maximum = static_cast<int64>(desc.maximum);
    if (bRange) {
      return ImGui::SliderScalar(label, ImGuiDataType_S64, &value.integer, &minimum, &maximum);
    }
    return ImGui::DragScalar(label, ImGuiDataType_S64, &value.integer, 0.2f);
  }
  if (bRange) {
    return ImGui::SliderScalar(label, ImGuiDataType_Double, &value.number, &desc.minimum,
                               &desc.maximum, "%.3f");
  }
  return ImGui::DragScalar(label, ImGuiDataType_Double, &value.number, 0.05f, nullptr, nullptr,
                           "%.3f");
}

bool
renderVectorRow(const PropertyWidgetDesc& desc, ScriptPropertyValue& value)
{
  const ANSICHAR* label = desc.label;
  const bool bRange = desc.minimum < desc.maximum;
  const float minimum = bRange ? static_cast<float>(desc.minimum) : 0.0f;
  const float maximum = bRange ? static_cast<float>(desc.maximum) : 0.0f;
  float* components = value.components;
  switch (value.type) {
  case ScriptPropertyType::Vector2:
    return ImGui::DragFloat2(label, components, 0.05f, minimum, maximum, "%.3f");
  case ScriptPropertyType::Vector3:
    return ImGui::DragFloat3(label, components, 0.05f, minimum, maximum, "%.3f");
  case ScriptPropertyType::Vector4:
    return ImGui::DragFloat4(label, components, 0.05f, minimum, maximum, "%.3f");
  default:
    return ImGui::ColorEdit4(label, components, ImGuiColorEditFlags_Float);
  }
}

/*
 * A button with the object's name that takes objects dragged from the Scene Graph, and a
 * button to clear it.
 */
bool
renderObjectRow(const PropertyWidgetDesc& desc, ScriptPropertyValue& value,
                const GameObject& owner)
{
  const Scene* scene = owner.getScene();
  const SPtr<GameObject> target =
      scene && !value.object.isNull() ? findObject(*scene, value.object) : nullptr;
  const ANSICHAR* targetName = "None (drop an object)";
  if (target) {
    targetName = target->getName().c_str();
  }
  else if (!value.object.isNull()) {
    targetName = "Missing object";
  }

  bool bChanged = false;
  const float clearWidth = value.object.isNull() ? 0.0f : ImGui::GetFrameHeight();
  const float width = ImGui::CalcItemWidth() - clearWidth;
  ImGui::Button(targetName, ImVec2(width > 1.0f ? width : 1.0f, 0.0f));
  if (!value.object.isNull()) {
    ImGui::SetItemTooltip("%s\nDrop another object from the Scene Graph to replace it",
                          targetName);
  }
  if (ImGui::BeginDragDropTarget()) {
    // Objects inside a prefab instance get new UUIDs every time the scene loads, so they
    // cannot be pointed at from outside it.
    const SPtr<GameObject> dragged = AssetDragDrop::getDraggedGameObject();
    const bool bAllowed = dragged && dragged->getScene() == scene
                          && (!dragged->getOutermostPrefabInstance()
                              || dragged->getOutermostPrefabInstance() == dragged.get());
    if (bAllowed) {
      if (const SPtr<GameObject> dropped = AssetDragDrop::acceptGameObject()) {
        value.object = dropped->getUUID();
        bChanged = true;
        // The Scene Graph selected the dragged row as the drag started; the Inspector goes
        // back to the object whose field took it.
        if (SPtr<GameObject> self = findObject(*scene, owner.getUUID())) {
          EditorSelection::setSelectedGameObject(self);
        }
      }
    }
    ImGui::EndDragDropTarget();
  }
  if (!value.object.isNull()) {
    ImGui::SameLine(0.0f, 0.0f);
    if (ImGui::Button(ICON_FA_TIMES, ImVec2(clearWidth, 0.0f))) {
      value.object = UUID::null();
      bChanged = true;
    }
    ImGui::SetItemTooltip("Clear");
  }
  ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
  ImGui::TextUnformatted(desc.label);
  return bChanged;
}

/*
 * What an asset row differs in per asset type.
 */
struct AssetRowKind {
  UUID typeId;
  const ANSICHAR* noneText;
  const ANSICHAR* missingText;
  const ANSICHAR* tooltip;
  const ANSICHAR* emptyListText;
};

/*
 * The asset dropped on the last item when it has the kind's type; null otherwise.
 */
SPtr<IAsset>
acceptAssetOfType(const UUID& typeId)
{
  if (typeId == AssetTypeTraits<PrefabAsset>::getTypeId()) {
    return AssetDragDrop::acceptPrefab();
  }
  if (typeId == AssetTypeTraits<TextureAsset>::getTypeId()) {
    return AssetDragDrop::acceptTexture();
  }
  // Scenes are dragged as any asset.
  const SPtr<IAsset> dropped = AssetDragDrop::acceptAnyAsset();
  return dropped && dropped->getAssetTypeId() == typeId ? dropped : nullptr;
}

/*
 * A button with the asset's name that lists every asset of the kind when clicked and takes
 * those dragged from the Content Browser, and a button to clear it.
 */
bool
renderAssetRow(const PropertyWidgetDesc& desc, ScriptPropertyValue& value,
               const AssetRowKind& kind)
{
  const SPtr<IAsset> asset =
      value.object.isNull() ? nullptr : AssetManager::instance().getAsset(value.object);
  const ANSICHAR* assetName = kind.noneText;
  if (asset) {
    assetName = asset->getName();
  }
  else if (!value.object.isNull()) {
    assetName = kind.missingText;
  }

  bool bChanged = false;
  const float clearWidth = value.object.isNull() ? 0.0f : ImGui::GetFrameHeight();
  const float width = ImGui::CalcItemWidth() - clearWidth;
  if (ImGui::Button(assetName, ImVec2(width > 1.0f ? width : 1.0f, 0.0f))) {
    ImGui::OpenPopup("##assets");
  }
  ImGui::SetItemTooltip("%s\n%s", assetName, kind.tooltip);
  if (ImGui::BeginDragDropTarget()) {
    if (const SPtr<IAsset> dropped = acceptAssetOfType(kind.typeId)) {
      value.object = dropped->getUUID();
      bChanged = true;
    }
    ImGui::EndDragDropTarget();
  }
  if (ImGui::BeginPopup("##assets")) {
    // Only while the list is open, so walking every asset costs nothing on other frames.
    Vector<SPtr<IAsset>> assets;
    AssetManager::instance().getAssetsOfType(kind.typeId, assets);
    if (assets.empty()) {
      ImGui::TextDisabled("%s", kind.emptyListText);
    }
    for (const SPtr<IAsset>& listed : assets) {
      ImGui::PushID(listed.get());
      if (ImGui::Selectable(listed->getName(), listed->getUUID() == value.object)) {
        value.object = listed->getUUID();
        bChanged = true;
      }
      ImGui::SetItemTooltip("%s", listed->getAssetPath());
      ImGui::PopID();
    }
    ImGui::EndPopup();
  }
  if (!value.object.isNull()) {
    ImGui::SameLine(0.0f, 0.0f);
    if (ImGui::Button(ICON_FA_TIMES, ImVec2(clearWidth, 0.0f))) {
      value.object = UUID::null();
      bChanged = true;
    }
    ImGui::SetItemTooltip("Clear");
  }
  ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
  ImGui::TextUnformatted(desc.label);
  return bChanged;
}

/*
 * A combo of the names; the value is the index of the one picked.
 */
bool
renderEnumRow(const PropertyWidgetDesc& desc, ScriptPropertyValue& value)
{
  const int64 count = static_cast<int64>(desc.names.size());
  const ANSICHAR* shown = value.integer >= 0 && value.integer < count
                              ? desc.names[static_cast<SIZE_T>(value.integer)].c_str()
                              : "?";
  bool bChanged = false;
  if (ImGui::BeginCombo(desc.label, shown)) {
    for (int64 i = 0; i < count; ++i) {
      if (ImGui::Selectable(desc.names[static_cast<SIZE_T>(i)].c_str(), value.integer == i)) {
        value.integer = i;
        bChanged = true;
      }
    }
    ImGui::EndCombo();
  }
  return bChanged;
}

/*
 * A button listing the names whose bits are set, which opens a checkbox per name.
 */
bool
renderFlagsRow(const PropertyWidgetDesc& desc, ScriptPropertyValue& value)
{
  const uint64 bits = static_cast<uint64>(value.integer);
  const uint32 count =
      static_cast<uint32>(Math::min(desc.names.size(), static_cast<SIZE_T>(64)));
  uint32 setCount = 0;
  const ANSICHAR* firstSet = nullptr;
  for (uint32 i = 0; i < count; ++i) {
    if ((bits >> i) & 1u) {
      firstSet = firstSet ? firstSet : desc.names[i].c_str();
      ++setCount;
    }
  }

  // Written into a stack buffer, so the row does not allocate every frame.
  ANSICHAR summary[96] = "";
  SIZE_T length = 0;
  const auto append = [&summary, &length](const StringView part) {
    for (const ANSICHAR character : part) {
      if (length + 1 < sizeof(summary)) {
        summary[length++] = character;
      }
    }
    summary[length] = '\0';
  };
  if (setCount == 0) {
    append("None");
  }
  else if (setCount == count) {
    append("All");
  }
  else {
    append(firstSet);
    if (setCount > 1) {
      ANSICHAR number[StringUtils::MAX_INTEGER_CHARS];
      append(" and ");
      append(StringUtils::toChars(number, setCount - 1));
      append(" more");
    }
  }

  bool bChanged = false;
  if (ImGui::BeginCombo(desc.label, summary)) {
    for (uint32 i = 0; i < count; ++i) {
      bool bSet = ((bits >> i) & 1u) != 0;
      if (ImGui::Checkbox(desc.names[i].c_str(), &bSet)) {
        const uint64 bit = static_cast<uint64>(1) << i;
        value.integer = static_cast<int64>(bSet ? (bits | bit) : (bits & ~bit));
        bChanged = true;
      }
    }
    ImGui::EndCombo();
  }
  return bChanged;
}

/*
 * The widget of the property's type, editing value. True when it changed.
 */
bool
renderValue(const PropertyWidgetDesc& desc, ScriptPropertyValue& value,
            const GameObject& owner)
{
  if (value.type == ScriptPropertyType::Integer) {
    if (desc.widget == ComponentPropertyWidget::Enum) {
      return renderEnumRow(desc, value);
    }
    if (desc.widget == ComponentPropertyWidget::Flags) {
      return renderFlagsRow(desc, value);
    }
  }
  switch (value.type) {
  case ScriptPropertyType::Number:
  case ScriptPropertyType::Integer:
    return renderNumberRow(desc, value);
  case ScriptPropertyType::Boolean:
    return ImGui::Checkbox(desc.label, &value.boolean);
  case ScriptPropertyType::String:
    StringUtils::copyToBuffer(g_textBuffer, value.text);
    if (ImGui::InputText(desc.label, g_textBuffer, sizeof(g_textBuffer))) {
      value.text = g_textBuffer;
      return true;
    }
    return false;
  case ScriptPropertyType::GameObject:
    return renderObjectRow(desc, value, owner);
  case ScriptPropertyType::Prefab:
    return renderAssetRow(desc, value,
                          {.typeId = AssetTypeTraits<PrefabAsset>::getTypeId(),
                           .noneText = "None (drop a prefab)",
                           .missingText = "Missing prefab",
                           .tooltip = "Click to pick a prefab, or drop one from the Content "
                                      "Browser",
                           .emptyListText = "The project has no prefabs"});
  case ScriptPropertyType::Scene:
    return renderAssetRow(desc, value,
                          {.typeId = AssetTypeTraits<SceneAsset>::getTypeId(),
                           .noneText = "None (drop a scene)",
                           .missingText = "Missing scene",
                           .tooltip = "Click to pick a scene, or drop one from the Content "
                                      "Browser. Scenes picked here go into packages.",
                           .emptyListText = "The project has no scenes"});
  case ScriptPropertyType::Texture:
    return renderAssetRow(desc, value,
                          {.typeId = AssetTypeTraits<TextureAsset>::getTypeId(),
                           .noneText = "None (drop a texture)",
                           .missingText = "Missing texture",
                           .tooltip = "Click to pick a texture, or drop one from the "
                                      "Content Browser. Textures picked here go into "
                                      "packages.",
                           .emptyListText = "The project has no textures"});
  default:
    return renderVectorRow(desc, value);
  }
}
} // namespace

/*
 */
bool
PropertyWidgetsUI::render(const PropertyWidgetDesc& desc, ScriptPropertyValue& value,
                          const GameObject& owner)
{
  return renderValue(desc, value, owner);
}

} // namespace chEngineSDK
