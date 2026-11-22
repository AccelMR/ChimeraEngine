/************************************************************************/
/**
 * @file chJobSystem.h
 * @author AccelMR
 * @date 2026/11/21
 * @brief
 *  Worker threads that run small pieces of work for the whole engine.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesUtilities.h"
#include "chInplaceFunction.h"

namespace chEngineSDK {

enum class JobPriority : uint8
{
  // Work the current frame waits for (parallelFor).
  High = 0,
  Normal = 1,
  // Long work nobody waits for this frame: loading, packaging, importing.
  Background = 2,

  COUNT
};

using JobFunction = InplaceFunction<void(), 64>;

/**
 * Names one submitted job. It stays valid after the job finishes: isDone is then true.
 */
struct JobHandle
{
  uint32 index = INVALID_INDEX;
  uint32 generation = 0;

  NODISCARD FORCEINLINE bool
  isValid() const noexcept
  {
    return index != INVALID_INDEX;
  }

  static constexpr uint32 INVALID_INDEX = ~0u;
};

/**
 * What one thread did since the job system started: the workers first, then the main
 * thread (jobs it ran while waiting, and its main-thread queue).
 */
struct JobThreadStats
{
  uint64 jobCount = 0;
  uint64 busyNanoseconds = 0;
};

/**
 * Time spent in the jobs of one name, over every thread, since the job system started.
 */
struct JobNameStats
{
  const ANSICHAR* name = nullptr;
  uint64 jobCount = 0;
  uint64 busyNanoseconds = 0;
};

/**
 * Exists so engine systems share one set of worker threads instead of each starting its
 * own. A job is a small callable kept inside a fixed slot (submitting never allocates) and
 * named by a string literal for the profiler. Workers take the highest priority first. A
 * thread that waits for a job runs other jobs meanwhile, so waiting from the main thread or
 * from inside a job never leaves a core idle or deadlocks. runOnMainThread queues work that
 * only the main thread may do (GPU uploads, the scene), run by pumpMainThread once a frame.
 *
 * Before startUp (tools, tests) and after shutDown every job runs at once on the calling
 * thread, so code that uses it needs no checks.
 */
class CH_UTILITY_EXPORT JobSystem
{
 public:
  /**
   * workerCount 0 takes Jobs.WorkerThreads, whose 0 means one per core but the main one.
   * Call on the main thread, which becomes the one pumpMainThread serves.
   */
  static void
  startUp(uint32 workerCount = 0);

  /**
   * Runs what is still queued, then stops the workers. Main-thread work left is dropped.
   */
  static void
  shutDown();

  NODISCARD static bool
  isStarted() noexcept;

  NODISCARD static uint32
  getWorkerCount() noexcept;

  /**
   * name must be a string literal. When every slot is taken the job runs at once on the
   * calling thread (logged once) instead of waiting for one.
   */
  static JobHandle
  submit(const ANSICHAR* name, JobFunction function, JobPriority priority = JobPriority::Normal);

  NODISCARD static bool
  isDone(JobHandle handle) noexcept;

  /**
   * Returns once the job finished, running other jobs meanwhile.
   */
  static void
  wait(JobHandle handle);

  /**
   * Calls function(begin, end) over [0, count) in pieces of at least minBatch, on the
   * workers and the calling thread, and returns when every piece ran.
   */
  template<typename Function>
  static void
  parallelFor(const ANSICHAR* name, SIZE_T count, SIZE_T minBatch, const Function& function)
  {
    parallelForRaw(name, count, minBatch, &callRange<Function>,
                   const_cast<void*>(static_cast<const void*>(&function)));
  }

  /**
   * Queues function for the next pumpMainThread. Any thread may call it.
   */
  static void
  runOnMainThread(const ANSICHAR* name, JobFunction function);

  /**
   * Runs the main-thread queue, stopping once budgetMilliseconds passed (what is left waits
   * for the next call; at least one job always runs). The application calls it every frame.
   */
  static void
  pumpMainThread(float budgetMilliseconds = 1.0e9f);

  NODISCARD static bool
  isMainThread() noexcept;

  /**
   * Fills the counters of every worker and of the main thread (last). Totals since startUp;
   * empty without CH_PROFILER.
   */
  static void
  getThreadStats(Vector<JobThreadStats>& outStats);

  /**
   * Fills the counters of every job name seen. Empty without CH_PROFILER.
   */
  static void
  getNameStats(Vector<JobNameStats>& outStats);

  // Jobs waiting or running at once; above this submit runs jobs inline.
  static constexpr uint32 MAX_JOBS = 4096;
  // Names the per-thread statistics keep apart; later ones are counted as "Other".
  static constexpr uint32 MAX_STAT_NAMES = 64;

 private:
  using RangeFunction = void (*)(void* context, SIZE_T begin, SIZE_T end);

  template<typename Function>
  static void
  callRange(void* context, SIZE_T begin, SIZE_T end)
  {
    (*static_cast<const Function*>(context))(begin, end);
  }

  static void
  parallelForRaw(const ANSICHAR* name,
                 SIZE_T count,
                 SIZE_T minBatch,
                 RangeFunction function,
                 void* context);
};

} // namespace chEngineSDK
