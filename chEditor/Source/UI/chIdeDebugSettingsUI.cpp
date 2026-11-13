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

#include "chEnginePaths.h"
#include "chFileSystem.h"
#include "chLogger.h"
#include "chProjectDescriptor.h"

#include "imgui.h"

#include "../../Content/Fonts/IconsFontAwsome5.h"

#include <cfloat>

namespace chEngineSDK {

CH_LOG_DECLARE_STATIC(IdeDebugSettingsLog, All);

namespace {
constexpr const ANSICHAR* kPopupName = "Generate IDE Debug Settings";
constexpr const ANSICHAR* kBeginMarker = "// chimera-generated begin ";
constexpr const ANSICHAR* kEndMarker = "// chimera-generated end ";
constexpr ImVec4 kErrorColor(1.0f, 0.4f, 0.4f, 1.0f);
constexpr ImVec4 kSuccessColor(0.5f, 0.9f, 0.5f, 1.0f);

#if USING(CH_PLATFORM_WIN32)
constexpr const ANSICHAR* kExecutableExtension = ".exe";
#else
constexpr const ANSICHAR* kExecutableExtension = "";
#endif // USING(CH_PLATFORM_WIN32)

struct ResultLine
{
  bool bSuccess = false;
  String text;
};

struct DialogState
{
  bool bOpenRequested = false;
  bool bVSCode = true;
#if USING(CH_PLATFORM_WIN32)
  bool bVisualStudioSolution = true;
  bool bVisualStudioFolder = true;
#endif // USING(CH_PLATFORM_WIN32)
  Vector<ResultLine> results;
};

DialogState&
getState()
{
  static DialogState state;
  return state;
}

/*
 * What every generated entry needs to know about the project and the build that runs now.
 */
struct LaunchTarget
{
  String projectName;
  String projectFile;
  Path executableDirectory;
  bool bHasGame = false;
};

String
escapeJson(StringView text)
{
  String result;
  result.reserve(text.size());
  for (const ANSICHAR c : text) {
    if (c == '"' || c == '\\') {
      result += '\\';
    }
    result += c;
  }
  return result;
}

String
escapeXml(StringView text)
{
  String result;
  result.reserve(text.size());
  for (const ANSICHAR c : text) {
    switch (c) {
      case '&': result += "&amp;"; break;
      case '<': result += "&lt;"; break;
      case '>': result += "&gt;"; break;
      case '"': result += "&quot;"; break;
      default: result += c; break;
    }
  }
  return result;
}

String
readText(const Path& path)
{
  const Vector<uint8> bytes = FileSystem::fastRead(path);
  return String(reinterpret_cast<const ANSICHAR*>(bytes.data()), bytes.size());
}

bool
writeText(const Path& path, const String& text)
{
  const SPtr<DataStream> stream = FileSystem::createAndOpenFile(path);
  return stream && stream->write(text.data(), text.size()) == text.size();
}

/*
 * Skips spaces and comments, which launch files may hold, so the caller sees the next value.
 */
SIZE_T
skipJsonSpace(const String& text, SIZE_T position)
{
  while (position < text.size()) {
    const ANSICHAR c = text[position];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      ++position;
    }
    else if (text.compare(position, 2, "//") == 0) {
      const SIZE_T lineEnd = text.find('\n', position);
      position = lineEnd == String::npos ? text.size() : lineEnd + 1;
    }
    else if (text.compare(position, 2, "/*") == 0) {
      const SIZE_T commentEnd = text.find("*/", position + 2);
      position = commentEnd == String::npos ? text.size() : commentEnd + 2;
    }
    else {
      break;
    }
  }
  return position;
}

/*
 * Removes the lines of an earlier block of this project, markers included.
 */
void
removeMarkedBlock(String& text, const String& key)
{
  const SIZE_T begin = text.find(kBeginMarker + key);
  if (begin == String::npos) {
    return;
  }
  const SIZE_T end = text.find(kEndMarker + key, begin);
  if (end == String::npos) {
    return;
  }
  const SIZE_T lineStart = text.rfind('\n', begin);
  const SIZE_T removeStart = lineStart == String::npos ? 0 : lineStart + 1;
  const SIZE_T lineEnd = text.find('\n', end);
  const SIZE_T removeEnd = lineEnd == String::npos ? text.size() : lineEnd + 1;
  text.erase(removeStart, removeEnd - removeStart);
}

/*
 * Puts the project's entries first in the "configurations" array. The block is replaced
 * whole on the next run, so entries of other projects and the user's own stay as they are.
 */
bool
insertMarkedBlock(String& text, const String& key, const Vector<String>& entries)
{
  removeMarkedBlock(text, key);
  const SIZE_T name = text.find("\"configurations\"");
  if (name == String::npos) {
    return false;
  }
  const SIZE_T arrayStart = text.find('[', name);
  if (arrayStart == String::npos) {
    return false;
  }
  const SIZE_T next = skipJsonSpace(text, arrayStart + 1);
  // JSON has no trailing comma, so the last entry only gets one when more follow.
  const bool bMoreFollow = next < text.size() && text[next] != ']';

  String block = "\n    ";
  block += kBeginMarker + key + "\n";
  for (SIZE_T i = 0; i < entries.size(); ++i) {
    block += entries[i];
    block += (i + 1 < entries.size() || bMoreFollow) ? ",\n" : "\n";
  }
  block += "    ";
  block += kEndMarker + key;

  // Keeps the line break that followed the "[", so the block sits on its own lines.
  text.insert(arrayStart + 1, block);
  return true;
}

/*
 * Writes the entries into a launch file of VS Code or Visual Studio, made with header when it
 * does not exist yet.
 */
ResultLine
writeLaunchFile(const Path& path, const String& header, const String& key,
                const Vector<String>& entries)
{
  String text = FileSystem::isFile(path) ? readText(path) : String();
  if (text.empty()) {
    text = header + "  \"configurations\": [\n  ]\n}\n";
  }
  if (!insertMarkedBlock(text, key, entries)) {
    return {false, path.toString() + " has no \"configurations\" list; it was not changed."};
  }
  if (!writeText(path, text)) {
    return {false, "Could not write " + path.toString() + "."};
  }
  return {true, "Wrote " + path.toString()};
}

String
makeVSCodeEntry(const LaunchTarget& target, StringView label, StringView program)
{
  const String executable =
      escapeJson((target.executableDirectory / (String(program) + kExecutableExtension))
                     .toString());
#if USING(CH_PLATFORM_WIN32)
  constexpr const ANSICHAR* kDebugger = "      \"type\": \"cppvsdbg\",\n";
  constexpr const ANSICHAR* kConsole = "      \"console\": \"internalConsole\"\n";
#else
  constexpr const ANSICHAR* kDebugger = "      \"type\": \"cppdbg\",\n";
  constexpr const ANSICHAR* kConsole = "      \"MIMode\": \"gdb\"\n";
#endif // USING(CH_PLATFORM_WIN32)
  String entry = "    {\n";
  entry += "      \"name\": \"" + String(label) + ": " + escapeJson(target.projectName) + "\",\n";
  entry += kDebugger;
  entry += "      \"request\": \"launch\",\n";
  entry += "      \"program\": \"" + executable + "\",\n";
  entry += "      \"args\": [\"-project=" + escapeJson(target.projectFile) + "\"],\n";
  entry += "      \"cwd\": \"${workspaceFolder}\",\n";
  entry += kConsole;
  entry += "    }";
  return entry;
}

ResultLine
writeVSCode(const LaunchTarget& target)
{
  Vector<String> entries;
  entries.push_back(makeVSCodeEntry(target, "Editor", "chEditor"));
  if (target.bHasGame) {
    entries.push_back(makeVSCodeEntry(target, "Game", "chGame"));
  }
  const Path path = EnginePaths::getEngineRootDirectory() / ".vscode/launch.json";
  return writeLaunchFile(path, "{\n  \"version\": \"0.2.0\",\n", target.projectFile, entries);
}

#if USING(CH_PLATFORM_WIN32)
/*
 * Visual Studio's Open Folder mode builds with its own CMake, so the entry names the target
 * as its Startup Item list does; the single-config presets put it in lib/.
 */
String
makeVisualStudioFolderEntry(const LaunchTarget& target, StringView label, StringView program)
{
  const String exe = String(program) + kExecutableExtension;
  String entry = "    {\n";
  entry += "      \"type\": \"default\",\n";
  entry += "      \"project\": \"CMakeLists.txt\",\n";
  entry += "      \"projectTarget\": \"" + exe + " (lib\\\\" + exe + ")\",\n";
  entry += "      \"name\": \"" + String(label) + ": " + escapeJson(target.projectName) + "\",\n";
  entry += "      \"args\": [\"-project=" + escapeJson(target.projectFile) + "\"]\n";
  entry += "    }";
  return entry;
}

ResultLine
writeVisualStudioFolder(const LaunchTarget& target)
{
  Vector<String> entries;
  entries.push_back(makeVisualStudioFolderEntry(target, "Editor", "chEditor"));
  entries.push_back(makeVisualStudioFolderEntry(target, "Game", "chGame"));
  const Path path = EnginePaths::getEngineRootDirectory() / ".vs/launch.vs.json";
  return writeLaunchFile(path, "{\n  \"version\": \"0.2.1\",\n  \"defaults\": {},\n",
                         target.projectFile, entries);
}

/*
 * Sets the debugger arguments of a project of a solution. CMake never writes the .user file,
 * so the value survives a new configure; other settings in it are kept.
 */
ResultLine
writeVcxprojUser(const Path& path, const String& arguments)
{
  constexpr const ANSICHAR* kOpenTag = "<LocalDebuggerCommandArguments>";
  constexpr const ANSICHAR* kCloseTag = "</LocalDebuggerCommandArguments>";
  const String group = String("  <PropertyGroup>\n    ") + kOpenTag + arguments + kCloseTag +
                       "\n    <DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>\n"
                       "  </PropertyGroup>\n";

  String text = FileSystem::isFile(path) ? readText(path) : String();
  if (text.find(kOpenTag) != String::npos) {
    // Every configuration that sets its own arguments gets the project.
    SIZE_T position = 0;
    while ((position = text.find(kOpenTag, position)) != String::npos) {
      const SIZE_T valueStart = position + std::char_traits<ANSICHAR>::length(kOpenTag);
      const SIZE_T valueEnd = text.find(kCloseTag, valueStart);
      if (valueEnd == String::npos) {
        break;
      }
      text.replace(valueStart, valueEnd - valueStart, arguments);
      position = valueStart + arguments.size();
    }
  }
  else if (const SIZE_T projectEnd = text.rfind("</Project>"); projectEnd != String::npos) {
    text.insert(projectEnd, group);
  }
  else {
    text = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
           "<Project ToolsVersion=\"Current\" "
           "xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\n" +
           group + "</Project>\n";
  }
  if (!writeText(path, text)) {
    return {false, "Could not write " + path.toString() + "."};
  }
  return {true, "Wrote " + path.toString()};
}

/*
 * Every build folder made with the Visual Studio generator: the one this editor runs from
 * (<build>/lib/<Config>) and the ones under <EngineRoot>/build.
 */
Vector<Path>
findSolutionFolders()
{
  Vector<Path> candidates;
  const Path executableDirectory = FileSystem::getExecutableDirectory();
  candidates.push_back(executableDirectory.getDirectory().getDirectory());
  Vector<Path> files;
  FileSystem::getChildren(EnginePaths::getEngineRootDirectory() / "build", files, candidates);

  Vector<Path> result;
  Vector<String> seen;
  for (const Path& candidate : candidates) {
    const Path folder = FileSystem::absolutePath(candidate);
    bool bSeen = false;
    for (const String& other : seen) {
      bSeen = bSeen || other == folder.toString();
    }
    if (!bSeen && FileSystem::isFile(folder / "chEditor/chEditor.vcxproj")) {
      seen.push_back(folder.toString());
      result.push_back(folder);
    }
  }
  return result;
}

void
writeVisualStudioSolutions(const LaunchTarget& target, Vector<ResultLine>& results)
{
  const Vector<Path> folders = findSolutionFolders();
  if (folders.empty()) {
    results.push_back({false, "No Visual Studio solution found under build/ (configure the "
                              "windows-msvc preset first)."});
    return;
  }
  const String arguments = escapeXml("-project=\"" + target.projectFile + "\"");
  for (const Path& folder : folders) {
    for (const ANSICHAR* program : {"chEditor", "chGame"}) {
      const Path project = folder / (String(program) + "/" + program + ".vcxproj");
      if (FileSystem::isFile(project)) {
        results.push_back(writeVcxprojUser(Path(project.toString() + ".user"), arguments));
      }
    }
  }
}
#endif // USING(CH_PLATFORM_WIN32)

void
generate(DialogState& state)
{
  state.results.clear();
  LaunchTarget target;
  target.projectFile = FileSystem::absolutePath(EnginePaths::getProjectFile()).toString();
  target.projectName = EnginePaths::getProjectDescriptor().name;
  if (target.projectName.empty()) {
    target.projectName = EnginePaths::getProjectFile().getFileName(false);
  }
  target.executableDirectory = FileSystem::getExecutableDirectory();
  target.bHasGame =
      FileSystem::isFile(target.executableDirectory / (String("chGame") + kExecutableExtension));

  if (state.bVSCode) {
    state.results.push_back(writeVSCode(target));
  }
#if USING(CH_PLATFORM_WIN32)
  if (state.bVisualStudioSolution) {
    writeVisualStudioSolutions(target, state.results);
  }
  if (state.bVisualStudioFolder) {
    state.results.push_back(writeVisualStudioFolder(target));
  }
#endif // USING(CH_PLATFORM_WIN32)

  for (const ResultLine& line : state.results) {
    if (line.bSuccess) {
      CH_LOG_INFO(IdeDebugSettingsLog, "{0}", line.text);
    }
    else {
      CH_LOG_WARNING(IdeDebugSettingsLog, "{0}", line.text);
    }
  }
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
                     "(-project), so the debugger stays on the process it started.");
  ImGui::Spacing();
  ImGui::Checkbox("VS Code (.vscode/launch.json)", &state.bVSCode);
#if USING(CH_PLATFORM_WIN32)
  ImGui::Checkbox("Visual Studio solution (build/*/chEditor.vcxproj.user)",
                  &state.bVisualStudioSolution);
  ImGui::Checkbox("Visual Studio Open Folder (.vs/launch.vs.json)", &state.bVisualStudioFolder);
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
    for (const ResultLine& line : state.results) {
      ImGui::PushStyleColor(ImGuiCol_Text, line.bSuccess ? kSuccessColor : kErrorColor);
      ImGui::TextWrapped("%s", line.text.c_str());
      ImGui::PopStyleColor();
    }
    ImGui::TextDisabled("Reload the solution or launch list if the IDE has it open.");
  }
  ImGui::EndPopup();
}

} // namespace chEngineSDK
