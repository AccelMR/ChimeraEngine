/************************************************************************/
/**
 * @file chNumberFormat.h
 * @author AccelMR
 * @date 2026/11/13
 * @brief Short text for big numbers: suffixes, scientific and engineering notation.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesUtilities.h"

namespace chEngineSDK {

/**
 * How NumberFormat writes a number at or above NumberFormatDesc::plainBelow. Saved by
 * components and scripts, so the values are fixed.
 */
enum class NumberNotation : uint8
{
  // 1234567
  Plain = 0,
  // 1.23M, then 1.23aa past T, 1.23ab...
  Suffix = 1,
  // 1.23e6
  Scientific = 2,
  // 1.23e6, the exponent always a multiple of 3 (12.3e6, 123e6).
  Engineering = 3,
  COUNT
};

struct NumberFormatDesc
{
  NumberNotation notation = NumberNotation::Suffix;
  // Digits after the point of the suffixed or scientific number, up to MAX_DECIMALS.
  int32 decimals = 2;
  // Drops the zeros that end the decimals: 1.50K is written 1.5K, 2.00K is 2K.
  bool bTrimZeros = true;
  // Numbers whose size is below this are written plainly in every notation.
  double plainBelow = 1000.0;
  // Digits after the point when the number is written plainly.
  int32 plainDecimals = 0;
};

/**
 * Exists so games with huge numbers (incrementals, damage numbers) can show them short and
 * without allocating: every function writes into a buffer the caller owns. Plain numbers
 * too long for the buffer are written in scientific notation instead.
 */
class CH_UTILITY_EXPORT NumberFormat
{
 public:
  /**
   * @return The text, which points into buffer. No '\0' is added.
   */
  template<SIZE_T N>
  NODISCARD static StringView
  format(ANSICHAR (&buffer)[N], double value, const NumberFormatDesc& desc = {}) noexcept
  {
    static_assert(N >= MAX_CHARS, "The buffer is too small for a formatted number.");
    return formatTo(buffer, N, value, desc);
  }

  /**
   * Same as format, for buffers whose size is only known at run time; size must be at
   * least MAX_CHARS.
   */
  NODISCARD static StringView
  formatTo(ANSICHAR* buffer, SIZE_T size, double value, const NumberFormatDesc& desc) noexcept;

  /**
   * The suffix of 1000^thousands: K, M, B, T, then aa, ab... az, ba... Empty for 0 and
   * for powers past the last suffix (zz).
   */
  NODISCARD static StringView
  getSuffix(uint32 thousands, ANSICHAR (&buffer)[2]) noexcept;

  static constexpr SIZE_T MAX_CHARS = 48;
  static constexpr int32 MAX_DECIMALS = 9;
  // The highest power of 1000 that has a suffix (zz).
  static constexpr uint32 MAX_SUFFIX_THOUSANDS = 4 + 26 * 26;
};

} // namespace chEngineSDK
