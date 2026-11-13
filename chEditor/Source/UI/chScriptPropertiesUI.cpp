/************************************************************************/
/**
 * @file chScriptPropertiesUI.cpp
 * @author AccelMR
 * @date 2026/10/22
 * @brief Inspector rows for the properties a script lists.
 */
/************************************************************************/
#include "chScriptPropertiesUI.h"

#include "chEditorScene.h"
#include "chGameObject.h"
#include "chPropertyWidgetsUI.h"
#include "chScriptAsset.h"
#include "chScriptComponent.h"

#include "imgui.h"

#include "../../Content/Fonts/IconsFontAwsome5.h"

namespace chEngineSDK {
namespace {
/*
 * Values saved for properties the script does not list (any more), or lists with another
 * type. They are kept until removed, so a typo in the script loses nothing.
 */
void
renderUnusedOverrides(ScriptComponent& component, const ScriptClassInfo& scriptClass)
{
  const ScriptPropertyOverride* toRemove = nullptr;
  for (const ScriptPropertyOverride& entry : component.getOverrides()) {
    const ScriptProperty* declared = nullptr;
    for (const ScriptProperty& property : scriptClass.properties) {
      if (property.name == entry.name) {
        declared = &property;
        break;
      }
    }
    if (declared && declared->defaultValue.type == entry.value.type) {
      continue;
    }

    ImGui::PushID(entry.name.c_str());
    if (ImGui::SmallButton(ICON_FA_TRASH)) {
      toRemove = &entry;
    }
    ImGui::SetItemTooltip("Remove the saved value");
    ImGui::SameLine();
    ImGui::TextDisabled("%s (unused %s)", entry.name.c_str(),
                        ScriptPropertyUtils::getTypeName(entry.value.type));
    if (declared) {
      ImGui::SetItemTooltip("The script lists it now as a %s",
                            ScriptPropertyUtils::getTypeName(declared->defaultValue.type));
    }
    else {
      ImGui::SetItemTooltip("The script does not list it any more");
    }
    ImGui::PopID();
  }
  if (toRemove) {
    const String name = toRemove->name;
    EditorScene::recordChange(*component.getOwner(), "Remove Script Value");
    static_cast<void>(component.removeOverride(name));
  }
}
} // namespace

/*
 */
void
ScriptPropertiesUI::render(ScriptComponent& component)
{
  if (!component.getScript()) {
    return;
  }
  const ScriptClassInfo* scriptClass = component.getScriptClass();
  if (!scriptClass || !scriptClass->bValid) {
    ImGui::TextColored(ImVec4(0.95f, 0.4f, 0.4f, 1.0f),
                       "The script has an error (see the Output Log)");
    return;
  }
  if (scriptClass->properties.empty() && component.getOverrides().empty()) {
    return;
  }

  GameObject& owner = *component.getOwner();
  const bool bRunning = component.isRunning();
  ScriptManager& scriptManager = ScriptManager::instance();
  ImGui::Spacing();
  for (const ScriptProperty& property : scriptClass->properties) {
    ImGui::PushID(property.name.c_str());
    const ScriptPropertyType type = property.defaultValue.type;
    const ScriptPropertyValue* saved = component.findOverride(property.name);
    const bool bOverridden = saved && saved->type == type;

    // While the script runs, what self holds now; the script may have changed it.
    ScriptPropertyValue value;
    if (!bRunning ||
        !scriptManager.getInstanceProperty(component.getInstance(), property.name, type,
                                           value)) {
      value = bOverridden ? *saved : property.defaultValue;
    }

    // Kept the same width whether or not the button shows, so rows do not jump.
    const float resetWidth = ImGui::GetFrameHeight();
    bool bReset = false;
    if (bOverridden) {
      bReset = ImGui::Button(ICON_FA_UNDO, ImVec2(resetWidth, 0.0f));
      ImGui::SetItemTooltip("Back to the script's default");
    }
    else {
      ImGui::Dummy(ImVec2(resetWidth, ImGui::GetFrameHeight()));
    }
    ImGui::SameLine();

    const PropertyWidgetDesc desc{.label = property.name.c_str(),
                                  .minimum = property.minimum,
                                  .maximum = property.maximum};
    if (PropertyWidgetsUI::render(desc, value, owner)) {
      EditorScene::recordChange(owner, "Edit Script Value");
      component.setOverride(property.name, value);
    }
    if (!property.tooltip.empty()) {
      ImGui::SetItemTooltip("%s", property.tooltip.c_str());
    }
    if (bReset) {
      EditorScene::recordChange(owner, "Reset Script Value");
      static_cast<void>(component.removeOverride(property.name));
    }
    ImGui::PopID();
  }
  renderUnusedOverrides(component, *scriptClass);
}

} // namespace chEngineSDK
