/************************************************************************/
/**
 * @file chComponentEditorUI.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief Inspector sections of components described for the editor.
 */
/************************************************************************/
#include "chComponentEditorUI.h"

#include "chAlgorithm.h"
#include "chComponentEditor.h"
#include "chEditorScene.h"
#include "chGameObject.h"
#include "chKeyCurveWidgetsUI.h"
#include "chPropertyWidgetsUI.h"
#include "chStringUtils.h"

#include "imgui.h"

#include "../../Content/Fonts/IconsFontAwsome5.h"

namespace chEngineSDK {
namespace {
// Typed into by the Add Component popup.
ANSICHAR g_search[64] = "";
// The keys of the gradient or curve being drawn, reused so drawing them does not allocate.
Vector<GradientKey> g_gradientKeys;
Vector<CurveKey> g_curveKeys;

/*
 * The header of each type, made once so drawing it does not build a string every frame.
 */
const String&
getHeaderLabel(const ComponentEditorDesc& desc)
{
  static UnorderedMap<UUID, String> labels;
  auto [it, bInserted] = labels.try_emplace(desc.typeId);
  if (bInserted) {
    it->second = ICON_FA_PUZZLE_PIECE "  ";
    it->second += desc.displayName;
  }
  return it->second;
}

void
renderTooltip(const String& tooltip)
{
  if (!tooltip.empty()) {
    ImGui::SetItemTooltip("%s", tooltip.c_str());
  }
}

/*
 * Each element of a List entry as a node with a row per field, a button to remove it and
 * one to add another.
 */
void
renderList(Component& component, const ComponentEditorEntry& entry)
{
  GameObject& owner = *component.getOwner();
  const SIZE_T count = entry.listCount(component);
  const bool bOpen = ImGui::TreeNodeEx(&entry, ImGuiTreeNodeFlags_DefaultOpen, "%s (%u)",
                                       entry.label.c_str(), static_cast<uint32>(count));
  renderTooltip(entry.tooltip);
  if (!bOpen) {
    return;
  }
  SIZE_T toRemove = count;
  for (SIZE_T i = 0; i < count; ++i) {
    ImGui::PushID(static_cast<int32>(i));
    const bool bItemOpen = ImGui::TreeNodeEx("item", ImGuiTreeNodeFlags_DefaultOpen, "%s %u",
                                             entry.itemName.c_str(), static_cast<uint32>(i + 1));
    ImGui::SameLine();
    // The remove button at the right edge.
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x -
                         ImGui::GetFrameHeight());
    if (ImGui::SmallButton(ICON_FA_TRASH)) {
      toRemove = i;
    }
    if (bItemOpen) {
      for (const ComponentListField& field : entry.fields) {
        ImGui::PushID(&field);
        ScriptPropertyValue value;
        value.type = field.type;
        field.getter(component, i, value);
        const PropertyWidgetDesc widget{.label = field.label.c_str(),
                                        .minimum = field.minimum,
                                        .maximum = field.maximum,
                                        .widget = field.widget,
                                        .names = field.names};
        if (PropertyWidgetsUI::render(widget, value, owner)) {
          EditorScene::recordChange(owner, "Edit " + field.label);
          field.setter(component, i, value);
        }
        renderTooltip(field.tooltip);
        ImGui::PopID();
      }
      ImGui::TreePop();
    }
    ImGui::PopID();
  }
  if (toRemove < count) {
    EditorScene::recordChange(owner, "Remove " + entry.itemName);
    entry.listRemove(component, toRemove);
  }
  ImGui::BeginDisabled(count >= entry.maxCount);
  if (ImGui::SmallButton(ICON_FA_PLUS "  Add")) {
    EditorScene::recordChange(owner, "Add " + entry.itemName);
    entry.listAdd(component);
  }
  ImGui::EndDisabled();
  ImGui::TreePop();
}

/*
 * True when Remove was clicked.
 */
bool
renderSection(Component& component, const ComponentEditorDesc& desc)
{
  if (!ImGui::CollapsingHeader(getHeaderLabel(desc).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
    return false;
  }
  GameObject& owner = *component.getOwner();

  bool bEnabled = component.isEnabled();
  if (ImGui::Checkbox("Enabled", &bEnabled)) {
    EditorScene::recordChange(owner, "Edit Enabled");
    component.setEnabled(bEnabled);
  }
  ImGui::SameLine();
  const bool bRemove = ImGui::SmallButton(ICON_FA_TRASH "  Remove");

  for (const ComponentEditorEntry& entry : desc.entries) {
    ImGui::PushID(&entry);
    switch (entry.kind) {
    case ComponentEditorEntryKind::Header:
      ImGui::SeparatorText(entry.label.c_str());
      break;
    case ComponentEditorEntryKind::Action:
      if (ImGui::Button(entry.label.c_str(), ImVec2(-FLT_MIN, 0.0f))) {
        entry.action(component);
      }
      if (!entry.tooltip.empty()) {
        ImGui::SetItemTooltip("%s", entry.tooltip.c_str());
      }
      break;
    case ComponentEditorEntryKind::Property: {
      ScriptPropertyValue value;
      value.type = entry.type;
      entry.getter(component, value);
      const PropertyWidgetDesc widget{.label = entry.label.c_str(),
                                      .minimum = entry.minimum,
                                      .maximum = entry.maximum,
                                      .widget = entry.widget,
                                      .names = entry.names};
      if (PropertyWidgetsUI::render(widget, value, owner)) {
        EditorScene::recordChange(owner, "Edit " + entry.label);
        entry.setter(component, value);
      }
      renderTooltip(entry.tooltip);
      break;
    }
    case ComponentEditorEntryKind::Gradient: {
      const Span<const GradientKey> keys = entry.gradientGetter(component);
      g_gradientKeys.assign(keys.begin(), keys.end());
      if (KeyCurveWidgetsUI::renderGradient(entry.label.c_str(), g_gradientKeys,
                                            entry.maxCount)) {
        EditorScene::recordChange(owner, "Edit " + entry.label);
        entry.gradientSetter(component, g_gradientKeys);
      }
      break;
    }
    case ComponentEditorEntryKind::Curve: {
      const Span<const CurveKey> keys = entry.curveGetter(component);
      g_curveKeys.assign(keys.begin(), keys.end());
      if (KeyCurveWidgetsUI::renderCurve(entry.label.c_str(), g_curveKeys,
                                         static_cast<float>(entry.minimum),
                                         static_cast<float>(entry.maximum),
                                         static_cast<float>(entry.emptyValue),
                                         entry.maxCount)) {
        EditorScene::recordChange(owner, "Edit " + entry.label);
        entry.curveSetter(component, g_curveKeys);
      }
      break;
    }
    case ComponentEditorEntryKind::List:
      renderList(component, entry);
      break;
    }
    ImGui::PopID();
  }
  return bRemove;
}
} // namespace

/*
 */
void
ComponentEditorUI::renderComponents(GameObject& gameObject)
{
  Component* toRemove = nullptr;
  for (const UniquePtr<Component>& component : gameObject.getComponents()) {
    const ComponentEditorDesc* desc = ComponentEditorRegister::find(component->getTypeId());
    if (!desc) {
      continue;
    }
    ImGui::PushID(component.get());
    if (renderSection(*component, *desc)) {
      toRemove = component.get();
    }
    ImGui::PopID();
  }
  // After the loop, which walks the list the removal changes.
  if (toRemove) {
    const ComponentEditorDesc* desc = ComponentEditorRegister::find(toRemove->getTypeId());
    EditorScene::recordChange(gameObject, "Remove " + desc->displayName);
    gameObject.removeComponent(*toRemove);
  }
}

/*
 */
void
ComponentEditorUI::renderAddComponent(GameObject& gameObject)
{
  const Vector<UniquePtr<ComponentEditorDesc>>& descriptions =
      ComponentEditorRegister::getAll();
  if (descriptions.empty()) {
    return;
  }

  ImGui::Spacing();
  if (ImGui::Button(ICON_FA_PLUS "  Add Component", ImVec2(-FLT_MIN, 0.0f))) {
    g_search[0] = '\0';
    ImGui::OpenPopup("AddComponent");
  }
  ImGui::SetItemTooltip("Components of the plugins the project enables");
  if (!ImGui::BeginPopup("AddComponent")) {
    return;
  }

  if (ImGui::IsWindowAppearing()) {
    ImGui::SetKeyboardFocusHere();
  }
  ImGui::InputTextWithHint("##search", ICON_FA_SEARCH "  Search", g_search, sizeof(g_search));
  const StringView search(g_search);

  // Grouped by category in the order the first type of each was added.
  const ComponentEditorDesc* picked = nullptr;
  bool bAnyShown = false;
  for (SIZE_T first = 0; first < descriptions.size(); ++first) {
    const String& category = descriptions[first]->category;
    bool bNewCategory = true;
    for (SIZE_T earlier = 0; earlier < first && bNewCategory; ++earlier) {
      bNewCategory = descriptions[earlier]->category != category;
    }
    if (!bNewCategory) {
      continue;
    }

    bool bHeaderShown = false;
    for (SIZE_T i = first; i < descriptions.size(); ++i) {
      const ComponentEditorDesc& desc = *descriptions[i];
      if (desc.category != category
          || (!search.empty() && !StringUtils::containsIgnoreCase(desc.displayName, search))) {
        continue;
      }
      if (!bHeaderShown) {
        ImGui::SeparatorText(category.c_str());
        bHeaderShown = true;
      }
      bAnyShown = true;
      const bool bPresent = Algorithm::containsIf(
          gameObject.getComponents(), [&desc](const UniquePtr<Component>& component) {
            return component->getTypeId() == desc.typeId;
          });
      ImGui::BeginDisabled(bPresent && !desc.bAllowMultiple);
      if (ImGui::Selectable(desc.displayName.c_str())) {
        picked = &desc;
      }
      ImGui::EndDisabled();
      if (bPresent && !desc.bAllowMultiple) {
        ImGui::SetItemTooltip("The object has one already");
      }
    }
  }
  if (!bAnyShown) {
    ImGui::TextDisabled("Nothing matches");
  }
  ImGui::EndPopup();

  if (picked) {
    EditorScene::recordChange(gameObject, "Add " + picked->displayName);
    static_cast<void>(gameObject.addComponentByType(picked->typeId));
  }
}

} // namespace chEngineSDK
