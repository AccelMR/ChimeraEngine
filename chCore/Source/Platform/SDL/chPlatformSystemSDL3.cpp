/************************************************************************/
/**
 * @file chPlatformSystemSDL3.cpp
 * @author AccelMR
 * @date 2026/10/22
 * @brief
 *  SDL3 implementation of PlatformSystem.
 */
/************************************************************************/
#include "chPlatformSystem.h"

#if USING(CH_DISPLAY_SDL3)

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_misc.h>
#include <SDL3/SDL_process.h>
#include <SDL3/SDL_stdinc.h>

#include "chLogger.h"

namespace chEngineSDK {

CH_LOG_DECLARE_STATIC(PlatformSystemLog, All);

/*
 */
bool
PlatformSystem::launchProcess(const Vector<String>& arguments)
{
  if (arguments.empty()) {
    return false;
  }

  Vector<const ANSICHAR*> argumentPointers;
  argumentPointers.reserve(arguments.size() + 1);
  for (const String& argument : arguments) {
    argumentPointers.push_back(argument.c_str());
  }
  argumentPointers.push_back(nullptr);

  SDL_Process* process = SDL_CreateProcess(argumentPointers.data(), false);
  if (!process) {
    CH_LOG_ERROR(PlatformSystemLog, "Could not start {0}: {1}", arguments[0], SDL_GetError());
    return false;
  }
  // Only the handle goes; the process keeps running.
  SDL_DestroyProcess(process);
  return true;
}

/*
 */
Path
PlatformSystem::getUserDataDirectory(StringView organization, StringView application)
{
  const String organizationText(organization);
  const String applicationText(application);
  ANSICHAR* folder = SDL_GetPrefPath(organizationText.c_str(), applicationText.c_str());
  if (!folder) {
    CH_LOG_ERROR(PlatformSystemLog, "Could not find the user data folder: {0}",
                 SDL_GetError());
    return Path();
  }
  String text(folder);
  SDL_free(folder);
  // SDL ends the folder with a separator, which joined paths would double.
  while (text.size() > 1 && (text.back() == '/' || text.back() == '\\')) {
    text.pop_back();
  }
  return Path(std::move(text));
}

/*
 */
void
PlatformSystem::showCrashDialog(const ANSICHAR* applicationName, const ANSICHAR* folder)
{
  ANSICHAR title[128];
  CrashReportWriter titleWriter(title, sizeof(title) - 1);
  titleWriter.append(applicationName);
  titleWriter.append(" crashed");
  title[titleWriter.getText().size()] = '\0';

  ANSICHAR message[2048];
  CrashReportWriter messageWriter(message, sizeof(message) - 1);
  messageWriter.append("The program stopped because of an error.\n\n"
                       "A crash report was saved in:\n");
  messageWriter.append(folder);
  messageWriter.append("\n\nPlease send this folder with a few words about what you were "
                       "doing.");
  message[messageWriter.getText().size()] = '\0';

  static constexpr int32 kOpenFolder = 1;
  const SDL_MessageBoxButtonData buttons[] = {
      {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT,
       0,
       "Close"},
      {0, kOpenFolder, "Open Folder"},
  };
  SDL_MessageBoxData data{};
  data.flags = SDL_MESSAGEBOX_ERROR;
  data.title = title;
  data.message = message;
  data.numbuttons = static_cast<int32>(sizeof(buttons) / sizeof(buttons[0]));
  data.buttons = buttons;

  int32 pressed = 0;
  if (!SDL_ShowMessageBox(&data, &pressed) || pressed != kOpenFolder) {
    return;
  }

  ANSICHAR url[1100];
  CrashReportWriter urlWriter(url, sizeof(url) - 1);
  urlWriter.append(folder[0] == '/' ? "file://" : "file:///");
  urlWriter.append(folder);
  url[urlWriter.getText().size()] = '\0';
  for (SIZE_T i = 0; url[i] != '\0'; ++i) {
    if (url[i] == '\\') {
      url[i] = '/';
    }
  }
  SDL_OpenURL(url);
}

} // namespace chEngineSDK

#endif // USING(CH_DISPLAY_SDL3)
