/************************************************************************/
/**
 * @file chNumberFormat.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief Short text for big numbers: suffixes, scientific and engineering notation.
 */
/************************************************************************/
#include "chNumberFormat.h"

#include "chMath.h"

#include <charconv>
// PlatformMath only has float versions, and these numbers need doubles.
#include <cmath>

namespace chEngineSDK {

namespace {
constexpr StringView kSingleSuffixes = "KMBT";
constexpr uint32 kSingleSuffixCount = 4;
constexpr uint32 kLetterCount = 26;

constexpr double kPowersOfTen[NumberFormat::MAX_DECIMALS + 1] = {
    1.0, 10.0, 100.0, 1000.0, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9};
constexpr uint64 kIntegerPowersOfTen[NumberFormat::MAX_DECIMALS + 1] = {
    1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};
// Below 2^53, so the scaled number is an exact integer and the fast path can write it.
constexpr double kMaxExactInteger = 9007199254740992.0;

// Every power of ten a double reaches, from the smallest subnormal to the largest value.
constexpr int32 kLowestExponent = -330;
constexpr int32 kHighestExponent = 310;
constexpr int32 kExponentCount = kHighestExponent - kLowestExponent + 1;

/**
 * pow(10, exponent) for any exponent a double holds, from a table made on first use: the
 * formatting of every number needs two, and pow costs more than the rest of the work.
 */
NODISCARD double
powerOfTen(int32 exponent) noexcept
{
  static const Array<double, kExponentCount> s_powers = [] {
    Array<double, kExponentCount> powers{};
    for (int32 i = 0; i < kExponentCount; ++i) {
      powers[i] = std::pow(10.0, static_cast<double>(i + kLowestExponent));
    }
    return powers;
  }();
  return s_powers[Math::clamp(exponent, kLowestExponent, kHighestExponent) - kLowestExponent];
}

/**
 * floor(log10(value)) for a positive finite value, from its binary exponent and one compare.
 */
NODISCARD int32
decimalExponent(double value) noexcept
{
  int32 binaryExponent = 0;
  CH_PARAMETER_UNUSED(std::frexp(value, &binaryExponent));
  // value is in [2^(e-1), 2^e), so the guess is the right exponent or one too low.
  int32 exponent =
      static_cast<int32>(std::floor(static_cast<double>(binaryExponent - 1) * 0.30102999566398));
  if (value >= powerOfTen(exponent + 1)) {
    ++exponent;
  }
  return exponent;
}

NODISCARD double
roundTo(double value, int32 decimals) noexcept
{
  const double scale = kPowersOfTen[decimals];
  return std::round(value * scale) / scale;
}

/**
 * Writes value with decimals digits after the point. Returns the end of the text, or null
 * when it does not fit.
 */
NODISCARD ANSICHAR*
writeFixed(ANSICHAR* begin, ANSICHAR* end, double value, int32 decimals, bool bTrimZeros)
{
  ANSICHAR* last = nullptr;
  const double scaled = std::round(value * kPowersOfTen[decimals]);
  if (scaled < kMaxExactInteger) {
    // Integer digits are much cheaper to write than a double with a precision.
    const uint64 number = static_cast<uint64>(scaled);
    const uint64 divisor = kIntegerPowersOfTen[decimals];
    const std::to_chars_result whole = std::to_chars(begin, end, number / divisor);
    if (whole.ec != std::errc() || end - whole.ptr < decimals + 1) {
      return nullptr;
    }
    last = whole.ptr;
    if (decimals > 0) {
      *last++ = '.';
      uint64 fraction = number % divisor;
      for (int32 digit = decimals - 1; digit >= 0; --digit) {
        last[digit] = static_cast<ANSICHAR>('0' + fraction % 10);
        fraction /= 10;
      }
      last += decimals;
    }
  }
  else {
    const std::to_chars_result result =
        std::to_chars(begin, end, value, std::chars_format::fixed, decimals);
    if (result.ec != std::errc()) {
      return nullptr;
    }
    last = result.ptr;
  }
  if (bTrimZeros && decimals > 0) {
    while (last > begin && last[-1] == '0') {
      --last;
    }
    if (last > begin && last[-1] == '.') {
      --last;
    }
  }
  return last;
}

NODISCARD StringView
copyText(ANSICHAR* buffer, StringView text) noexcept
{
  for (SIZE_T i = 0; i < text.size(); ++i) {
    buffer[i] = text[i];
  }
  return StringView(buffer, text.size());
}

NODISCARD int32
floorDivide(int32 value, int32 divisor) noexcept
{
  const int32 quotient = value / divisor;
  return (value % divisor != 0 && value < 0) ? quotient - 1 : quotient;
}

/**
 * Scientific notation with an exponent that is a multiple of step (1, or 3 for
 * engineering).
 */
NODISCARD ANSICHAR*
writeExponent(ANSICHAR* out, ANSICHAR* end, double value, int32 step, int32 decimals, bool bTrim)
{
  int32 exponent = floorDivide(decimalExponent(value), step) * step;
  const double limit = kPowersOfTen[step];
  double mantissa = value / powerOfTen(exponent);
  // Division rounding can leave it one step off near exact powers of ten.
  if (mantissa >= limit) {
    exponent += step;
    mantissa /= limit;
  }
  else if (mantissa < 1.0) {
    exponent -= step;
    mantissa *= limit;
  }
  mantissa = roundTo(mantissa, decimals);
  if (mantissa >= limit) {
    exponent += step;
    mantissa = roundTo(mantissa / limit, decimals);
  }

  out = writeFixed(out, end, mantissa, decimals, bTrim);
  if (out == nullptr || out == end) {
    return nullptr;
  }
  *out++ = 'e';
  const std::to_chars_result result = std::to_chars(out, end, exponent);
  return result.ec == std::errc() ? result.ptr : nullptr;
}

/**
 * Returns null when the number is past the last suffix.
 */
NODISCARD ANSICHAR*
writeSuffixed(ANSICHAR* out, ANSICHAR* end, double value, int32 decimals, bool bTrim)
{
  // At least K: a number that is not written plainly always gets a suffix, so 999.6 is 1K.
  int32 thousands = Math::max(floorDivide(decimalExponent(value), 3), 1);
  double mantissa = value / powerOfTen(thousands * 3);
  if (mantissa >= 1000.0) {
    ++thousands;
    mantissa /= 1000.0;
  }
  else if (mantissa < 1.0 && thousands > 1) {
    --thousands;
    mantissa *= 1000.0;
  }
  mantissa = roundTo(mantissa, decimals);
  if (mantissa >= 1000.0) {
    ++thousands;
    mantissa = roundTo(mantissa / 1000.0, decimals);
  }
  if (static_cast<uint32>(thousands) > NumberFormat::MAX_SUFFIX_THOUSANDS) {
    return nullptr;
  }

  out = writeFixed(out, end, mantissa, decimals, bTrim);
  if (out == nullptr) {
    return nullptr;
  }
  ANSICHAR suffixBuffer[2];
  const StringView suffix =
      NumberFormat::getSuffix(static_cast<uint32>(thousands), suffixBuffer);
  if (static_cast<SIZE_T>(end - out) < suffix.size()) {
    return nullptr;
  }
  for (const ANSICHAR letter : suffix) {
    *out++ = letter;
  }
  return out;
}
} // namespace

/*
 */
StringView
NumberFormat::formatTo(ANSICHAR* buffer,
                       SIZE_T size,
                       double value,
                       const NumberFormatDesc& desc) noexcept
{
  if (buffer == nullptr || size < MAX_CHARS) {
    return {};
  }
  if (std::isnan(value)) {
    return copyText(buffer, "NaN");
  }
  if (std::isinf(value)) {
    return copyText(buffer, value < 0.0 ? StringView("-Inf") : StringView("Inf"));
  }

  ANSICHAR* const end = buffer + size;
  ANSICHAR* out = buffer;
  if (value < 0.0) {
    *out++ = '-';
    value = -value;
  }
  const int32 decimals = Math::clamp(desc.decimals, 0, MAX_DECIMALS);
  const int32 plainDecimals = Math::clamp(desc.plainDecimals, 0, MAX_DECIMALS);

  // A number just under plainBelow can round up to it, which belongs to the notation.
  const bool bPlain = desc.notation == NumberNotation::Plain || value == 0.0 ||
                      roundTo(value, plainDecimals) < desc.plainBelow;
  if (bPlain) {
    ANSICHAR* const last = writeFixed(out, end, value, plainDecimals, desc.bTrimZeros);
    if (last != nullptr) {
      return StringView(buffer, static_cast<SIZE_T>(last - buffer));
    }
  }

  ANSICHAR* last = nullptr;
  switch (desc.notation) {
    case NumberNotation::Suffix:
      last = writeSuffixed(out, end, value, decimals, desc.bTrimZeros);
      break;
    case NumberNotation::Engineering:
      last = writeExponent(out, end, value, 3, decimals, desc.bTrimZeros);
      break;
    default:
      break;
  }
  // Scientific, and what did not fit any other way.
  if (last == nullptr) {
    last = writeExponent(out, end, value, 1, decimals, desc.bTrimZeros);
  }
  if (last == nullptr) {
    return copyText(buffer, "?");
  }
  return StringView(buffer, static_cast<SIZE_T>(last - buffer));
}

/*
 */
StringView
NumberFormat::getSuffix(uint32 thousands, ANSICHAR (&buffer)[2]) noexcept
{
  if (thousands == 0 || thousands > MAX_SUFFIX_THOUSANDS) {
    return {};
  }
  if (thousands <= kSingleSuffixCount) {
    buffer[0] = kSingleSuffixes[thousands - 1];
    return StringView(buffer, 1);
  }
  const uint32 index = thousands - kSingleSuffixCount - 1;
  buffer[0] = static_cast<ANSICHAR>('a' + index / kLetterCount);
  buffer[1] = static_cast<ANSICHAR>('a' + index % kLetterCount);
  return StringView(buffer, 2);
}

} // namespace chEngineSDK
