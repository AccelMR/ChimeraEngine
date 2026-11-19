/************************************************************************/
/**
 * @file chUtilTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of type sizes, Module, Algorithm, hashes, the linear allocator and Flags.
 */
/************************************************************************/
#include "chAlgorithm.h"
#include "chFlags.h"
#include "chDynamicLibManager.h"
#include "chEventSystem.h"
#include "chHash.h"
#include "chLinearAllocator.h"
#include "chModule.h"

#include "catch.hpp"

using namespace chEngineSDK;

TEST_CASE("chUtilities - BasicTypeSize", "[Util]") {
  REQUIRE(sizeof(unchar) == 1);
  REQUIRE(sizeof(uint8) == 1);
  REQUIRE(sizeof(uint16) == 2);
  REQUIRE(sizeof(uint32) == 4);
  REQUIRE(sizeof(uint64) == 8);

  REQUIRE(sizeof(int8) == 1);
  REQUIRE(sizeof(int16) == 2);
  REQUIRE(sizeof(int32) == 4);
  REQUIRE(sizeof(int64) == 8);

  REQUIRE(sizeof(ANSICHAR) == 1);
  REQUIRE(sizeof(float) == 4);
  REQUIRE(sizeof(double) == 8);

  REQUIRE(sizeof(ANSICHAR) == 1);
  REQUIRE(sizeof(WCHAR16) == 2);
  REQUIRE(sizeof(WCHAR32) == 4);
  REQUIRE(sizeof(UNICHAR) == 2);
  REQUIRE(sizeof(unchar) == 1);

  REQUIRE(sizeof(TYPE_OF_NULL) == 4);
  REQUIRE(sizeof(SIZE_T) == 8);
}

TEST_CASE("chUtilities - Utilities", "[Util]") {
  class Submodule : public Module<Submodule>
  {
   public:
    int32 TestNumber = 11552;
  };
  REQUIRE_THROWS_AS(Submodule::instance(), InternalErrorException);
  REQUIRE_THROWS_AS(Submodule::instancePtr(), InternalErrorException);

  Submodule::startUp<Submodule>();
  REQUIRE_NOTHROW(Submodule::instance());
  REQUIRE_NOTHROW(Submodule::instancePtr());

  REQUIRE(Submodule::instance().TestNumber == 11552);

  REQUIRE_THROWS_AS(Submodule::startUp<Submodule>(), InternalErrorException);

  Submodule::shutDown();
  REQUIRE_THROWS_AS(Submodule::instance(), InternalErrorException);
  REQUIRE_THROWS_AS(Submodule::instancePtr(), InternalErrorException);

  // DynamicLibraryManager::startUp();
  // WeakPtr<DynamicLibrary> TestDll =
  // DynamicLibraryManager::instance().loadDynLibrary("DllTest"); SPtr<DynamicLibrary> RealPtr
  // = TestDll.lock(); REQUIRE(RealPtr);

  // using SymbolFromDll = void(*)();
  // SymbolFromDll func = reinterpret_cast<SymbolFromDll>(RealPtr->getSymbol("testFunction"));

  // REQUIRE(func);
  // func();

  Event<int32(int32, float)> Onsomething;
  HEvent listener1 = Onsomething.connect([](int32 a, float b) -> int32 {
    REQUIRE(a == 10);
    REQUIRE(b == 125.55f);
    return 1;
  });

  struct Test {
    Test(int32 a, float b) : A(a), B(b) {}
    int32 A;
    float B;

    int32
    foo(int32 a, float b) {
      REQUIRE(a == 10);
      REQUIRE(b == 125.55f);
      return 0;
    }
  };

  auto* TestClass = new Test(123, 35445.64565f);

  HEvent listener2 = Onsomething.connect([&TestClass](int32 a, float b) -> int32 {
    REQUIRE(TestClass->A == 123);
    REQUIRE(TestClass->B == 35445.64565f);
    TestClass->foo(a, b);
    return 1;
  });

  HEvent listener3 = Onsomething.connect(
      std::bind(&Test::foo, TestClass, std::placeholders::_1, std::placeholders::_2));

  Onsomething(10, 125.55f);
}

TEST_CASE("chUtilities - Algorithm", "[Util]") {
  Vector<int32> values = {5, 1, 4, 1, 3};
  REQUIRE(Algorithm::contains(values, 4));
  REQUIRE_FALSE(Algorithm::contains(values, 7));

  REQUIRE(Algorithm::removeAll(values, 1) == 2);
  REQUIRE(values == Vector<int32>{5, 4, 3});
  REQUIRE(Algorithm::removeFirst(values, 4));
  REQUIRE_FALSE(Algorithm::removeFirst(values, 4));
  REQUIRE(values == Vector<int32>{5, 3});

  values = {3, 1, 2};
  Algorithm::sort(values);
  REQUIRE(values == Vector<int32>{1, 2, 3});
  Algorithm::sort(values, [](int32 a, int32 b) { return a > b; });
  REQUIRE(values == Vector<int32>{3, 2, 1});

  const Vector<uint64> sorted = {2, 4, 4, 8};
  REQUIRE(Algorithm::lowerBound(sorted, uint64{0}) == 0);
  REQUIRE(Algorithm::lowerBound(sorted, uint64{4}) == 1);
  REQUIRE(Algorithm::lowerBound(sorted, uint64{5}) == 3);
  REQUIRE(Algorithm::lowerBound(sorted, uint64{9}) == 4);

  Vector<int32> ring = {4, 5, 1, 2, 3};
  Algorithm::rotateToFront(ring, 2);
  REQUIRE(ring == Vector<int32>{1, 2, 3, 4, 5});
  Algorithm::rotateToFront(ring, ring.size());
  REQUIRE(ring == Vector<int32>{1, 2, 3, 4, 5});
}

// #endif //CH_PLATFORM_WINDOWS

// int main(int argc, ANSICHAR** argv) {
//   (void)argc;
//   (void)argv;
//   return 0;
// }

TEST_CASE("chUtilities - HashUtils", "[Util]")
{
  // Reference values of 64-bit FNV-1a.
  REQUIRE(HashUtils::hashBytes(nullptr, 0) == HashUtils::FNV_OFFSET_BASIS);
  const uint8 letterA = 'a';
  REQUIRE(HashUtils::hashBytes(&letterA, 1) == 0xAF63DC4C8601EC8Cull);

  const uint64 seed = HashUtils::FNV_OFFSET_BASIS;
  REQUIRE(HashUtils::combine(seed, 7u) == HashUtils::combine(seed, 7u));
  REQUIRE(HashUtils::combine(seed, 7u) != HashUtils::combine(seed, 8u));

  // The order of the fields matters.
  REQUIRE(HashUtils::combine(HashUtils::combine(seed, 1u), 2u) !=
          HashUtils::combine(HashUtils::combine(seed, 2u), 1u));

  // 0.0 and -0.0 compare equal, so they hash the same.
  REQUIRE(HashUtils::combine(seed, 0.0f) == HashUtils::combine(seed, -0.0f));
  REQUIRE(HashUtils::combine(seed, 1.0f) != HashUtils::combine(seed, 2.0f));

  enum class TestEnum : uint8 { First, Second };
  REQUIRE(HashUtils::combine(seed, TestEnum::First) !=
          HashUtils::combine(seed, TestEnum::Second));

  int32 first = 0;
  int32 second = 0;
  REQUIRE(HashUtils::combine(seed, &first) != HashUtils::combine(seed, &second));
}

TEST_CASE("chUtilities - LinearAllocator", "[Util]")
{
  LinearAllocator allocator(256);

  SECTION("Alignment and order")
  {
    void* first = allocator.allocate(3, 1);
    void* aligned = allocator.allocate(8, 64);
    REQUIRE(reinterpret_cast<SIZE_T>(aligned) % 64 == 0);
    REQUIRE(aligned > first);
    REQUIRE(allocator.getBlockCount() == 1);
  }

  SECTION("Big requests get their own block and blocks are kept by reset")
  {
    REQUIRE(allocator.allocate(100) != nullptr);
    void* big = allocator.allocate(1000, 16);
    REQUIRE(reinterpret_cast<SIZE_T>(big) % 16 == 0);
    REQUIRE(allocator.getBlockCount() == 2);
    const SIZE_T capacity = allocator.getCapacity();
    REQUIRE(capacity >= 256 + 1000);

    for (int32 frame = 0; frame < 3; ++frame) {
      allocator.reset();
      REQUIRE(allocator.allocate(100) != nullptr);
      REQUIRE(allocator.allocate(1000, 16) != nullptr);
    }
    REQUIRE(allocator.getBlockCount() == 2);
    REQUIRE(allocator.getCapacity() == capacity);
  }

  SECTION("Reset destroys objects, newest first")
  {
    Vector<int32> destroyed;
    struct Tracked
    {
      Tracked(Vector<int32>& log, int32 id) : m_log(log), m_id(id) {}
      ~Tracked() { m_log.push_back(m_id); }
      Vector<int32>& m_log;
      int32 m_id;
    };

    const Tracked* first = allocator.create<Tracked>(destroyed, 1);
    const Tracked* second = allocator.create<Tracked>(destroyed, 2);
    const int32* plain = allocator.create<int32>(7);
    REQUIRE(first->m_id == 1);
    REQUIRE(second->m_id == 2);
    REQUIRE(*plain == 7);

    allocator.reset();
    REQUIRE(destroyed == Vector<int32>{2, 1});

    allocator.reset();
    REQUIRE(destroyed.size() == 2);
  }
}

namespace {
enum class TestFlag : uint8
{
  None = 0,
  A = 1 << 0,
  B = 1 << 1,
  C = 1 << 2
};
CH_FLAGS(TestFlag);

static_assert(std::is_trivially_copyable_v<TestFlagFlags>);
static_assert(sizeof(TestFlagFlags) == sizeof(uint8));
static_assert(!std::is_convertible_v<TestFlagFlags, uint32>);
constexpr TestFlagFlags kAB = TestFlag::A | TestFlag::B;
static_assert(kAB.getBits() == 3);
static_assert(kAB.isSet(TestFlag::A) && !kAB.isSet(TestFlag::C));
static_assert((TestFlag::A & TestFlag::B) == TestFlag::None);
static_assert((TestFlag::A ^ TestFlag::A) == TestFlag::None);
} // namespace

TEST_CASE("chUtilities - Flags", "[Util]") {
  TestFlagFlags flags;
  REQUIRE_FALSE(static_cast<bool>(flags));
  REQUIRE(flags == TestFlag::None);
  REQUIRE(flags.isSet(TestFlag::None));
  REQUIRE_FALSE(flags.isSetAny(TestFlag::None));

  flags.set(TestFlag::A).set(TestFlag::C);
  REQUIRE(flags.getBits() == 5);
  REQUIRE(flags.isSet(TestFlag::A | TestFlag::C));
  REQUIRE_FALSE(flags.isSet(TestFlag::A | TestFlag::B));
  REQUIRE(flags.isSetAny(TestFlag::A | TestFlag::B));
  REQUIRE(TestFlag::A != flags);

  flags.unset(TestFlag::A);
  REQUIRE(flags == TestFlag::C);

  REQUIRE((kAB & TestFlag::B) == TestFlag::B);
  REQUIRE((kAB ^ TestFlag::A) == TestFlag::B);
  REQUIRE((~TestFlag::A).getBits() == 0xFE);
  REQUIRE((kAB | TestFlag::C).getBits() == 7);

  TestFlagFlags mixed = TestFlag::A;
  mixed |= TestFlag::B;
  mixed &= TestFlag::B | TestFlag::C;
  mixed ^= TestFlag::C;
  REQUIRE(mixed == (TestFlag::B | TestFlag::C));
  REQUIRE(TestFlagFlags::fromBits(6) == mixed);
}
