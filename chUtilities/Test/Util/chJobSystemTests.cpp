/************************************************************************/
/**
 * @file chJobSystemTests.cpp
 * @author AccelMR
 * @date 2026/11/21
 * @brief Tests of InplaceFunction and the job system.
 */
/************************************************************************/
#include <thread>

#include "chInplaceFunction.h"
#include "chJobSystem.h"
#include "chSTDThreading.h"

#include "catch.hpp"

using namespace chEngineSDK;

namespace {
// Counts how many copies of it are alive, to see the function destroys what it holds once.
struct LiveCounter
{
  explicit LiveCounter(int32* live) : m_live(live) { ++*m_live; }
  LiveCounter(LiveCounter&& other) noexcept : m_live(other.m_live) { ++*m_live; }
  LiveCounter(const LiveCounter&) = delete;
  ~LiveCounter() { --*m_live; }

  int32* m_live;
};

// Waits until flag is set; jobs that hold a worker while the test fills the queues.
void
waitForFlag(const Atomic<bool>& flag)
{
  while (!flag.load()) {
    std::this_thread::yield();
  }
}
} // namespace

TEST_CASE("chUtilities - Inplace function", "[Util]")
{
  int32 value = 0;
  InplaceFunction<int32(int32), 32> add = [&value](int32 amount) {
    value += amount;
    return value;
  };
  REQUIRE(add);
  REQUIRE(add(3) == 3);

  InplaceFunction<int32(int32), 32> moved = std::move(add);
  REQUIRE_FALSE(add);
  REQUIRE(moved(4) == 7);

  int32 live = 0;
  {
    InplaceFunction<void(), 32> holder = [counter = LiveCounter(&live)]() {};
    REQUIRE(live == 1);
    InplaceFunction<void(), 32> other;
    other = std::move(holder);
    REQUIRE(live == 1);
    other.reset();
    REQUIRE(live == 0);
    REQUIRE_FALSE(other);
  }
  REQUIRE(live == 0);
}

TEST_CASE("chUtilities - Job system", "[Util]")
{
  SECTION("Before start up jobs run at once")
  {
    REQUIRE_FALSE(JobSystem::isStarted());
    int32 ran = 0;
    const JobHandle handle = JobSystem::submit("InlineTest", [&ran] { ++ran; });
    REQUIRE(ran == 1);
    REQUIRE(JobSystem::isDone(handle));
    JobSystem::runOnMainThread("InlineMainTest", [&ran] { ++ran; });
    REQUIRE(ran == 2);
  }

  JobSystem::startUp(3);
  REQUIRE(JobSystem::isStarted());
  REQUIRE(JobSystem::getWorkerCount() == 3);

  SECTION("Every job runs once")
  {
    Atomic<int32> counter{0};
    Vector<JobHandle> handles;
    for (int32 i = 0; i < 2000; ++i) {
      handles.push_back(JobSystem::submit("CountTest", [&counter] { counter.fetch_add(1); }));
    }
    for (const JobHandle& handle : handles) {
      JobSystem::wait(handle);
      REQUIRE(JobSystem::isDone(handle));
    }
    REQUIRE(counter.load() == 2000);
  }

  SECTION("A job may wait for jobs it starts")
  {
    Atomic<int32> counter{0};
    Vector<JobHandle> outer;
    for (int32 i = 0; i < 16; ++i) {
      outer.push_back(JobSystem::submit("OuterTest", [&counter] {
        JobHandle inner[8];
        for (JobHandle& handle : inner) {
          handle = JobSystem::submit("InnerTest", [&counter] { counter.fetch_add(1); });
        }
        for (const JobHandle& handle : inner) {
          JobSystem::wait(handle);
        }
        counter.fetch_add(100);
      }));
    }
    for (const JobHandle& handle : outer) {
      JobSystem::wait(handle);
    }
    REQUIRE(counter.load() == 16 * 108);
  }

  SECTION("parallelFor covers every index once")
  {
    Vector<Atomic<int32>> hits(10007);
    JobSystem::parallelFor("ParallelTest", hits.size(), 64, [&hits](SIZE_T begin, SIZE_T end) {
      for (SIZE_T i = begin; i < end; ++i) {
        hits[i].fetch_add(1);
      }
    });
    bool bOnce = true;
    for (const Atomic<int32>& hit : hits) {
      bOnce = bOnce && hit.load() == 1;
    }
    REQUIRE(bOnce);

    int32 small = 0;
    JobSystem::parallelFor("SmallTest", 10, 64, [&small](SIZE_T begin, SIZE_T end) {
      small += static_cast<int32>(end - begin);
    });
    REQUIRE(small == 10);
  }

  SECTION("Main-thread jobs wait for the pump and respect its budget")
  {
    int32 ran = 0;
    const JobHandle handle = JobSystem::submit("QueueMainTest", [&ran] {
      for (int32 i = 0; i < 3; ++i) {
        JobSystem::runOnMainThread("MainTest", [&ran] { ++ran; });
      }
    });
    JobSystem::wait(handle);
    REQUIRE(ran == 0);
    // A budget of zero still runs one job per pump.
    JobSystem::pumpMainThread(0.0f);
    REQUIRE(ran == 1);
    JobSystem::pumpMainThread();
    REQUIRE(ran == 3);
    REQUIRE(JobSystem::isMainThread());
  }

  SECTION("When every slot is taken a job runs on the caller")
  {
    Atomic<bool> release{false};
    Vector<JobHandle> blockers;
    for (uint32 i = 0; i < JobSystem::MAX_JOBS; ++i) {
      blockers.push_back(
          JobSystem::submit("BlockTest", [&release] { waitForFlag(release); },
                            JobPriority::Background));
    }
    bool bRanInline = false;
    const JobHandle extra = JobSystem::submit("ExtraTest", [&bRanInline] { bRanInline = true; });
    REQUIRE(bRanInline);
    REQUIRE_FALSE(extra.isValid());
    release.store(true);
    for (const JobHandle& handle : blockers) {
      JobSystem::wait(handle);
    }
  }

#if USING(CH_PROFILER)
  SECTION("Statistics count jobs by name and thread")
  {
    Vector<JobHandle> handles;
    for (int32 i = 0; i < 50; ++i) {
      handles.push_back(JobSystem::submit("StatsTest", [] {}));
    }
    for (const JobHandle& handle : handles) {
      JobSystem::wait(handle);
    }
    Vector<JobNameStats> names;
    JobSystem::getNameStats(names);
    uint64 statsJobs = 0;
    for (const JobNameStats& stats : names) {
      if (StringView(stats.name) == "StatsTest") {
        statsJobs = stats.jobCount;
      }
    }
    REQUIRE(statsJobs == 50);
    Vector<JobThreadStats> threads;
    JobSystem::getThreadStats(threads);
    REQUIRE(threads.size() == 4);
  }
#endif // USING(CH_PROFILER)

  JobSystem::shutDown();
  REQUIRE_FALSE(JobSystem::isStarted());
}

TEST_CASE("chUtilities - Job priorities and shut down", "[Util]")
{
  JobSystem::startUp(1);
  Atomic<bool> release{false};
  const JobHandle blocker = JobSystem::submit("HoldTest", [&release] { waitForFlag(release); });
  // Give the one worker time to take the blocker before the others are queued.
  while (JobSystem::isDone(blocker)) {
    std::this_thread::yield();
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  Mutex mutex;
  Vector<int32> order;
  auto record = [&mutex, &order](int32 value) {
    LockGuard<Mutex> lock(mutex);
    order.push_back(value);
  };
  JobSystem::submit("BackgroundTest", [&record] { record(2); }, JobPriority::Background);
  JobSystem::submit("NormalTest", [&record] { record(1); }, JobPriority::Normal);
  JobSystem::submit("HighTest", [&record] { record(0); }, JobPriority::High);
  release.store(true);

  // Shutting down runs what is still queued, in priority order.
  JobSystem::shutDown();
  REQUIRE(order == Vector<int32>{0, 1, 2});
}
