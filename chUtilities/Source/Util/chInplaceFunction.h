/************************************************************************/
/**
 * @file chInplaceFunction.h
 * @author AccelMR
 * @date 2026/11/21
 * @brief
 *  A callable kept in a fixed buffer, never on the heap.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesUtilities.h"

#include <memory>
#include <new>
#include <type_traits>

namespace chEngineSDK {

template<typename Signature, SIZE_T Capacity>
class InplaceFunction;

/**
 * Exists so code that stores many callables (the job system's slots) never allocates: the
 * callable is moved into a buffer of Capacity bytes inside the object, and one that does not
 * fit stops the build instead of going to the heap. Capture pointers or indices to stay
 * small. Move only; calling an empty one is a bug (asserted).
 */
template<typename Result, typename... Args, SIZE_T Capacity>
class InplaceFunction<Result(Args...), Capacity>
{
 public:
  InplaceFunction() noexcept = default;

  template<typename Callable,
           typename = std::enable_if_t<
               !std::is_same_v<std::decay_t<Callable>, InplaceFunction> &&
               std::is_invocable_r_v<Result, std::decay_t<Callable>&, Args...>>>
  InplaceFunction(Callable&& callable)
  {
    using Stored = std::decay_t<Callable>;
    static_assert(sizeof(Stored) <= Capacity,
                  "The callable does not fit; capture pointers or indices instead.");
    static_assert(alignof(Stored) <= alignof(std::max_align_t),
                  "The callable needs more alignment than the buffer has.");
    static_assert(std::is_nothrow_move_constructible_v<Stored>,
                  "The callable must move without throwing.");
    new (m_storage) Stored(std::forward<Callable>(callable));
    m_operations = getOperations<Stored>();
  }

  InplaceFunction(InplaceFunction&& other) noexcept
  {
    moveFrom(other);
  }

  InplaceFunction&
  operator=(InplaceFunction&& other) noexcept
  {
    if (this != &other) {
      reset();
      moveFrom(other);
    }
    return *this;
  }

  InplaceFunction(const InplaceFunction&) = delete;
  InplaceFunction&
  operator=(const InplaceFunction&) = delete;

  ~InplaceFunction()
  {
    reset();
  }

  Result
  operator()(Args... args)
  {
    CH_ASSERT(m_operations && "Calling an empty InplaceFunction");
    return m_operations->invoke(m_storage, std::forward<Args>(args)...);
  }

  NODISCARD FORCEINLINE explicit
  operator bool() const noexcept
  {
    return m_operations != nullptr;
  }

  void
  reset() noexcept
  {
    if (m_operations) {
      m_operations->destroy(m_storage);
      m_operations = nullptr;
    }
  }

 private:
  struct Operations
  {
    Result (*invoke)(void* storage, Args&&... args);
    void (*moveTo)(void* source, void* destination) noexcept;
    void (*destroy)(void* storage) noexcept;
  };

  template<typename Stored>
  static Result
  invokeStored(void* storage, Args&&... args)
  {
    return (*static_cast<Stored*>(storage))(std::forward<Args>(args)...);
  }

  template<typename Stored>
  static void
  moveStored(void* source, void* destination) noexcept
  {
    new (destination) Stored(std::move(*static_cast<Stored*>(source)));
    std::destroy_at(static_cast<Stored*>(source));
  }

  template<typename Stored>
  static void
  destroyStored(void* storage) noexcept
  {
    std::destroy_at(static_cast<Stored*>(storage));
  }

  // One table per stored type, shared by every function that holds one.
  template<typename Stored>
  NODISCARD static const Operations*
  getOperations() noexcept
  {
    static constexpr Operations operations{&invokeStored<Stored>, &moveStored<Stored>,
                                           &destroyStored<Stored>};
    return &operations;
  }

  void
  moveFrom(InplaceFunction& other) noexcept
  {
    if (other.m_operations) {
      other.m_operations->moveTo(other.m_storage, m_storage);
      m_operations = other.m_operations;
      other.m_operations = nullptr;
    }
  }

  alignas(std::max_align_t) uint8 m_storage[Capacity];
  const Operations* m_operations = nullptr;
};

} // namespace chEngineSDK
