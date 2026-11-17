/************************************************************************/
/**
 * @file chSystemTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of the event system, the Logger and the Profiler.
 */
/************************************************************************/
#include "chConsoleVariable.h"
#include "chEventSystem.h"
#include "chLogger.h"
#include "chProfiler.h"
#include "chSTDThreading.h"

#include "catch.hpp"

using namespace chEngineSDK;

TEST_CASE("chUtilities - EventSystem", "[Util]") {
  SECTION("More listeners than the inline capacity") {
    Event<void(int32)> onValue;
    int32 total = 0;
    Vector<HEvent> handles;
    for (int32 i = 0; i < 40; ++i) {
      handles.push_back(onValue.connect([&total](int32 value) { total += value; }));
    }
    onValue(2);
    REQUIRE(total == 80);
  }

  SECTION("Custom inline capacity") {
    Event<void(int32), 4> onValue;
    int32 total = 0;
    Vector<HEvent> handles;
    for (int32 i = 0; i < 4; ++i) {
      handles.push_back(onValue.connect([&total](int32 value) { total += value; }));
    }
    onValue(1);
    REQUIRE(total == 4);

    for (int32 i = 0; i < 6; ++i) {
      handles.push_back(onValue.connect([&total](int32 value) { total += value; }));
    }
    onValue(1);
    REQUIRE(total == 14);
  }

  SECTION("Arguments by value reach every listener") {
    Event<void(String)> onText;
    int32 matches = 0;
    HEvent first = onText.connect([&matches](String text) { matches += text == "hello"; });
    HEvent second = onText.connect([&matches](String text) { matches += text == "hello"; });
    onText(String("hello"));
    REQUIRE(matches == 2);
  }

  SECTION("Disconnect during a fire") {
    Event<void()> onFire;
    int32 firstCalls = 0;
    int32 secondCalls = 0;
    HEvent second;
    HEvent first = onFire.connect([&]() {
      ++firstCalls;
      first.disconnect();
      second.disconnect();
    });
    second = onFire.connect([&]() { ++secondCalls; });

    // The second listener was already pinned, so it still runs during this fire.
    onFire();
    REQUIRE(firstCalls == 1);
    REQUIRE(secondCalls == 1);

    onFire();
    REQUIRE(firstCalls == 1);
    REQUIRE(secondCalls == 1);
  }

  SECTION("Connect during a fire") {
    Event<void()> onFire;
    int32 lateCalls = 0;
    HEvent late;
    HEvent first = onFire.connect([&]() {
      if (!late.isValid()) {
        late = onFire.connect([&]() { ++lateCalls; });
      }
    });

    onFire();
    REQUIRE(lateCalls == 0);
    onFire();
    REQUIRE(lateCalls == 1);
  }

  SECTION("Clear during a fire") {
    Event<void()> onFire;
    int32 calls = 0;
    HEvent first = onFire.connect([&]() {
      ++calls;
      onFire.clear();
    });
    HEvent second = onFire.connect([&]() { ++calls; });

    onFire();
    REQUIRE(calls == 2);
    onFire();
    REQUIRE(calls == 2);
  }

  SECTION("Fire from inside a callback") {
    Event<void(int32)> onDepth;
    int32 calls = 0;
    HEvent handle = onDepth.connect([&](int32 depth) {
      ++calls;
      if (depth < 3) {
        onDepth(depth + 1);
      }
    });
    onDepth(0);
    REQUIRE(calls == 4);
  }

  SECTION("Moving keeps the listeners") {
    int32 total = 0;
    UnorderedMap<int32, Event<void(int32)>> events;
    HEvent handle;
    {
      Event<void(int32)> onValue;
      handle = onValue.connect([&total](int32 value) { total += value; });
      events.emplace(0, std::move(onValue));
    }
    events[0](3);
    REQUIRE(total == 3);

    Event<void(int32)> other;
    other = std::move(events[0]);
    other(2);
    REQUIRE(total == 5);
    REQUIRE(handle.isValid());
  }
}

TEST_CASE("chUtilities - Logger", "[Util]") {
  CH_LOG_DECLARE_STATIC(LoggerTestLog, All);
  REQUIRE(Logger::findCategory("LoggerTestLog") == &LoggerTestLog);

  // Logged before the Logger starts, and added to its buffer when it starts.
  CH_LOG_INFO(LoggerTestLog, "Early");

  Logger::startUp();
  Logger& logger = Logger::instance();
  REQUIRE(logger.getBufferedLogs().back()->message == "Early");

  logger.setConsoleOutput(false);
  logger.setBufferingEnabled(true, 3);

  for (int32 i = 0; i < 5; ++i) {
    CH_LOG_INFO(LoggerTestLog, "Message {0}", i);
  }

  // The ring buffer keeps the last 3 entries, oldest first.
  Vector<SPtr<const LogBufferEntry>> buffered = logger.getBufferedLogs();
  REQUIRE(buffered.size() == 3);
  REQUIRE(buffered[0]->message == "Message 2");
  REQUIRE(buffered[1]->message == "Message 3");
  REQUIRE(buffered[2]->message == "Message 4");

  const LogBufferEntry& entry = *buffered[2];
  REQUIRE(entry.verbosity == LogVerbosity::Info);
  REQUIRE(entry.category == "LoggerTestLog");
  REQUIRE(entry.sourceFile == "chSystemTests.cpp");
  REQUIRE(entry.sourceLine > 0);
  REQUIRE(StringView(entry.timestamp).size() == 23);
  REQUIRE(entry.timestamp[4] == '-');
  REQUIRE(entry.timestamp[10] == ' ');
  REQUIRE(entry.timestamp[19] == '.');

  // Replays the buffer in order, then receives the same shared entry as the buffer.
  Vector<SPtr<const LogBufferEntry>> received;
  HEvent listener = logger.onLogWritten(
      [&received](const SPtr<const LogBufferEntry>& logEntry) {
        received.push_back(logEntry);
      },
      true);
  REQUIRE(received.size() == 3);
  REQUIRE(received[0]->message == "Message 2");

  CH_LOG_WARNING(LoggerTestLog, "Shared");
  REQUIRE(received.size() == 4);
  REQUIRE(received.back() == logger.getBufferedLogs().back());
  Logger::disconnectLogListener(listener);

  // Shrinking the buffer keeps the newest entries.
  logger.setBufferingEnabled(true, 2);
  buffered = logger.getBufferedLogs();
  REQUIRE(buffered.size() == 2);
  REQUIRE(buffered[0]->message == "Message 4");
  REQUIRE(buffered[1]->message == "Shared");

  int32 lateCalls = 0;
  HEvent lateListener = logger.onLogWritten(
      [&lateCalls](const SPtr<const LogBufferEntry>&) { ++lateCalls; });

  Logger::shutDown();

  // After shutDown logging only reaches the console, and a listener can still be released.
  CH_LOG_INFO(LoggerTestLog, "After shut down");
  REQUIRE(lateCalls == 0);
  Logger::disconnectLogListener(lateListener);
  REQUIRE_FALSE(lateListener.isValid());
}

#if USING(CH_PROFILER)
TEST_CASE("chUtilities - Profiler", "[Util]")
{
  IConsoleVariable* updateInterval = ConsoleVariables::find("Profiler.UpdateInterval");
  REQUIRE(updateInterval != nullptr);
  // Every frame publishes, so each frame can be checked alone.
  REQUIRE(updateInterval->setFromString("0", ConsoleVariableSource::Console));

  SECTION("Disabled scopes measure nothing")
  {
    Profiler::setEnabled(false);
    Profiler::beginFrame();
    REQUIRE(Profiler::beginScope("Ignored") == Profiler::INVALID_SCOPE);
    Profiler::endFrame();
  }

  SECTION("Nested scopes add up by name under their parent")
  {
    Profiler::setEnabled(true);
    Profiler::beginFrame();
    {
      CH_PROFILE_SCOPE("Update");
      for (int32 i = 0; i < 3; ++i) {
        CH_PROFILE_SCOPE("Step");
      }
    }
    {
      CH_PROFILE_SCOPE("Render");
      CH_PROFILE_SCOPE("Step");
    }
    Profiler::endFrame();

    const Span<const ProfilerScopeStats> scopes = Profiler::getScopeStats();
    REQUIRE(scopes.size() == 5);
    REQUIRE(scopes[0].name == "Frame");
    REQUIRE(scopes[0].depth == 0);
    REQUIRE(scopes[1].name == "Update");
    REQUIRE(scopes[1].depth == 1);
    REQUIRE(scopes[2].name == "Step");
    REQUIRE(scopes[2].depth == 2);
    REQUIRE(scopes[2].callsPerFrame == 3.0f);
    REQUIRE(scopes[3].name == "Render");
    REQUIRE(scopes[3].depth == 1);
    REQUIRE(scopes[4].name == "Step");
    REQUIRE(scopes[4].callsPerFrame == 1.0f);
    REQUIRE(scopes[0].milliseconds >= scopes[1].milliseconds);
    REQUIRE(Profiler::getAverageFrameMilliseconds() == scopes[0].milliseconds);

    const uint32 newest =
        (Profiler::getFrameHistoryOffset() + Profiler::FRAME_HISTORY_SIZE - 1) %
        Profiler::FRAME_HISTORY_SIZE;
    REQUIRE(Profiler::getFrameHistory()[newest] >= 0.0f);
  }

  SECTION("GPU times add up by name and depth")
  {
    Profiler::setEnabled(true);
    Profiler::beginFrame();
    Profiler::addGpuTime("RenderGraph", 0, 3.0f);
    Profiler::addGpuTime("GBuffer", 1, 1.0f);
    Profiler::addGpuTime("Lighting", 1, 1.5f);
    Profiler::addGpuTime("GBuffer", 1, 0.5f);
    Profiler::endFrame();

    const Span<const ProfilerScopeStats> scopes = Profiler::getGpuScopeStats();
    REQUIRE(scopes.size() == 3);
    REQUIRE(scopes[0].name == "RenderGraph");
    REQUIRE(scopes[0].depth == 0);
    REQUIRE(scopes[1].name == "GBuffer");
    REQUIRE(scopes[1].milliseconds == 1.5f);
    REQUIRE(scopes[1].callsPerFrame == 2.0f);
    REQUIRE(scopes[2].name == "Lighting");
    REQUIRE(Profiler::getAverageGpuFrameMilliseconds() == 3.0f);
  }

  SECTION("Other threads are not measured")
  {
    Profiler::setEnabled(true);
    Profiler::beginFrame();
    uint32 workerScope = 0;
    Thread worker([&workerScope]() { workerScope = Profiler::beginScope("Worker"); });
    worker.join();
    REQUIRE(workerScope == Profiler::INVALID_SCOPE);
    Profiler::endFrame();
  }

  Profiler::setEnabled(false);
  REQUIRE(updateInterval->setFromString("0.5", ConsoleVariableSource::Console));
}
#endif // USING(CH_PROFILER)
