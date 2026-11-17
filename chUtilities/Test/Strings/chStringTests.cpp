/************************************************************************/
/**
 * @file chStringTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of string helpers, formatting, numbers as text and UTF-8.
 */
/************************************************************************/
#include <limits>

#include "chAngle.h"
#include "chBox2D.h"
#include "chCapsule.h"
#include "chFrustum.h"
#include "chLinearColor.h"
#include "chMatrix4.h"
#include "chMatrixHelpers.h"
#include "chNumberFormat.h"
#include "chOrientedBox.h"
#include "chPath.h"
#include "chPlane.h"
#include "chQuaternion.h"
#include "chRay.h"
#include "chRotator.h"
#include "chSphereBoxBounds.h"
#include "chStringUtils.h"
#include "chUnicode.h"
#include "chVector2.h"
#include "chVector2I.h"
#include "chVector3.h"
#include "chVector3I.h"
#include "chVector4.h"

#include "catch.hpp"

using namespace chEngineSDK;

TEST_CASE("chUtilities - ContainsIgnoreCase", "[Strings]") {
  REQUIRE(StringUtils::containsIgnoreCase("Texture_Wood", "wood"));
  REQUIRE(StringUtils::containsIgnoreCase("Texture_Wood", "TEXTURE"));
  REQUIRE(StringUtils::containsIgnoreCase("Texture_Wood", "e_w"));
  REQUIRE(StringUtils::containsIgnoreCase("abc", "abc"));
  REQUIRE(StringUtils::containsIgnoreCase("abc", ""));
  REQUIRE(StringUtils::containsIgnoreCase("", ""));
  REQUIRE_FALSE(StringUtils::containsIgnoreCase("abc", "abcd"));
  REQUIRE_FALSE(StringUtils::containsIgnoreCase("", "a"));
  REQUIRE_FALSE(StringUtils::containsIgnoreCase("Texture_Wood", "stone"));
  REQUIRE_FALSE(StringUtils::containsIgnoreCase("aab", "abb"));

  REQUIRE(StringUtils::equalsIgnoreCase("PNG", "png"));
  REQUIRE(StringUtils::equalsIgnoreCase("", ""));
  REQUIRE_FALSE(StringUtils::equalsIgnoreCase("png", "pn"));
  REQUIRE(StringUtils::compareIgnoreCase("apple", "BANANA") < 0);
  REQUIRE(StringUtils::compareIgnoreCase("Zebra", "apple") > 0);
  REQUIRE(StringUtils::compareIgnoreCase("PNG", "png") == 0);
  REQUIRE(StringUtils::compareIgnoreCase("pn", "PNG") < 0);
  REQUIRE(StringUtils::compareIgnoreCase("", "") == 0);
  REQUIRE_FALSE(StringUtils::equalsIgnoreCase("png", "jpg"));
}

TEST_CASE("chUtilities - Format", "[Strings]") {
  enum class TestEnum : int32 { A = 3, B = -2 };

  SECTION("default output") {
    REQUIRE(StringUtils::format("{} {} {}", 42, -7, 18446744073709551615ull) ==
            "42 -7 18446744073709551615");
    REQUIRE(StringUtils::format("{} {} {} {}", 1.0, 0.1, 1e20, -2.5f) == "1 0.1 1e+20 -2.5");
    // A float is written as a float: its double would read 0.10000000149011612.
    REQUIRE(StringUtils::format("{} {:.2f}", 0.1f, 0.1f) == "0.1 0.10");
    REQUIRE(StringUtils::format("{}|{}|{}", 'a', true, false) == "a|true|false");
    REQUIRE(StringUtils::format("{}", static_cast<uint8>(65)) == "65");
    REQUIRE(StringUtils::format("{} {}", TestEnum::A, TestEnum::B) == "3 -2");
    REQUIRE(StringUtils::format("{1}-{0}-{1}", "x", String("y")) == "y-x-y");
    REQUIRE(StringUtils::format("{} {}", Path("a\\b"), static_cast<const ANSICHAR*>(nullptr)) ==
            "a/b (null)");
    REQUIRE(StringUtils::format("{{{}}}", 5) == "{5}");
    REQUIRE(StringUtils::toString(0.5) == "0.5");
  }

  SECTION("specs") {
    REQUIRE(StringUtils::format("{:.2f}", 3.14159) == "3.14");
    REQUIRE(StringUtils::format("{:f}", 1.0) == "1.000000");
    REQUIRE(StringUtils::format("{:.3e}", 12345.678) == "1.235e+04");
    REQUIRE(StringUtils::format("{:.3}", 3.14159) == "3.14");
    REQUIRE(StringUtils::format("{:.1f}", 1e300).size() == 303);
    REQUIRE(StringUtils::format("{:x} {:X} {:b} {:o}", 255, 255u, 5, 8) == "ff FF 101 10");
    REQUIRE(StringUtils::format("{:08x}", 0xbeefu) == "0000beef");
    REQUIRE(StringUtils::format("{:05}", -42) == "-0042");
    REQUIRE(StringUtils::format("{:6.2f}", -1.5) == " -1.50");
    REQUIRE(StringUtils::format("[{:>5}][{:<5}][{:^5}]", 1, 2, 3) == "[    1][2    ][  3  ]");
    REQUIRE(StringUtils::format("[{:6}][{:>6}][{:*^7}]", "ab", "ab", "ab") ==
            "[ab    ][    ab][**ab***]");
    REQUIRE(StringUtils::format("[{0:>3}][{0}]", 'c') == "[  c][c]");
    REQUIRE(StringUtils::format("{:2}", "longer") == "longer");
  }
}

TEST_CASE("chUtilities - Math toString", "[Strings]") {
  REQUIRE(Vector2(1.0f, -2.5f).toString() == "Vector2(1.000, -2.500)");
  REQUIRE(Vector3(1.0f, 2.0f, 3.0f).toString() == "Vector3(1.000, 2.000, 3.000)");
  REQUIRE(Vector4(1.0f, 2.0f, 3.0f, 4.0f).toString() == "Vector4(1.000, 2.000, 3.000, 4.000)");
  REQUIRE(Vector2I(1, -2).toString() == "Vector2I(1, -2)");
  REQUIRE(Vector3I(1, 2, 3).toString() == "Vector3I(1, 2, 3)");
  REQUIRE(Radian(1.5f).toString() == "1.5000 rad");
  REQUIRE(Degree(90.0f).toString() == "90.00 deg");
  REQUIRE(Quaternion::IDENTITY.toString() == "Quaternion(0.0000, 0.0000, 0.0000, 1.0000)");
  REQUIRE(Rotator(10.0f, 20.0f, 30.0f).toString() ==
          "Rotator(pitch 10.00, yaw 20.00, roll 30.00)");
  REQUIRE(Matrix4::IDENTITY.toString() ==
          "Matrix4([1.000, 0.000, 0.000, 0.000], [0.000, 1.000, 0.000, 0.000], "
          "[0.000, 0.000, 1.000, 0.000], [0.000, 0.000, 0.000, 1.000])");
  REQUIRE(LinearColor(1.0f, 0.5f, 0.25f).toString() ==
          "LinearColor(1.000, 0.500, 0.250, 1.000)");
  REQUIRE(Plane(Vector3::UP, 2.0f).toString() ==
          "Plane(normal (0.000, 0.000, 1.000), w 2.000)");
  REQUIRE(AABox(Vector3::ZERO, Vector3::UNIT).toString() ==
          "AABox(min (0.000, 0.000, 0.000), max (1.000, 1.000, 1.000))");
  REQUIRE(Sphere(Vector3::UNIT, 2.0f).toString() ==
          "Sphere(center (1.000, 1.000, 1.000), radius 2.000)");
  REQUIRE(SphereBoxBounds(Vector3::ZERO, Vector3::UNIT, 1.5f).toString() ==
          "SphereBoxBounds(center (0.000, 0.000, 0.000), extent (1.000, 1.000, 1.000), "
          "radius 1.500)");
  REQUIRE(Box2D(Vector2::ZERO, Vector2::UNIT).toString() ==
          "Box2D(min (0.000, 0.000), max (1.000, 1.000))");
  REQUIRE(Ray(Vector3::ZERO, Vector3::FORWARD).toString() ==
          "Ray(origin (0.000, 0.000, 0.000), direction (1.000, 0.000, 0.000))");
  REQUIRE(OrientedBox(Vector3::ZERO, Vector3::UNIT, Quaternion::IDENTITY).toString() ==
          "OrientedBox(center (0.000, 0.000, 0.000), extent (1.000, 1.000, 1.000), "
          "rotation Rotator(pitch 0.00, yaw 0.00, roll 0.00))");
  REQUIRE(Capsule(Vector3::ZERO, Vector3::UP, 0.5f).toString() ==
          "Capsule(start (0.000, 0.000, 0.000), end (0.000, 0.000, 1.000), radius 0.500)");

  const Frustum frustum(OrthographicMatrix(10.0f, 10.0f, 1.0f, 100.0f));
  const String frustumText = frustum.toString();
  REQUIRE(frustumText.starts_with("Frustum(left ("));
  REQUIRE(frustumText.find("near (0.000, 0.000, 1.000, ") != String::npos);

  // Logs write them in place through formatTo, and a float spec reaches every number.
  REQUIRE(StringUtils::format("at {}", Vector3::UP) == "at Vector3(0.000, 0.000, 1.000)");
  REQUIRE(StringUtils::format("{:.1f}", Vector3(1.26f, -2.0f, 3.0f)) ==
          "Vector3(1.3, -2.0, 3.0)");
  REQUIRE(StringUtils::format("{:.0f}", Rotator(10.0f, 20.0f, 30.0f)) ==
          "Rotator(pitch 10, yaw 20, roll 30)");
  REQUIRE(StringUtils::format("{:.2e}", Vector2(1000.0f, 0.5f)) ==
          "Vector2(1.00e+03, 5.00e-01)");
  REQUIRE(StringUtils::format("{:.1f}", Degree(45.0f)) == "45.0 deg");
  REQUIRE(StringUtils::format("{:.1f}", Vector2I(1, 2)) == "Vector2I(1, 2)");
  REQUIRE(StringUtils::format("{:.1f}", OrientedBox(Vector3::ZERO, Vector3::UNIT,
                                                    Quaternion::IDENTITY)) ==
          "OrientedBox(center (0.0, 0.0, 0.0), extent (1.0, 1.0, 1.0), "
          "rotation Rotator(pitch 0.0, yaw 0.0, roll 0.0))");

  // The width covers the whole text, not each number.
  REQUIRE(StringUtils::format("[{:>24}]", Vector2(1.0f, 2.0f)) ==
          "[   Vector2(1.000, 2.000)]");
  REQUIRE(StringUtils::format("[{:*<24.1f}]|{}", Vector2(1.0f, 2.0f), 7) ==
          "[Vector2(1.0, 2.0)*******]|7");
  REQUIRE(StringUtils::format("[{:^20.0f}]", Vector2(1.0f, 2.0f)) == "[   Vector2(1, 2)    ]");

  REQUIRE(StringUtils::toString(Vector3::UNIT) == "Vector3(1.000, 1.000, 1.000)");
}

TEST_CASE("chUtilities - ToChars", "[Strings]") {
  ANSICHAR integer[StringUtils::MAX_INTEGER_CHARS];
  REQUIRE(StringUtils::toChars(integer, 0) == "0");
  REQUIRE(StringUtils::toChars(integer, static_cast<uint32>(437)) == "437");
  REQUIRE(StringUtils::toChars(integer, static_cast<uint8>(255)) == "255");
  REQUIRE(StringUtils::toChars(integer, static_cast<int64>(-9223372036854775807ll - 1)) ==
          "-9223372036854775808");
  REQUIRE(StringUtils::toChars(integer, static_cast<uint64>(18446744073709551615ull)) ==
          "18446744073709551615");

  ANSICHAR number[StringUtils::MAX_FLOAT_CHARS];
  REQUIRE(StringUtils::toChars(number, 0.1) == "0.1");
  REQUIRE(StringUtils::toChars(number, -2.5f) == "-2.5");
  REQUIRE(StringUtils::toChars(number, 0.1f) == "0.1");
  REQUIRE(StringUtils::toChars(number, -2.2250738585072014e-308) ==
          "-2.2250738585072014e-308");
}

TEST_CASE("chUtilities - Unicode", "[Strings]") {
  // "a", e acute, euro sign, and an emoji that needs a surrogate pair in UTF-16.
  const String utf8 = "a\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80";
  const U16String utf16 = u"a\xE9\x20AC\U0001F600";
  const U32String utf32 = U"a\xE9\x20AC\U0001F600";
  const String replacement = "\xEF\xBF\xBD";

  SECTION("valid text") {
    REQUIRE(UTF8::toUTF16(utf8) == utf16);
    REQUIRE(UTF8::fromUTF16(utf16) == utf8);
    REQUIRE(UTF8::toUTF32(utf8) == utf32);
    REQUIRE(UTF8::fromUTF32(utf32) == utf8);
    REQUIRE(UTF8::fromWide(UTF8::toWide(utf8)) == utf8);
    REQUIRE(UTF8::toUTF16("").empty());
  }

  SECTION("invalid UTF-8") {
    // Overlong '/', 5 byte form, cut sequence before 'A', lone continuation byte, encoded
    // surrogate.
    REQUIRE(UTF8::toUTF32("\xC0\xAF") == U"\xFFFD");
    REQUIRE(UTF8::toUTF32("\xF8\x88\x80\x80\x80").size() == 5);
    REQUIRE(UTF8::toUTF32("\xE2\x82" "A") == U"\xFFFD" "A");
    REQUIRE(UTF8::toUTF32("\x80" "b") == U"\xFFFD" "b");
    REQUIRE(UTF8::toUTF32("\xED\xA0\x80") == U"\xFFFD");
  }

  SECTION("invalid UTF-16 and UTF-32") {
    const U16String loneHigh = {0xD800, u'A'};
    const U16String loneLow = {0xDC00};
    REQUIRE(UTF8::fromUTF16(loneHigh) == replacement + "A");
    REQUIRE(UTF8::fromUTF16(loneLow) == replacement);

    const U32String outOfRange = {0xD800, 0x110000};
    REQUIRE(UTF8::fromUTF32(outOfRange) == replacement + replacement);
    REQUIRE(UTF8::toUTF16(replacement) == u"\xFFFD");
  }
}

TEST_CASE("chUtilities - Number format", "[Strings]")
{
  ANSICHAR buffer[NumberFormat::MAX_CHARS];
  const auto text = [&buffer](double value, const NumberFormatDesc& desc = {}) {
    return String(NumberFormat::format(buffer, value, desc));
  };

  SECTION("Suffixes")
  {
    REQUIRE(text(0.0) == "0");
    REQUIRE(text(999.0) == "999");
    REQUIRE(text(999.4) == "999");
    // Rounds up to 1000, which is not written plainly.
    REQUIRE(text(999.6) == "1K");
    REQUIRE(text(1234.0) == "1.23K");
    REQUIRE(text(1500.0) == "1.5K");
    REQUIRE(text(2000.0) == "2K");
    REQUIRE(text(999999.0) == "1M");
    REQUIRE(text(1e9) == "1B");
    REQUIRE(text(1e12) == "1T");
    REQUIRE(text(1e15) == "1aa");
    REQUIRE(text(1.5e18) == "1.5ab");
    REQUIRE(text(-2500.0) == "-2.5K");
    REQUIRE(text(1e308) == "100dt");
    REQUIRE(text(1500.0, {.bTrimZeros = false}) == "1.50K");
    REQUIRE(text(1234.0, {.decimals = 0}) == "1K");
    REQUIRE(text(12.5, {.plainDecimals = 1}) == "12.5");
    REQUIRE(text(12.0, {.plainDecimals = 1}) == "12");
  }

  SECTION("Scientific and engineering")
  {
    const NumberFormatDesc scientific{.notation = NumberNotation::Scientific};
    REQUIRE(text(123456.0, scientific) == "1.23e5");
    REQUIRE(text(1e100, scientific) == "1e100");
    REQUIRE(text(999.6, scientific) == "1e3");
    REQUIRE(text(9.999e5, scientific) == "1e6");
    REQUIRE(text(500.0, scientific) == "500");

    const NumberFormatDesc engineering{.notation = NumberNotation::Engineering};
    REQUIRE(text(123456.0, engineering) == "123.46e3");
    REQUIRE(text(1234567.0, engineering) == "1.23e6");
    REQUIRE(text(12345678.0, engineering) == "12.35e6");
  }

  SECTION("Plain")
  {
    const NumberFormatDesc plain{.notation = NumberNotation::Plain};
    REQUIRE(text(1234567.0, plain) == "1234567");
    // Too long for the buffer, so it falls back to scientific notation.
    REQUIRE(text(1e300, plain) == "1e300");
  }

  SECTION("Values that are not numbers")
  {
    REQUIRE(text(std::numeric_limits<double>::quiet_NaN()) == "NaN");
    REQUIRE(text(std::numeric_limits<double>::infinity()) == "Inf");
    REQUIRE(text(-std::numeric_limits<double>::infinity()) == "-Inf");
  }

  SECTION("Suffix names")
  {
    ANSICHAR suffix[2];
    REQUIRE(NumberFormat::getSuffix(0, suffix).empty());
    REQUIRE(NumberFormat::getSuffix(1, suffix) == "K");
    REQUIRE(NumberFormat::getSuffix(4, suffix) == "T");
    REQUIRE(NumberFormat::getSuffix(5, suffix) == "aa");
    REQUIRE(NumberFormat::getSuffix(30, suffix) == "az");
    REQUIRE(NumberFormat::getSuffix(31, suffix) == "ba");
    REQUIRE(NumberFormat::getSuffix(NumberFormat::MAX_SUFFIX_THOUSANDS, suffix) == "zz");
    REQUIRE(NumberFormat::getSuffix(NumberFormat::MAX_SUFFIX_THOUSANDS + 1, suffix).empty());
  }
}

TEST_CASE("chUtilities - UTF8 decodeNext", "[Strings]")
{
  // "a", e with acute (2 bytes), euro sign (3 bytes), a broken lead byte, "b".
  const StringView text("a\xC3\xA9\xE2\x82\xAC\xFF" "b");
  SIZE_T position = 0;
  REQUIRE(UTF8::decodeNext(text, position) == 'a');
  REQUIRE(UTF8::decodeNext(text, position) == 0xE9);
  REQUIRE(UTF8::decodeNext(text, position) == 0x20AC);
  REQUIRE(UTF8::decodeNext(text, position) == UTF8::REPLACEMENT_CHAR);
  REQUIRE(UTF8::decodeNext(text, position) == 'b');
  REQUIRE(position == text.size());
}
