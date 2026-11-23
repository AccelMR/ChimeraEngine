/************************************************************************/
/**
 * @file chGameConsole.cpp
 * @author AccelMR
 * @date 2026/11/23
 * @brief The console a running game opens over its image.
 */
/************************************************************************/
#include "chGameConsole.h"

#if USING(CH_CONSOLE)

#include "chConsoleVariable.h"
#include "chDisplayEvent.h"
#include "chFont.h"
#include "chInputManager.h"
#include "chMath.h"
#include "chScreenOverlay.h"

namespace chEngineSDK {
namespace {
ConsoleVariable<String> g_cvarToggleKey(
    "Console.ToggleKey",
    "Grave",
    "Key that opens and closes the game's console, by its Input.ini name (Grave is the key "
    "left of 1 on any layout).");
ConsoleVariable<float> g_cvarHeight("Console.Height",
                                    0.5f,
                                    "Part of the window the game's console covers, 0.1 to 1.");
ConsoleVariable<float> g_cvarFontSize("Console.FontSize",
                                      22.0f,
                                      "Height of the game's console text, in pixels.");
ConsoleVariable<int32> g_cvarMaxLines("Console.MaxLines",
                                      200,
                                      "Log lines the game's console keeps.");
ConsoleVariable<bool> g_cvarShowDebug("Console.ShowDebug",
                                      false,
                                      "Shows debug log lines in the game's console too.");

// Above anything a game draws in the overlay.
constexpr int16 kConsoleLayer = 30000;
constexpr float kPadding = 8.0f;
constexpr float kLineSpacing = 1.25f;
constexpr float kCaretBlinkSeconds = 0.5f;
constexpr SIZE_T kMaxShownSuggestions = 8;
constexpr LinearColor kBackgroundColor(0.003f, 0.003f, 0.006f, 0.85f);
constexpr LinearColor kInputColor(0.015f, 0.015f, 0.025f, 0.95f);
constexpr LinearColor kSuggestionColor(0.025f, 0.025f, 0.045f, 0.95f);

NODISCARD LinearColor
getVerbosityColor(const LogVerbosity verbosity) noexcept
{
  switch (verbosity) {
    case LogVerbosity::Fatal:
    case LogVerbosity::Error:
      return LinearColor(1.0f, 0.3f, 0.3f, 1.0f);
    case LogVerbosity::Warning:
      return LinearColor(1.0f, 0.75f, 0.25f, 1.0f);
    case LogVerbosity::Debug:
      return LinearColor(0.5f, 0.5f, 0.55f, 1.0f);
    default:
      return LinearColor(0.85f, 0.85f, 0.88f, 1.0f);
  }
}

NODISCARD bool
isContinuationByte(const ANSICHAR character) noexcept
{
  return (static_cast<uint8>(character) & 0xC0u) == 0x80u;
}

/**
 * Calls lineFunction(line) for each line of text, last first.
 */
template<typename LineFunction>
void
forEachLineBackwards(StringView text, LineFunction lineFunction)
{
  SIZE_T end = text.size();
  while (true) {
    const SIZE_T newline = end == 0 ? StringView::npos : text.rfind('\n', end - 1);
    const SIZE_T start = newline == StringView::npos ? 0 : newline + 1;
    if (!lineFunction(text.substr(start, end - start))) {
      return;
    }
    if (newline == StringView::npos) {
      return;
    }
    end = newline;
  }
}

NODISCARD SIZE_T
countTextLines(StringView text) noexcept
{
  SIZE_T lines = 1;
  for (const ANSICHAR character : text) {
    lines += character == '\n' ? 1 : 0;
  }
  return lines;
}

NODISCARD bool
isShown(const LogBufferEntry& entry) noexcept
{
  return entry.verbosity != LogVerbosity::Debug || g_cvarShowDebug.get();
}
} // namespace

/*
 */
GameConsole::GameConsole()
 : m_clearCommand("clear", "", "Clears the lines of the console it is typed in.",
                  [this](const ConsoleArguments&) { clear(); })
{
  m_logEvent = Logger::instance().onLogWritten(
      [this](const SPtr<const LogBufferEntry>& entry) {
        LockGuard<Mutex> lock(m_pendingMutex);
        m_pendingEntries.push_back(entry);
      },
      true);
  InputManager& input = InputManager::instance();
  m_keyEvent =
      input.OnKeyPressed.connect([this](const KeyEvent& event) { onKeyPressed(event); });
  m_textKeyEvent =
      input.OnTextKey.connect([this](const KeyEvent& event) { onTextKey(event); });
  m_textEvent = input.OnTextInput.connect([this](StringView text) { onText(text); });
}

/*
 */
GameConsole::~GameConsole()
{
  Logger::disconnectLogListener(m_logEvent);
  m_keyEvent.disconnect();
  m_textKeyEvent.disconnect();
  m_textEvent.disconnect();
  if (m_bOpen && InputManager::isStarted()) {
    InputManager::instance().stopTextInput();
  }
}

/*
 */
void
GameConsole::update(const float deltaTime)
{
  m_bIgnoreText = false;
  m_caretTime += deltaTime;
  {
    LockGuard<Mutex> lock(m_pendingMutex);
    m_pendingEntries.swap(m_flushEntries);
  }
  if (m_flushEntries.empty()) {
    return;
  }
  for (SPtr<const LogBufferEntry>& entry : m_flushEntries) {
    m_entries.push_back(std::move(entry));
  }
  m_flushEntries.clear();
  const SIZE_T maxLines = static_cast<SIZE_T>(Math::max(g_cvarMaxLines.get(), 1));
  if (m_entries.size() > maxLines) {
    m_entries.erase(m_entries.begin(),
                    m_entries.begin() + static_cast<int64>(m_entries.size() - maxLines));
  }
}

/*
 */
void
GameConsole::open()
{
  if (m_bOpen) {
    return;
  }
  m_bOpen = true;
  m_bIgnoreText = true;
  m_caretTime = 0.0f;
  m_scroll = 0;
  InputManager::instance().startTextInput();
}

/*
 */
void
GameConsole::close()
{
  if (!m_bOpen) {
    return;
  }
  m_bOpen = false;
  InputManager::instance().stopTextInput();
}

/*
 */
void
GameConsole::clear()
{
  // Also what waits, such as the echo of the clear command itself.
  {
    LockGuard<Mutex> lock(m_pendingMutex);
    m_pendingEntries.clear();
  }
  m_entries.clear();
  m_scroll = 0;
}

/*
 */
void
GameConsole::onKeyPressed(const KeyEvent& event)
{
  if (!m_bOpen && event.key == InputKeyUtils::fromName(g_cvarToggleKey.get())) {
    open();
  }
}

/*
 */
void
GameConsole::onTextKey(const KeyEvent& event)
{
  if (!m_bOpen) {
    return;
  }
  m_caretTime = 0.0f;
  const InputKey key = event.key;
  if (key == InputKey::Escape ||
      (!event.bRepeat && key == InputKeyUtils::fromName(g_cvarToggleKey.get()))) {
    close();
    return;
  }

  switch (key) {
    case InputKey::Enter:
    case InputKey::NumpadEnter:
      runInput();
      break;
    case InputKey::Backspace:
      if (m_cursor > 0) {
        SIZE_T start = m_cursor - 1;
        while (start > 0 && isContinuationByte(m_input[start])) {
          --start;
        }
        m_input.erase(start, m_cursor - start);
        m_cursor = start;
        m_historyIndex = NO_HISTORY;
        updateSuggestions();
      }
      break;
    case InputKey::Delete:
      if (m_cursor < m_input.size()) {
        SIZE_T end = m_cursor + 1;
        while (end < m_input.size() && isContinuationByte(m_input[end])) {
          ++end;
        }
        m_input.erase(m_cursor, end - m_cursor);
        m_historyIndex = NO_HISTORY;
        updateSuggestions();
      }
      break;
    case InputKey::Left:
      while (m_cursor > 0) {
        --m_cursor;
        if (!isContinuationByte(m_input[m_cursor])) {
          break;
        }
      }
      break;
    case InputKey::Right:
      while (m_cursor < m_input.size()) {
        ++m_cursor;
        if (m_cursor == m_input.size() || !isContinuationByte(m_input[m_cursor])) {
          break;
        }
      }
      break;
    case InputKey::Home:
      m_cursor = 0;
      break;
    case InputKey::End:
      m_cursor = m_input.size();
      break;
    case InputKey::Up:
      stepHistory(-1);
      break;
    case InputKey::Down:
      stepHistory(1);
      break;
    case InputKey::Tab:
      completeInput();
      break;
    case InputKey::PageUp:
      m_scroll = Math::min(m_scroll + 10, countLines() > 0 ? countLines() - 1 : 0);
      break;
    case InputKey::PageDown:
      m_scroll = m_scroll > 10 ? m_scroll - 10 : 0;
      break;
    case InputKey::L:
      if (event.hasControl()) {
        clear();
      }
      break;
    default:
      break;
  }
}

/*
 */
void
GameConsole::onText(StringView text)
{
  if (!m_bOpen || m_bIgnoreText) {
    return;
  }
  // A held Ctrl means a shortcut (Ctrl+L), not text.
  if (InputManager::instance().getState().isDown(InputKey::LeftControl) ||
      InputManager::instance().getState().isDown(InputKey::RightControl)) {
    return;
  }
  m_input.insert(m_cursor, text);
  m_cursor += text.size();
  m_historyIndex = NO_HISTORY;
  m_caretTime = 0.0f;
  updateSuggestions();
}

/*
 */
void
GameConsole::runInput()
{
  if (m_input.empty()) {
    return;
  }
  // Moved out first: the command may close the console or clear it.
  const String line = std::move(m_input);
  setInput({});
  m_scroll = 0;
  Console::execute(line);
}

/*
 */
void
GameConsole::stepHistory(const int32 direction)
{
  const Vector<String>& history = Console::getHistory();
  if (history.empty()) {
    return;
  }
  const int32 last = static_cast<int32>(history.size()) - 1;
  if (direction < 0) {
    m_historyIndex = m_historyIndex == NO_HISTORY ? last : Math::max(m_historyIndex - 1, 0);
  }
  else {
    if (m_historyIndex == NO_HISTORY) {
      return;
    }
    ++m_historyIndex;
    if (m_historyIndex > last) {
      m_historyIndex = NO_HISTORY;
    }
  }
  const int32 index = m_historyIndex;
  setInput(index == NO_HISTORY ? StringView()
                               : StringView(history[static_cast<SIZE_T>(index)]));
  m_historyIndex = index;
}

/*
 */
void
GameConsole::completeInput()
{
  // Only the name is completed.
  if (m_input.empty() || m_input.find_first_of(" \t") != String::npos) {
    return;
  }
  Console::complete(m_input, m_completion);
  if (m_completion.empty()) {
    return;
  }
  if (m_completion.size() == 1) {
    setInput(m_completion[0] + " ");
    return;
  }
  // What every match starts with, ignoring case, written as the first match writes it.
  const String& first = m_completion[0];
  SIZE_T shared = first.size();
  for (const String& name : m_completion) {
    SIZE_T length = 0;
    while (length < shared && length < name.size() &&
           StringUtils::equalsIgnoreCase(StringView(name).substr(length, 1),
                                         StringView(first).substr(length, 1))) {
      ++length;
    }
    shared = length;
  }
  if (shared > m_input.size()) {
    setInput(StringView(first).substr(0, shared));
  }
}

/*
 */
void
GameConsole::setInput(StringView text)
{
  m_input = text;
  m_cursor = m_input.size();
  m_historyIndex = NO_HISTORY;
  updateSuggestions();
}

/*
 */
void
GameConsole::updateSuggestions()
{
  m_suggestions.clear();
  if (m_input.empty() || m_input.find_first_of(" \t") != String::npos) {
    return;
  }
  Console::complete(m_input, m_completion);
  const SIZE_T shown = Math::min(m_completion.size(), kMaxShownSuggestions);
  for (SIZE_T i = 0; i < shown; ++i) {
    const String& name = m_completion[i];
    if (const IConsoleVariable* variable = ConsoleVariables::find(name)) {
      m_suggestions.push_back(name + " = " + variable->toString());
    }
    else if (const ConsoleCommand* command = Console::findCommand(name)) {
      m_suggestions.push_back(name + " " + command->getUsage());
    }
  }
  if (m_completion.size() > shown) {
    m_suggestions.push_back(StringUtils::format("{0} more", m_completion.size() - shown));
  }
}

/*
 */
SIZE_T
GameConsole::countLines() const
{
  SIZE_T lines = 0;
  for (const SPtr<const LogBufferEntry>& entry : m_entries) {
    if (isShown(*entry)) {
      lines += countTextLines(entry->message);
    }
  }
  return lines;
}

/*
 */
void
GameConsole::draw(ScreenOverlay& overlay, const float width, const float height)
{
  if (!m_bOpen) {
    return;
  }
  const SPtr<Font>& font = Font::getDefault();
  if (!font) {
    return;
  }
  const float fontSize = Math::max(g_cvarFontSize.get(), 6.0f);
  const float lineHeight = fontSize * kLineSpacing;
  const float panelHeight = height * Math::clamp(g_cvarHeight.get(), 0.1f, 1.0f);
  const float barHeight = lineHeight + kPadding;
  const float barTop = panelHeight - barHeight;

  overlay.rectangle(Vector2::ZERO, Vector2(width, panelHeight), kBackgroundColor,
                    kConsoleLayer);
  overlay.rectangle(Vector2(0.0f, barTop), Vector2(width, barHeight), kInputColor,
                    kConsoleLayer, 1);

  // The typed line and its caret.
  const TextLayoutDesc lineDesc{.size = fontSize,
                                .pivot = Vector2(0.0f, 0.5f),
                                .bRichText = false,
                                .color = LinearColor::WHITE};
  const float barCenter = barTop + barHeight * 0.5f;
  String shown = "> ";
  shown += StringView(m_input).substr(0, m_cursor);
  m_layout.build(*font, shown, lineDesc);
  const float caretX = kPadding + m_layout.getSize().x;
  shown += StringView(m_input).substr(m_cursor);
  m_layout.build(*font, shown, lineDesc);
  overlay.text(*font, m_layout, Vector2(kPadding, barCenter), kConsoleLayer, 2);
  if (Math::fmod(m_caretTime, kCaretBlinkSeconds * 2.0f) < kCaretBlinkSeconds) {
    overlay.rectangle(Vector2(caretX, barCenter - fontSize * 0.5f), Vector2(2.0f, fontSize),
                      LinearColor::WHITE, kConsoleLayer, 3);
  }

  // The log, newest at the bottom, from m_scroll lines up.
  float y = barTop - kPadding * 0.5f;
  SIZE_T skipped = 0;
  for (SIZE_T i = m_entries.size(); i > 0 && y > 0.0f; --i) {
    const LogBufferEntry& entry = *m_entries[i - 1];
    if (!isShown(entry)) {
      continue;
    }
    const TextLayoutDesc desc{.size = fontSize,
                              .pivot = Vector2(0.0f, 0.0f),
                              .bRichText = false,
                              .color = getVerbosityColor(entry.verbosity)};
    forEachLineBackwards(entry.message, [&](StringView line) {
      if (skipped < m_scroll) {
        ++skipped;
        return true;
      }
      m_layout.build(*font, line, desc);
      overlay.text(*font, m_layout, Vector2(kPadding, y), kConsoleLayer, 2);
      y -= lineHeight;
      return y > 0.0f;
    });
  }
  if (m_scroll > 0) {
    m_layout.build(*font, StringUtils::format("{0} lines below (Page Down)", m_scroll),
                   {.size = fontSize * 0.8f,
                    .align = TextAlign::Right,
                    .pivot = Vector2(1.0f, 0.5f),
                    .bRichText = false,
                    .color = LinearColor(0.6f, 0.6f, 0.7f, 1.0f)});
    overlay.text(*font, m_layout, Vector2(width - kPadding, barCenter), kConsoleLayer, 2);
  }

  // Names that start with what is typed, in a box under the panel.
  if (!m_suggestions.empty()) {
    const float boxHeight = static_cast<float>(m_suggestions.size()) * lineHeight + kPadding;
    overlay.rectangle(Vector2(0.0f, panelHeight), Vector2(width * 0.5f, boxHeight),
                      kSuggestionColor, kConsoleLayer, 1);
    float suggestionY = panelHeight + kPadding * 0.5f;
    for (const String& suggestion : m_suggestions) {
      m_layout.build(*font, suggestion,
                     {.size = fontSize,
                      .pivot = Vector2(0.0f, 1.0f),
                      .bRichText = false,
                      .color = LinearColor(0.75f, 0.8f, 1.0f, 1.0f)});
      overlay.text(*font, m_layout, Vector2(kPadding, suggestionY), kConsoleLayer, 2);
      suggestionY += lineHeight;
    }
  }
}

} // namespace chEngineSDK

#endif // USING(CH_CONSOLE)
