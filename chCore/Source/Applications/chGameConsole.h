/************************************************************************/
/**
 * @file chGameConsole.h
 * @author AccelMR
 * @date 2026/11/23
 * @brief The console a running game opens over its image.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#if USING(CH_CONSOLE)

#include "chConsole.h"
#include "chLogger.h"
#include "chSTDThreading.h"
#include "chTextLayout.h"

namespace chEngineSDK {

class ScreenOverlay;
struct KeyEvent;

/**
 * Exists so a game without the editor can run console commands and see its log: a panel over
 * the top of the image, opened with Console.ToggleKey, showing the last log lines and a line
 * to type in. While it is open the game gets no keyboard keys (text input is on). It draws
 * into a ScreenOverlay the application passes to the renderer. Needs the Logger and the
 * InputManager started; main thread only, except the log, which may come from any thread.
 */
class CH_CORE_EXPORT GameConsole
{
 public:
  GameConsole();

  ~GameConsole();

  GameConsole(const GameConsole&) = delete;

  GameConsole&
  operator=(const GameConsole&) = delete;

  /**
   * Takes the log lines written since the last call. Once per frame.
   */
  void
  update(float deltaTime);

  /**
   * Adds the panel to overlay when the console is open; width and height are the window's.
   */
  void
  draw(ScreenOverlay& overlay, float width, float height);

  void
  open();

  void
  close();

  NODISCARD FORCEINLINE bool
  isOpen() const noexcept
  {
    return m_bOpen;
  }

  /**
   * Forgets every line shown; the log itself keeps them.
   */
  void
  clear();

 private:
  void
  onKeyPressed(const KeyEvent& event);

  void
  onTextKey(const KeyEvent& event);

  void
  onText(StringView text);

  void
  runInput();

  void
  stepHistory(int32 direction);

  void
  completeInput();

  void
  setInput(StringView text);

  void
  updateSuggestions();

  NODISCARD SIZE_T
  countLines() const;

  static constexpr int32 NO_HISTORY = -1;

  // Log entries from any thread wait here until update.
  Mutex m_pendingMutex;
  Vector<SPtr<const LogBufferEntry>> m_pendingEntries;
  Vector<SPtr<const LogBufferEntry>> m_flushEntries;
  // Oldest first.
  Vector<SPtr<const LogBufferEntry>> m_entries;
  HEvent m_logEvent;
  HEvent m_keyEvent;
  HEvent m_textKeyEvent;
  HEvent m_textEvent;

  ConsoleCommand m_clearCommand;
  String m_input;
  // Byte offset of the caret in m_input, always at the start of a character.
  SIZE_T m_cursor = 0;
  int32 m_historyIndex = NO_HISTORY;
  // Lines scrolled up from the newest.
  SIZE_T m_scroll = 0;
  float m_caretTime = 0.0f;
  bool m_bOpen = false;
  // The key that opened it may still type its character in the same frame.
  bool m_bIgnoreText = false;

  Vector<String> m_completion;
  Vector<String> m_suggestions;
  TextLayout m_layout;
};

} // namespace chEngineSDK

#endif // USING(CH_CONSOLE)
