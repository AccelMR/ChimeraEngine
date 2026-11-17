/************************************************************************/
/**
 * @file chGpuAgeRing.h
 * @author AccelMR
 * @date 2026/11/16
 * @brief
 *  A ring of GPU entries that are written once and die by their age.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chIBuffer.h"
#include "chIGraphicsAPI.h"
#include "chMath.h"

namespace chEngineSDK {

/**
 * Exists so systems that spawn many short lived things (floating numbers, particles) pay
 * for each one once: an entry is written into a CPU copy when it is born and goes to the
 * GPU with the others written since the last upload, in one or two copies; a shader then
 * moves it by its age. Entries die in about the order they were made, so the live ones are
 * one part of the ring (drawn in at most two ranges), and a full ring writes over the
 * oldest. The ring keeps its own clock, which only moves with advance.
 *
 * Costs: create sizes everything once, so write, advance and upload never allocate; write
 * is a copy and a few counters; advance walks only the entries that died.
 */
template<typename T>
class GpuAgeRing
{
 public:
  /**
   * Sizes the CPU copy and, when there is a graphics API, a GpuOnly storage buffer of
   * capacity entries. Clears the ring.
   */
  void
  create(uint32 capacity)
  {
    m_entries.assign(capacity, T{});
    m_deaths.assign(capacity, 0.0f);
    m_buffer.reset();
    if (IGraphicsAPI::isStarted() && capacity > 0) {
      m_buffer = IGraphicsAPI::instance().createBuffer(
          {.size = static_cast<uint32>(capacity * sizeof(T)),
           .usage = BufferUsage::StorageBuffer,
           .memoryUsage = MemoryUsage::GpuOnly});
    }
    clear();
  }

  void
  release()
  {
    m_buffer.reset();
    m_entries.clear();
    m_entries.shrink_to_fit();
    m_deaths.clear();
    m_deaths.shrink_to_fit();
    clear();
  }

  /**
   * Forgets every entry; the clock keeps its time.
   */
  void
  clear() noexcept
  {
    m_writeCursor = 0;
    m_liveStart = 0;
    m_liveCount = 0;
    m_pendingStart = 0;
    m_pendingCount = 0;
  }

  /**
   * Moves the clock and drops the entries that died. Returns how many may still live.
   */
  uint32
  advance(float deltaTime) noexcept
  {
    m_time += deltaTime;
    const uint32 capacity = getCapacity();
    while (m_liveCount > 0 && m_deaths[m_liveStart] <= m_time) {
      m_liveStart = (m_liveStart + 1) % capacity;
      --m_liveCount;
    }
    if (m_liveCount == 0) {
      m_pendingCount = 0;
    }
    return m_liveCount;
  }

  /**
   * Writes entry at the cursor, living lifetime seconds from now, and gives its slot. The
   * ring must have been created.
   */
  uint32
  write(const T& entry, float lifetime) noexcept
  {
    const uint32 capacity = getCapacity();
    const uint32 slot = m_writeCursor;
    m_entries[slot] = entry;
    m_deaths[slot] = m_time + lifetime;
    m_writeCursor = (m_writeCursor + 1) % capacity;

    if (m_liveCount == capacity) {
      m_liveStart = (m_liveStart + 1) % capacity;
    }
    else {
      ++m_liveCount;
    }
    if (m_pendingCount == 0) {
      m_pendingStart = slot;
    }
    m_pendingCount = Math::min(m_pendingCount + 1, capacity);
    return slot;
  }

  /**
   * The i-th entry that may still live, from the oldest; changes to it reach the GPU only
   * after markLivePending.
   */
  NODISCARD FORCEINLINE T&
  getLiveEntry(uint32 index) noexcept
  {
    return m_entries[(m_liveStart + index) % getCapacity()];
  }

  /**
   * Every live entry goes up again with the next upload.
   */
  void
  markLivePending() noexcept
  {
    m_pendingStart = m_liveStart;
    m_pendingCount = m_liveCount;
  }

  /**
   * Copies the entries written since the last upload to the GPU.
   */
  void
  upload()
  {
    if (m_pendingCount == 0) {
      return;
    }
    if (m_buffer) {
      const uint32 untilEnd = Math::min(m_pendingCount, getCapacity() - m_pendingStart);
      m_buffer->update(m_entries.data() + m_pendingStart,
                       untilEnd * sizeof(T),
                       static_cast<uint32>(m_pendingStart * sizeof(T)));
      if (untilEnd < m_pendingCount) {
        m_buffer->update(m_entries.data(), (m_pendingCount - untilEnd) * sizeof(T));
      }
    }
    m_pendingCount = 0;
  }

  /**
   * Calls function(first, count) for the one or two ranges of slots that may still live.
   */
  template<typename Function>
  void
  forEachLiveRange(Function&& function) const
  {
    if (m_liveCount == 0) {
      return;
    }
    const uint32 untilEnd = Math::min(m_liveCount, getCapacity() - m_liveStart);
    function(m_liveStart, untilEnd);
    if (untilEnd < m_liveCount) {
      function(0u, m_liveCount - untilEnd);
    }
  }

  NODISCARD FORCEINLINE uint32
  getCapacity() const noexcept { return static_cast<uint32>(m_entries.size()); }

  NODISCARD FORCEINLINE uint32
  getLiveCount() const noexcept { return m_liveCount; }

  NODISCARD FORCEINLINE uint32
  getWriteCursor() const noexcept { return m_writeCursor; }

  /**
   * Seconds on the ring's clock: what entries' birth times are written with.
   */
  NODISCARD FORCEINLINE float
  getTime() const noexcept { return m_time; }

  NODISCARD FORCEINLINE Span<const T>
  getEntries() const noexcept { return m_entries; }

  /**
   * Null without a graphics API.
   */
  NODISCARD FORCEINLINE IBuffer*
  getBuffer() const noexcept { return m_buffer.get(); }

 private:
  Vector<T> m_entries;
  // When each entry stops living.
  Vector<float> m_deaths;
  uint32 m_writeCursor = 0;
  uint32 m_liveStart = 0;
  uint32 m_liveCount = 0;
  // Entries written since the last upload, from m_pendingStart.
  uint32 m_pendingStart = 0;
  uint32 m_pendingCount = 0;
  float m_time = 0.0f;
  SPtr<IBuffer> m_buffer;
};

} // namespace chEngineSDK
