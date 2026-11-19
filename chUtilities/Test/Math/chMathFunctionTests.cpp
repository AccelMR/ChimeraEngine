/************************************************************************/
/**
 * @file chMathFunctionTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of PlatformMath, angles, Float16 and random numbers.
 */
/************************************************************************/
#include <cmath>
#include <limits>

#include "chAngle.h"
#include "chFileStream.h"
#include "chFileSystem.h"
#include "chFloat16.h"
#include "chLogger.h"
#include "chMath.h"
#include "chPath.h"
#include "chRandom.h"
#include "chStringUtils.h"
#include "chUnicode.h"

#include "catch.hpp"

using namespace chEngineSDK;

/************************************************************************/
/*
 * Math trigonometric Tests.  Radian and Degree Class Non-Dependent
 */
/************************************************************************/
TEST_CASE("chUtilities - PlatformMath", "[Math]") {
  // Static Variables
  REQUIRE(Math::PI == Approx(3.14159274f));
  REQUIRE(Math::RAD2DEG == Approx(57.295776f));
  REQUIRE(Math::DEG2RAD == Approx(0.0174532924f));
  REQUIRE(Math::TWO_PI == Approx(6.28318548f));
  REQUIRE(Math::HALF_PI == Approx(1.57079637f));
  REQUIRE(Math::QUARTER_PI == Approx(0.78539816339f));

  // Every constant must be the float nearest to the exact value.
  constexpr double kPi = 3.14159265358979323846;
  REQUIRE(Math::PI == static_cast<float>(kPi));
  REQUIRE(Math::TWO_PI == static_cast<float>(kPi * 2.0));
  REQUIRE(Math::HALF_PI == static_cast<float>(kPi / 2.0));
  REQUIRE(Math::QUARTER_PI == static_cast<float>(kPi / 4.0));
  REQUIRE(Math::INV_PI == static_cast<float>(1.0 / kPi));
  REQUIRE(Math::RAD2DEG == static_cast<float>(180.0 / kPi));
  REQUIRE(Math::DEG2RAD == static_cast<float>(kPi / 180.0));

  // Known at compile time, so other modules fold them.
  static_assert(Math::PI > 3.14f && Math::PI < 3.15f);
  static_assert(Math::abs(-2.0f) == 2.0f);
  static_assert(Math::clamp(5, 0, 3) == 3);
  static_assert(!Math::isFinite(std::numeric_limits<float>::infinity()));

  // Functions
  REQUIRE(Math::unwindDegrees(270.0f) == Approx(-90.0f));
  REQUIRE(Math::unwindDegrees(-270.0f) == Approx(90.0f));
  REQUIRE(Math::unwindDegrees(725.0f) == Approx(5.0f));
  REQUIRE(Math::unwindDegrees(-725.0f) == Approx(-5.0f));
  REQUIRE(Math::unwindDegrees(180.0f) == 180.0f);
  REQUIRE(Math::unwindDegrees(-180.0f) == -180.0f);
  REQUIRE(Math::unwindDegrees(45.0f) == 45.0f);
  REQUIRE(Math::unwindDegrees(720.0f) == 0.0f);
  REQUIRE(Math::unwindRadians(Math::TWO_PI) == 0.0f);
  REQUIRE(Math::unwindRadians(4.71239f) == Approx(-1.5707955f));
  REQUIRE(Math::unwindRadians(-4.71239f) == Approx(1.5707955f));

  // Subtracting 360 no longer changes a float this big, so a loop would never end.
  const float bigDegrees = Math::unwindDegrees(1.0e10f);
  REQUIRE(bigDegrees >= -180.0f);
  REQUIRE(bigDegrees <= 180.0f);
  const float bigRadians = Math::unwindRadians(-1.0e10f);
  REQUIRE(bigRadians >= -Math::PI);
  REQUIRE(bigRadians <= Math::PI);

  REQUIRE(Math::square(3.0f) == 9.0f);
  REQUIRE(Math::min(2.0f, 3.0f) == 2.0f);
  REQUIRE(Math::max(2.0f, 3.0f) == 3.0f);
  REQUIRE(Math::clamp(-1.0f, 0.0f, 1.0f) == 0.0f);
  REQUIRE(Math::clamp(0.5f, 0.0f, 1.0f) == 0.5f);
  REQUIRE(Math::nearEqual(1.0f, 1.0f + Math::SMALL_NUMBER * 0.5f));
  REQUIRE_FALSE(Math::nearEqual(1.0f, 1.001f));

  // abs clears the sign bit, so -0 becomes +0.
  REQUIRE_FALSE(std::signbit(Math::abs(-0.0f)));
  REQUIRE(Math::abs(-std::numeric_limits<float>::infinity()) ==
          std::numeric_limits<float>::infinity());

  REQUIRE_FALSE(Math::isFinite(std::numeric_limits<float>::infinity()));
  REQUIRE_FALSE(Math::isFinite(-std::numeric_limits<float>::infinity()));
  REQUIRE_FALSE(Math::isFinite(std::numeric_limits<float>::quiet_NaN()));
  REQUIRE(Math::isFinite(std::numeric_limits<float>::max()));
  REQUIRE(Math::isFinite(std::numeric_limits<float>::denorm_min()));

  // sinCos against the standard library over several turns, both signs.
  float maxSinError = 0.0f;
  float maxCosError = 0.0f;
  for (int32 step = -2000; step <= 2000; ++step) {
    const float angle = static_cast<float>(step) * 0.01f;
    float sinValue = 0.0f;
    float cosValue = 0.0f;
    Math::sinCos(angle, sinValue, cosValue);
    maxSinError = Math::max(maxSinError, Math::abs(sinValue - std::sin(angle)));
    maxCosError = Math::max(maxCosError, Math::abs(cosValue - std::cos(angle)));
  }
  REQUIRE(maxSinError < 1.0e-5f);
  REQUIRE(maxCosError < 1.0e-5f);

  // Values that used to overflow the integer cast must not crash, and NaN stays NaN.
  float hugeSin = 0.0f;
  float hugeCos = 0.0f;
  Math::sinCos(1.0e20f, hugeSin, hugeCos);
  REQUIRE(Math::abs(hugeSin) <= 1.0f);
  REQUIRE(Math::abs(hugeCos) <= 1.0f);
  Math::sinCos(std::numeric_limits<float>::quiet_NaN(), hugeSin, hugeCos);
  REQUIRE_FALSE(Math::isFinite(hugeSin));
  REQUIRE_FALSE(Math::isFinite(hugeCos));

  REQUIRE(Math::sqrt(25.0f) == Approx(5.0f));
  REQUIRE(Math::invSqrt(25.0f) == Approx(0.2f));
  REQUIRE(Math::pow(5.0f, 2.0f) == Approx(25.0f));
  REQUIRE(Math::pow(3.0f, 3.0f) == Approx(27.0f));
  REQUIRE(Math::floor(2.7f) == 2.0f);
  REQUIRE(Math::floor(-2.2f) == -3.0f);
  REQUIRE(Math::ceil(2.2f) == 3.0f);
  REQUIRE(Math::ceil(-2.7f) == -2.0f);
  REQUIRE(Math::log2(8.0f) == 3.0f);
  REQUIRE(Math::log2(1.0f) == 0.0f);
  REQUIRE(Math::abs(Math::log2(0.5f) + 1.0f) < 1e-6f);
  REQUIRE(Math::abs(-3.0f) == Approx(3.0f));

  REQUIRE(Math::lerp(56.0f, 76.0f, 4.0f) == Approx(136.0f));
  REQUIRE(Math::lerp(56.0f, 76.0f, 1.0f) == Approx(76.0f));

  REQUIRE(Math::invLerp(56.0f, 76.0f, 4.0f) == Approx(-2.6f));
  REQUIRE(Math::invLerp(56.0f, 76.0f, 1.0f) == Approx(-2.75f));

  REQUIRE(Math::isFinite(1551.0f) == true);
  REQUIRE(Math::isFinite(Math::sqrt(-1.0f)) == false);

  REQUIRE(Math::fmod(545.0f, 360.0f) == Approx(185.f));
  REQUIRE(Math::fmod(7.5f, 2.1f) == Approx(1.2f));
}

/************************************************************************/
/*
 * Radian Degree Tests.
 */
/************************************************************************/
TEST_CASE("chUtilities - Degree", "[Math]") {
  // Degree{} value-initializes to zero; a plain declaration stays uninitialized on purpose.
  const Degree zeroed{};
  REQUIRE(zeroed.valueDegree() == 0.0f);

  static_assert(Degree(180.0f).valueRadian() == Math::PI);
  static_assert(Degree(90.0f) < Degree(180.0f));

  const Degree straight(180.0f);
  REQUIRE(straight.valueDegree() == 180.0f);
  REQUIRE(straight.valueRadian() == Approx(Math::PI));

  // Conversion is explicit and goes both ways.
  const Degree fromRadian{Radian(Math::HALF_PI)};
  REQUIRE(fromRadian.valueDegree() == Approx(90.0f));
  REQUIRE(Degree(Radian(Degree(37.5f))).valueDegree() == Approx(37.5f));

  REQUIRE(Degree(10.0f) + Degree(20.0f) == Degree(30.0f));
  REQUIRE(Degree(10.0f) - Degree(20.0f) == Degree(-10.0f));
  REQUIRE(-Degree(10.0f) == Degree(-10.0f));
  REQUIRE(Degree(10.0f) * 3.0f == Degree(30.0f));
  REQUIRE(3.0f * Degree(10.0f) == Degree(30.0f));
  REQUIRE(Degree(30.0f) / 3.0f == Degree(10.0f));

  // Mixing units needs an explicit conversion.
  REQUIRE((Degree(90.0f) + Degree(Radian(Math::HALF_PI))).valueDegree() == Approx(180.0f));

  Degree compound(10.0f);
  compound += Degree(5.0f);
  REQUIRE(compound == Degree(15.0f));
  compound -= Degree(10.0f);
  REQUIRE(compound == Degree(5.0f));
  compound *= 4.0f;
  REQUIRE(compound == Degree(20.0f));
  compound /= 2.0f;
  REQUIRE(compound == Degree(10.0f));

  REQUIRE(Degree(10.0f) < Degree(20.0f));
  REQUIRE(Degree(20.0f) > Degree(10.0f));
  REQUIRE(Degree(10.0f) <= Degree(10.0f));
  REQUIRE(Degree(10.0f) >= Degree(10.0f));
  REQUIRE(Degree(10.0f) != Degree(20.0f));

  Degree toUnwind(270.0f);
  REQUIRE(toUnwind.unwindedValue() == Approx(-90.0f));
  REQUIRE(toUnwind.valueDegree() == 270.0f);
  toUnwind.unwind();
  REQUIRE(toUnwind.valueDegree() == Approx(-90.0f));
  REQUIRE(Degree(720.0f).unwindedValue() == 0.0f);
}

TEST_CASE("chUtilities - Radian", "[Math]") {
  const Radian zeroed{};
  REQUIRE(zeroed.valueRadian() == 0.0f);

  static_assert(Radian(Math::PI).valueDegree() == 180.0f);
  static_assert(Radian(1.0f) > Radian(0.5f));

  const Radian half(Math::PI);
  REQUIRE(half.valueRadian() == Math::PI);
  REQUIRE(half.valueDegree() == Approx(180.0f));

  const Radian fromDegree{Degree(90.0f)};
  REQUIRE(fromDegree.valueRadian() == Approx(Math::HALF_PI));
  REQUIRE(Radian(Degree(Radian(1.25f))).valueRadian() == Approx(1.25f));

  REQUIRE(Radian(1.0f) + Radian(2.0f) == Radian(3.0f));
  REQUIRE(Radian(1.0f) - Radian(2.0f) == Radian(-1.0f));
  REQUIRE(-Radian(1.0f) == Radian(-1.0f));
  REQUIRE(Radian(1.0f) * 3.0f == Radian(3.0f));
  REQUIRE(3.0f * Radian(1.0f) == Radian(3.0f));
  REQUIRE(Radian(3.0f) / 3.0f == Radian(1.0f));

  REQUIRE((Radian(Math::HALF_PI) + Radian(Degree(90.0f))).valueRadian() ==
          Approx(Math::PI));

  Radian compound(1.0f);
  compound += Radian(0.5f);
  REQUIRE(compound == Radian(1.5f));
  compound -= Radian(1.0f);
  REQUIRE(compound == Radian(0.5f));
  compound *= 4.0f;
  REQUIRE(compound == Radian(2.0f));
  compound /= 2.0f;
  REQUIRE(compound == Radian(1.0f));

  REQUIRE(Radian(1.0f) < Radian(2.0f));
  REQUIRE(Radian(2.0f) > Radian(1.0f));
  REQUIRE(Radian(1.0f) <= Radian(1.0f));
  REQUIRE(Radian(1.0f) >= Radian(1.0f));
  REQUIRE(Radian(1.0f) != Radian(2.0f));

  Radian toUnwind(Math::TWO_PI);
  REQUIRE(toUnwind.unwindedValue() == 0.0f);
  toUnwind.unwind();
  REQUIRE(toUnwind.valueRadian() == 0.0f);
  REQUIRE(Radian(4.71239f).unwindedValue() == Approx(-1.5707955f));
}

/************************************************************************/
/*
 * Math trigonometric Tests.  Radian and Degree Class Dependent
 */
/************************************************************************/
TEST_CASE("chUtilities - MathTrigonometricRadianDegree", "[Math]") {
  Radian RadianToTest1(Math::HALF_PI);
  Radian RadianToTest2(Math::PI);
  Degree DegreeToTest1(RadianToTest1);
  Degree DegreeToTest2(RadianToTest2);

  REQUIRE(Math::cos(RadianToTest1) == Approx(0.0f).margin(Math::KINDA_SMALL_NUMBER));
  REQUIRE(Math::cos(DegreeToTest1) == Approx(0.0f).margin(Math::KINDA_SMALL_NUMBER));

  REQUIRE(Math::sin(RadianToTest1) == Approx(1.0f));
  REQUIRE(Math::sin(DegreeToTest1) == Approx(1.0f));

  REQUIRE(Math::tan(RadianToTest2) == Approx(0.0f).margin(Math::KINDA_SMALL_NUMBER));
  REQUIRE(Math::tan(DegreeToTest2) == Approx(0.0f).margin(Math::KINDA_SMALL_NUMBER));

  Radian RadiancoAcos = Math::acos(-1.0f);
  const Degree DegreecoAcos(Math::acos(-1.0f));
  REQUIRE(RadiancoAcos.valueDegree() == Approx(180.0f).margin(Math::KINDA_SMALL_NUMBER));
  REQUIRE(DegreecoAcos.valueDegree() == Approx(180.0f).margin(Math::KINDA_SMALL_NUMBER));

  Radian RadiancoAsin = Math::asin(1.0f);
  const Degree DegreecoAsin(Math::asin(1.0f));
  REQUIRE(RadiancoAsin.valueDegree() == Approx(90.0f).margin(Math::KINDA_SMALL_NUMBER));
  REQUIRE(DegreecoAsin.valueDegree() == Approx(90.0f).margin(Math::KINDA_SMALL_NUMBER));

  Radian RadiancoAtan = Math::atan(1.0f);
  const Degree DegreecoAtan(Math::atan(1.0f));
  REQUIRE(RadiancoAtan.valueDegree() == Approx(45.0f).margin(Math::KINDA_SMALL_NUMBER));
  REQUIRE(DegreecoAtan.valueDegree() == Approx(45.0f).margin(Math::KINDA_SMALL_NUMBER));

  Radian RadiancoAtan2 = Math::atan2(1.0f, 1.0f);
  Degree DegreecoAtan2(Math::atan2(1.0f, 1.0f));
  REQUIRE(RadiancoAtan2.valueRadian() ==
          Approx(0.785398163397f).margin(Math::KINDA_SMALL_NUMBER));
  REQUIRE(DegreecoAtan2.valueRadian() ==
          Approx(0.785398163397f).margin(Math::KINDA_SMALL_NUMBER));

  // Rounding can push a dot product of unit vectors just past 1.
  REQUIRE(Math::acos(1.0000001f).valueRadian() == 0.0f);
  REQUIRE(Math::acos(-1.0000001f).valueRadian() == Approx(Math::PI));
  REQUIRE(Math::asin(1.0000001f).valueRadian() == Approx(Math::HALF_PI));
  REQUIRE(Math::asin(-1.0000001f).valueRadian() == Approx(-Math::HALF_PI));
}

TEST_CASE("chUtilities - MathHyperbolic", "[Math]") {
  REQUIRE(Math::cosh(Math::PI) == Approx(11.59195328f).margin(Math::KINDA_SMALL_NUMBER));
  REQUIRE(Math::sinh(Math::PI) == Approx(11.548739368f).margin(Math::KINDA_SMALL_NUMBER));
  REQUIRE(Math::tanh(Math::HALF_PI) == Approx(0.91715234f).margin(Math::KINDA_SMALL_NUMBER));
  REQUIRE(Math::acosh(2.0f) == Approx(1.316957896925f).margin(Math::KINDA_SMALL_NUMBER));
  REQUIRE(Math::asinh(2.0f) == Approx(1.443635475179f).margin(Math::KINDA_SMALL_NUMBER));
  REQUIRE(Math::atanh(0.6f) == Approx(0.69314718056f).margin(Math::KINDA_SMALL_NUMBER));
}

// TEST_CASE("chUtilities - StringAndUTF8") {
//     const U16String TestWString(UTF8::toUTF16("Created as wide string"));
//     const String WellPerformedConvertion("Created as wide string");

//     REQUIRE(UTF8::fromWide(TestWString) == WellPerformedConvertion);
//     REQUIRE(UTF8::toWide(WellPerformedConvertion) == TestWString);

//     const String ReplaceTestString("Test-string-that-should-replace-all-hyphens");
//     const String WellPerformedReplacing("Test string that should replace all hyphens");
//     REQUIRE(StringUtils::replaceAllChars(ReplaceTestString, '-', ' ') ==
//     WellPerformedReplacing);

//     const String ReplaceSubStringtest("Test that asdfg contains asdfg some asdfg substrings
//     to erase"); const String WellPerformedSubStrReplace("Test that sd contains sd some sd
//     substrings to erase"); REQUIRE(StringUtils::replaceAllSubStr(ReplaceSubStringtest, "asdfg",
//     "sd") == WellPerformedSubStrReplace);

//     const String ToSplitChar("This-is-a-test-that-should-split-by-hyphens");
//     REQUIRE(StringUtils::splitString(ToSplitChar, '-').size() == 9);

//     const String
//     ToSplitString("This123is123a123test123that123should123split123by123hyphen");
//     REQUIRE(StringUtils::splitString(ToSplitString, "123").size() == 9);

//     String Formated = StringUtils::format("hello world, {0}, {0}, {1}, {2}, {3}",
//         123, 1.23, "123", String("123"));

//     const String Anwser("hello world, 123, 123, 1.230000, 123, 123");
//     REQUIRE(Anwser == Formated);

//     // Platform-specific paths
// #if USING(CH_PLATFORM_WIN32)
//     const Path TestPath("C:\\Users\\Public\\");
//     Path WrongTest("C:/Users/Public/NoExist/");
//     const Path ToCreate("C:/Users/Public/Test/");
//     const Path ToCreateMany("C:/Users/Public/Test/a/b/c/");
//     REQUIRE(TestPath.toString() == "C:/Users/Public/");
//     REQUIRE(WrongTest.toString() == "C:/Users/Public/NoExist/");
// #elif USING(CH_PLATFORM_LINUX)
//     const Path TestPath("/home/accelmr/Public/");
//     Path WrongTest("/home/accelmr/Public/NoExist/");
//     const Path ToCreate("/home/accelmr/Public/Test/");
//     const Path ToCreateMany("/home/accelmr/Public/Test/a/b/c/");

//     REQUIRE(TestPath.toString() == "/home/accelmr/Public/");
//     REQUIRE(WrongTest.toString() == "/home/accelmr/Public/NoExist/");
// #endif

//     Path Relativepath2("./Here/path");
//     Path Relativepath3("Here/path");

//     REQUIRE(Relativepath2.toString() == "./Here/path");
//     REQUIRE(Relativepath3.toString() == "Here/path");

//     REQUIRE(FileSystem::createDirectory(ToCreate));

//     REQUIRE(FileSystem::createDirectories(ToCreateMany));

//     SPtr<DataStream> memStream = chMakeShared<MemoryDataStream>(8);
//     REQUIRE(memStream->size() == 8);

//     int Test1110 = 110;
//     memStream->write(&Test1110, sizeof(int));
//     *memStream << 18;

//     SIZE_T moved = memStream->tell();
//     REQUIRE(moved == 8);
//     memStream->seek(0);

//     int InTest110 = 0;
//     memStream->read(&InTest110, sizeof(int));
//     REQUIRE(InTest110 == 110);

//     int Intest18;
//     *memStream >> Intest18;
//     REQUIRE(Intest18 == 18);

//     REQUIRE_FALSE(memStream->isFile());
//     REQUIRE(memStream->isReadable());
//     REQUIRE(memStream->isWriteable());

//     Path testFilePath(ToCreateMany.toString() + "testFile.test");
//     FileSystem::dumpMemStreamIntoFile(memStream, testFilePath);

//     auto readFileSteam = FileSystem::openFile(testFilePath);
//     REQUIRE(readFileSteam);

//     int FileInTest110 = 0;
//     readFileSteam->read(&FileInTest110, sizeof(int));
//     REQUIRE(FileInTest110 == 110);

//     int FileIntest18;
//     *readFileSteam >> FileIntest18;
//     REQUIRE(FileIntest18 == 18);

//     memStream.reset();
//     REQUIRE_FALSE(memStream);

//     readFileSteam->close();

//     const Path newFilePath(ToCreateMany.toString() + "testFile2.test2");
//     SPtr<DataStream> newFile = FileSystem::createAndOpenFile(newFilePath);

//     *newFile << "This is a test to see if write() is ok" << " concat ok " << "\n";
//     *newFile << "This must be a new line";
//     newFile->close();

//     auto readFile = FileSystem::openFile(newFilePath);
//     String fromFile = readFile->getAsString();

//     const String mustBe("This is a test to see if write() is ok\xEF\xBB\xBF concat ok
//     \xEF\xBB\xBF\n\xEF\xBB\xBFThis must be a new line"); REQUIRE(mustBe == fromFile);

//     readFile->close();

//     REQUIRE_THROWS_AS(FileSystem::remove(ToCreate), std::exception);
//     REQUIRE(FileSystem::removeAll(ToCreate));

//     /*
//     const Path DebugFile("C:/Users/Public/debug.txt");
//     CH_LOG_DEBUG("TEST");
//     CH_LOG_ERROR("TEST");
//     CH_LOG_WARNING("TEST");
//     g_Debug().saveLog(DebugFile);*/
// }

TEST_CASE("chUtilities - RandomNumbers", "[Math]") {
  uint32 randomNumber = 0;
  Random rnd = Random();

  randomNumber = rnd.getPseudoRandom() % 1000;
  REQUIRE(randomNumber == 363);

  for (uint8 i = 0; i < 10; i++) {
    randomNumber = rnd.getPseudoRandom() % 1000;
  }

  REQUIRE(randomNumber == 735);
}

TEST_CASE("chUtilities - Float16", "[Math]")
{
  SECTION("Exact values")
  {
    REQUIRE(Float16::fromFloat(0.0f) == 0x0000);
    REQUIRE(Float16::fromFloat(-0.0f) == 0x8000);
    REQUIRE(Float16::fromFloat(1.0f) == 0x3C00);
    REQUIRE(Float16::fromFloat(-2.0f) == 0xC000);
    REQUIRE(Float16::fromFloat(0.5f) == 0x3800);
    REQUIRE(Float16::fromFloat(65504.0f) == 0x7BFF);
    // Smallest normal and smallest subnormal half.
    REQUIRE(Float16::fromFloat(6.103515625e-05f) == 0x0400);
    REQUIRE(Float16::fromFloat(5.9604644775390625e-08f) == 0x0001);
  }

  SECTION("Rounding")
  {
    // 1 + 2^-11 is halfway between 1 and the next half: ties go to the even one.
    REQUIRE(Float16::fromFloat(1.0f + 1.0f / 2048.0f) == 0x3C00);
    REQUIRE(Float16::fromFloat(1.0f + 3.0f / 2048.0f) == 0x3C02);
    REQUIRE(Float16::fromFloat(1.0f + 1.1f / 2048.0f) == 0x3C01);
    // Half of the smallest subnormal rounds to zero, a bit more rounds up.
    REQUIRE(Float16::fromFloat(2.98023223876953125e-08f) == 0x0000);
    REQUIRE(Float16::fromFloat(3.1e-08f) == 0x0001);
  }

  SECTION("Out of range")
  {
    REQUIRE(Float16::fromFloat(65520.0f) == 0x7C00);
    REQUIRE(Float16::fromFloat(-1.0e10f) == 0xFC00);
    REQUIRE(Float16::fromFloat(std::numeric_limits<float>::infinity()) == 0x7C00);
    const uint16 nan = Float16::fromFloat(std::numeric_limits<float>::quiet_NaN());
    REQUIRE((nan & 0x7C00) == 0x7C00);
    REQUIRE((nan & 0x03FF) != 0);
    REQUIRE(std::isnan(Float16::toFloat(nan)));
  }

  SECTION("Every half survives a round trip")
  {
    for (uint32 bits = 0; bits <= 0xFFFF; ++bits) {
      const uint16 half = static_cast<uint16>(bits);
      const float value = Float16::toFloat(half);
      if (std::isnan(value)) {
        continue;
      }
      REQUIRE(Float16::fromFloat(value) == half);
    }
    REQUIRE(Float16::toFloat(0x0001) == 5.9604644775390625e-08f);
    REQUIRE(Float16::toFloat(0x3555) == 0.333251953125f);
  }
}
