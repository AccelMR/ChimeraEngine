/************************************************************************/
/**
 * @file chJobSystem.cpp
 * @author AccelMR
 * @date 2026/11/21
 * @brief
 *  Worker threads that run small pieces of work for the whole engine.
 */
/************************************************************************/
#include "chJobSystem.h"

#include <chrono>

#include "chConsoleVariable.h"
#include "chLogger.h"
#include "chMath.h"
#include "chSTDThreading.h"

namespace chEngineSDK {
CH_LOG_DECLARE_STATIC(JobSystemLog, All);

namespace {
ConsoleVariable<int32> g_cvarWorkerThreads(
    "Jobs.WorkerThreads",
    0,
    "Worker threads of the job system; 0 is one per core but the one the main thread uses.",
    "WorkerThreads");

// More pieces than this in one parallelFor bring nothing but overhead.
constexpr SIZE_T kMaxParallelPieces = 64;

using Clock = std::chrono::steady_clock;

struct JobSlot
{
  JobFunction function;
  const ANSICHAR* name = nullptr;
  // Goes up when the job finishes, which is what isDone looks at.
  Atomic<uint32> generation{1};
};

// A fixed ring of slot indices; MAX_JOBS slots can never overfill it.
struct SlotQueue
{
  Array<uint32, JobSystem::MAX_JOBS> items{};
  uint32 head = 0;
  uint32 count = 0;

  void
  push(uint32 slot) noexcept
  {
    items[(head + count) % JobSystem::MAX_JOBS] = slot;
    ++count;
  }

  uint32
  pop() noexcept
  {
    const uint32 slot = items[head];
    head = (head + 1) % JobSystem::MAX_JOBS;
    --count;
    return slot;
  }
};

struct MainThreadJob
{
  const ANSICHAR* name = nullptr;
  JobFunction function;
};

#if USING(CH_PROFILER)
struct NameCounter
{
  const ANSICHAR* name = nullptr;
  uint64 jobCount = 0;
  uint64 busyNanoseconds = 0;
};

// Written by its own thread under its own (almost never contended) lock.
struct alignas(64) ThreadCounters
{
  Mutex mutex;
  JobThreadStats totals;
  Array<NameCounter, JobSystem::MAX_STAT_NAMES> names{};
  uint32 nameCount = 0;
  NameCounter other{.name = "Other"};
};
#endif // USING(CH_PROFILER)

struct JobState
{
  Array<JobSlot, JobSystem::MAX_JOBS> slots;
  Vector<uint32> freeSlots;
  Array<SlotQueue, static_cast<SIZE_T>(JobPriority::COUNT)> queues;
  Mutex mutex;
  ConditionVariable workAvailable;
  ConditionVariable jobFinished;
  Vector<Thread> workers;
  bool bRunning = false;
  Atomic<bool> bStarted{false};
  Thread::id mainThread;
  Atomic<bool> bWarnedFull{false};

  Mutex mainMutex;
  Vector<MainThreadJob> mainQueue;
  Vector<MainThreadJob> mainRunning;

#if USING(CH_PROFILER)
  // One per worker, then one for the main thread.
  Vector<UniquePtr<ThreadCounters>> counters;
#endif // USING(CH_PROFILER)
};

// Built on first use: the slots are big and only programs that use jobs need them.
JobState&
getState()
{
  static JobState state;
  return state;
}

// The counters this thread writes into: its worker index, the main thread's after the
// workers, or none (-1) for threads the job system did not start.
thread_local int32 t_threadIndex = -1;

bool
hasWork(const JobState& state) noexcept
{
  for (const SlotQueue& queue : state.queues) {
    if (queue.count > 0) {
      return true;
    }
  }
  return false;
}

// Call with the lock held and work queued.
uint32
popHighest(JobState& state) noexcept
{
  for (SlotQueue& queue : state.queues) {
    if (queue.count > 0) {
      return queue.pop();
    }
  }
  return JobHandle::INVALID_INDEX;
}

void
recordJob(const ANSICHAR* name, Clock::time_point start)
{
#if USING(CH_PROFILER)
  JobState& state = getState();
  if (t_threadIndex < 0 || static_cast<SIZE_T>(t_threadIndex) >= state.counters.size()) {
    return;
  }
  const uint64 nanoseconds = static_cast<uint64>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count());
  ThreadCounters& counters = *state.counters[static_cast<SIZE_T>(t_threadIndex)];
  LockGuard<Mutex> lock(counters.mutex);
  ++counters.totals.jobCount;
  counters.totals.busyNanoseconds += nanoseconds;
  // Names are literals, so the pointer tells them apart.
  NameCounter* counter = &counters.other;
  for (uint32 i = 0; i < counters.nameCount; ++i) {
    if (counters.names[i].name == name) {
      counter = &counters.names[i];
      break;
    }
  }
  if (counter == &counters.other && counters.nameCount < JobSystem::MAX_STAT_NAMES) {
    counter = &counters.names[counters.nameCount++];
    counter->name = name;
  }
  ++counter->jobCount;
  counter->busyNanoseconds += nanoseconds;
#else
  CH_PARAMETER_UNUSED(name);
  CH_PARAMETER_UNUSED(start);
#endif // USING(CH_PROFILER)
}

void
runSlot(JobState& state, uint32 index)
{
  JobSlot& slot = state.slots[index];
  const Clock::time_point start = Clock::now();
  slot.function();
  slot.function.reset();
  recordJob(slot.name, start);
  {
    LockGuard<Mutex> lock(state.mutex);
    slot.generation.fetch_add(1, std::memory_order_release);
    state.freeSlots.push_back(index);
  }
  state.jobFinished.notify_all();
}

void
workerLoop(JobState& state, int32 threadIndex)
{
  t_threadIndex = threadIndex;
  while (true) {
    uint32 index = JobHandle::INVALID_INDEX;
    {
      UniqueLock<Mutex> lock(state.mutex);
      state.workAvailable.wait(lock, [&state] { return hasWork(state) || !state.bRunning; });
      // Stopping drains the queues first.
      if (!hasWork(state)) {
        return;
      }
      index = popHighest(state);
    }
    runSlot(state, index);
  }
}

void
runInline(const ANSICHAR* name, JobFunction& function)
{
  const Clock::time_point start = Clock::now();
  function();
  recordJob(name, start);
}
} // namespace

/*
 */
void
JobSystem::startUp(uint32 workerCount)
{
  JobState& state = getState();
  if (state.bStarted.load()) {
    return;
  }
  if (workerCount == 0) {
    const int32 configured = g_cvarWorkerThreads.get();
    const uint32 cores = Math::max<uint32>(Thread::hardware_concurrency(), 2);
    workerCount = configured > 0 ? static_cast<uint32>(configured) : cores - 1;
  }

  state.freeSlots.clear();
  state.freeSlots.reserve(MAX_JOBS);
  for (uint32 i = MAX_JOBS; i > 0; --i) {
    state.freeSlots.push_back(i - 1);
  }
  state.mainThread = std::this_thread::get_id();
  t_threadIndex = static_cast<int32>(workerCount);
#if USING(CH_PROFILER)
  state.counters.clear();
  for (uint32 i = 0; i <= workerCount; ++i) {
    state.counters.push_back(chMakeUnique<ThreadCounters>());
  }
#endif // USING(CH_PROFILER)

  state.bRunning = true;
  state.workers.reserve(workerCount);
  for (uint32 i = 0; i < workerCount; ++i) {
    state.workers.emplace_back(&workerLoop, std::ref(state), static_cast<int32>(i));
  }
  state.bStarted.store(true);
  CH_LOG_INFO(JobSystemLog, "Job system started with {0} worker threads.", workerCount);
}

/*
 */
void
JobSystem::shutDown()
{
  JobState& state = getState();
  if (!state.bStarted.load()) {
    return;
  }
  {
    LockGuard<Mutex> lock(state.mutex);
    state.bRunning = false;
  }
  state.workAvailable.notify_all();
  for (Thread& worker : state.workers) {
    worker.join();
  }
  state.workers.clear();
  state.bStarted.store(false);
  {
    LockGuard<Mutex> lock(state.mainMutex);
    if (!state.mainQueue.empty()) {
      CH_LOG_WARNING(JobSystemLog, "{0} main-thread jobs were dropped at shut down.",
                     state.mainQueue.size());
    }
    state.mainQueue.clear();
  }
}

/*
 */
bool
JobSystem::isStarted() noexcept
{
  return getState().bStarted.load(std::memory_order_acquire);
}

/*
 */
uint32
JobSystem::getWorkerCount() noexcept
{
  return isStarted() ? static_cast<uint32>(getState().workers.size()) : 0;
}

/*
 */
JobHandle
JobSystem::submit(const ANSICHAR* name, JobFunction function, JobPriority priority)
{
  JobState& state = getState();
  if (!state.bStarted.load(std::memory_order_acquire)) {
    runInline(name, function);
    return {};
  }

  JobHandle handle;
  {
    LockGuard<Mutex> lock(state.mutex);
    if (!state.freeSlots.empty()) {
      handle.index = state.freeSlots.back();
      state.freeSlots.pop_back();
      JobSlot& slot = state.slots[handle.index];
      slot.function = std::move(function);
      slot.name = name;
      handle.generation = slot.generation.load(std::memory_order_relaxed);
      state.queues[static_cast<SIZE_T>(priority)].push(handle.index);
    }
  }
  if (!handle.isValid()) {
    if (!state.bWarnedFull.exchange(true)) {
      CH_LOG_WARNING(JobSystemLog,
                     "Every one of the {0} job slots is taken; jobs run on the thread that "
                     "submits them until some finish.",
                     MAX_JOBS);
    }
    runInline(name, function);
    return {};
  }
  state.workAvailable.notify_one();
  return handle;
}

/*
 */
bool
JobSystem::isDone(JobHandle handle) noexcept
{
  return !handle.isValid() ||
         getState().slots[handle.index].generation.load(std::memory_order_acquire) !=
             handle.generation;
}

/*
 */
void
JobSystem::wait(JobHandle handle)
{
  JobState& state = getState();
  while (!isDone(handle)) {
    uint32 index = JobHandle::INVALID_INDEX;
    {
      UniqueLock<Mutex> lock(state.mutex);
      if (hasWork(state)) {
        index = popHighest(state);
      }
      else {
        // The job runs on another thread; its end wakes this one.
        state.jobFinished.wait_for(lock, std::chrono::milliseconds(1),
                                   [handle] { return isDone(handle); });
        continue;
      }
    }
    runSlot(state, index);
  }
}

/*
 */
void
JobSystem::parallelForRaw(const ANSICHAR* name,
                          SIZE_T count,
                          SIZE_T minBatch,
                          RangeFunction function,
                          void* context)
{
  if (count == 0) {
    return;
  }
  minBatch = Math::max<SIZE_T>(minBatch, 1);
  const SIZE_T threads = static_cast<SIZE_T>(getWorkerCount()) + 1;
  const SIZE_T pieces = Math::min(Math::min(threads, kMaxParallelPieces),
                                  Math::max<SIZE_T>(count / minBatch, 1));
  if (pieces <= 1) {
    function(context, 0, count);
    return;
  }

  const SIZE_T pieceSize = (count + pieces - 1) / pieces;
  Array<JobHandle, kMaxParallelPieces> handles{};
  SIZE_T handleCount = 0;
  for (SIZE_T begin = pieceSize; begin < count; begin += pieceSize) {
    const SIZE_T end = Math::min(begin + pieceSize, count);
    handles[handleCount++] = submit(
        name, [function, context, begin, end] { function(context, begin, end); },
        JobPriority::High);
  }
  function(context, 0, Math::min(pieceSize, count));
  for (SIZE_T i = 0; i < handleCount; ++i) {
    wait(handles[i]);
  }
}

/*
 */
void
JobSystem::runOnMainThread(const ANSICHAR* name, JobFunction function)
{
  JobState& state = getState();
  if (!state.bStarted.load(std::memory_order_acquire)) {
    runInline(name, function);
    return;
  }
  LockGuard<Mutex> lock(state.mainMutex);
  state.mainQueue.push_back({.name = name, .function = std::move(function)});
}

/*
 */
void
JobSystem::pumpMainThread(float budgetMilliseconds)
{
  JobState& state = getState();
  CH_ASSERT(!state.bStarted.load() || isMainThread());
  {
    LockGuard<Mutex> lock(state.mainMutex);
    if (state.mainQueue.empty()) {
      return;
    }
    // Swapped out, so jobs queued while these run wait for the next pump.
    std::swap(state.mainQueue, state.mainRunning);
  }

  const Clock::time_point start = Clock::now();
  SIZE_T ran = 0;
  for (; ran < state.mainRunning.size(); ++ran) {
    if (ran > 0) {
      const float elapsed =
          std::chrono::duration<float, std::milli>(Clock::now() - start).count();
      if (elapsed >= budgetMilliseconds) {
        break;
      }
    }
    MainThreadJob& job = state.mainRunning[ran];
    runInline(job.name, job.function);
  }

  {
    LockGuard<Mutex> lock(state.mainMutex);
    // What did not fit goes back in front of anything queued meanwhile, in order.
    if (ran < state.mainRunning.size()) {
      state.mainQueue.insert(state.mainQueue.begin(),
                             std::make_move_iterator(state.mainRunning.begin() +
                                                     static_cast<int64>(ran)),
                             std::make_move_iterator(state.mainRunning.end()));
    }
  }
  state.mainRunning.clear();
}

/*
 */
bool
JobSystem::isMainThread() noexcept
{
  return std::this_thread::get_id() == getState().mainThread;
}

/*
 */
void
JobSystem::getThreadStats(Vector<JobThreadStats>& outStats)
{
  outStats.clear();
#if USING(CH_PROFILER)
  for (const UniquePtr<ThreadCounters>& counters : getState().counters) {
    LockGuard<Mutex> lock(counters->mutex);
    outStats.push_back(counters->totals);
  }
#endif // USING(CH_PROFILER)
}

/*
 */
void
JobSystem::getNameStats(Vector<JobNameStats>& outStats)
{
  outStats.clear();
#if USING(CH_PROFILER)
  auto add = [&outStats](const NameCounter& counter) {
    if (counter.jobCount == 0) {
      return;
    }
    for (JobNameStats& stats : outStats) {
      if (stats.name == counter.name) {
        stats.jobCount += counter.jobCount;
        stats.busyNanoseconds += counter.busyNanoseconds;
        return;
      }
    }
    outStats.push_back({.name = counter.name,
                        .jobCount = counter.jobCount,
                        .busyNanoseconds = counter.busyNanoseconds});
  };
  for (const UniquePtr<ThreadCounters>& counters : getState().counters) {
    LockGuard<Mutex> lock(counters->mutex);
    for (uint32 i = 0; i < counters->nameCount; ++i) {
      add(counters->names[i]);
    }
    add(counters->other);
  }
#endif // USING(CH_PROFILER)
}

} // namespace chEngineSDK
