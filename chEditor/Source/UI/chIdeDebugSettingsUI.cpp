/************************************************************************/
/**
 * @file chIdeDebugSettingsUI.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Writes debugger launch settings for the open project into the IDEs.
 */
/************************************************************************/
#include "chIdeDebugSettingsUI.h"

#include "chIdeDebugSettings.h"

#include "imgui.h"

#include "../../Content/Fonts/IconsFontAwsome5.h"

#include <cfloat>

namespace chEngineSDK {

namespace {
constexpr const ANSICHAR* kPopupName = "Generate IDE Debug Settings";
constexpr ImVec4 kErrorColor(1.0f, 0.4f, 0.4f, 1.0f);
constexpr ImVec4 kWarningColor(1.0f, 0.8f, 0.4f, 1.0f);
constexpr ImVec4 kSuccessColor(0.5f, 0.9f, 0.5f, 1.0f);

#if !USING(CH_PLATFORM_WIN32)
constexpr const ANSICHAR* kDebuggers[] = {"gdb", "lldb"};
#endif // !USING(CH_PLATFORM_WIN32)

struct DialogState
{
  bool bOpenRequested = false;
  IdeDebugSettingsOptions options;
#if !USING(CH_PLATFORM_WIN32)
  int32 debuggerIndex = 0;
#endif // !USING(CH_PLATFORM_WIN32)
  Vector<IdeDebugSettingsResult> results;
};

DialogState&
getState()
{
  static DialogState state;
  return state;
}

ImVec4
getResultColor(IdeDebugSettingsResultKind kind)
{
  switch (kind) {
    case IdeDebugSettingsResultKind::Written: return kSuccessColor;
    case IdeDebugSettingsResultKind::Warning: return kWarningColor;
    case IdeDebugSettingsResultKind::Error: return kErrorColor;
  }
  return kErrorColor;
}

void
generate(DialogState& state)
{
  state.results.clear();
#if !USING(CH_PLATFORM_WIN32)
  state.options.debugger = kDebuggers[state.debuggerIndex];
#endif // !USING(CH_PLATFORM_WIN32)
  IdeDebugSettings::generate(state.options, state.results);
}
} // namespace

/*
 */
void
IdeDebugSettingsUI::open()
{
  DialogState& state = getState();
  state.bOpenRequested = true;
  state.results.clear();
}

/*
 */
void
IdeDebugSettingsUI::render()
{
  DialogState& state = getState();
  if (state.bOpenRequested) {
    ImGui::OpenPopup(kPopupName);
    state.bOpenRequested = false;
  }

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  const float width = ImGui::GetFontSize() * 36.0f;
  ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f), ImVec2(width, FLT_MAX));
  bool bOpen = true;
  if (!ImGui::BeginPopupModal(kPopupName, &bOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }

  ImGui::TextWrapped("Adds launch settings that start chEditor and chGame on this project "
                     "(-project), so the debugger stays on the process it started. VS Code "
                     "also gets \"Game (Release, no editor)\", which builds and runs the game "
                     "preset that Package Project copies.");
  ImGui::Spacing();
  ImGui::Checkbox("VS Code (.vscode/launch.json, tasks.json)", &state.options.bVSCode);
  ImGui::Indent();
  ImGui::BeginDisabled(!state.options.bVSCode);
  ImGui::Checkbox("Follow the CMake Tools tab", &state.options.bFollowCMakeTools);
  ImGui::SetItemTooltip("On: Editor and Game build with the build preset selected in the CMake "
                        "Tools tab and start from its build folder, so changing the preset "
                        "there needs no new settings. Needs the CMake Tools extension with "
                        "presets and a launch target selected.\nOff: Debug and Release entries "
                        "fixed to the build folders found next to this editor.");
  ImGui::EndDisabled();
  ImGui::Unindent();
#if USING(CH_PLATFORM_WIN32)
  ImGui::Checkbox("Visual Studio solution (build/*/chEditor.vcxproj.user)",
                  &state.options.bVisualStudioSolution);
  ImGui::Checkbox("Visual Studio Open Folder (.vs/launch.vs.json)", &state.options.bVisualStudioFolder);
#else
  ImGui::BeginDisabled(!state.options.bVSCode);
  ImGui::TextUnformatted("Debugger:");
  ImGui::SameLine();
  ImGui::RadioButton("gdb", &state.debuggerIndex, 0);
  ImGui::SameLine();
  ImGui::RadioButton("lldb", &state.debuggerIndex, 1);
  ImGui::EndDisabled();
#endif // USING(CH_PLATFORM_WIN32)
  ImGui::TextDisabled("Each project keeps its own entries; the rest of each file is kept.");
  ImGui::Spacing();

  if (ImGui::Button(ICON_FA_COGS "  Generate")) {
    generate(state);
  }
  ImGui::SameLine();
  if (ImGui::Button("Close")) {
    ImGui::CloseCurrentPopup();
  }

  if (!state.results.empty()) {
    ImGui::Separator();
    for (const IdeDebugSettingsResult& line : state.results) {
      ImGui::PushStyleColor(ImGuiCol_Text, getResultColor(line.kind));
      ImGui::TextWrapped("%s", line.text.c_str());
      ImGui::PopStyleColor();
    }
    ImGui::TextDisabled("Reload the solution or launch list if the IDE has it open.");
  }
  ImGui::EndPopup();
}

} // namespace chEngineSDK
