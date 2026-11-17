/************************************************************************/
/**
 * @file chKeyCurve.h
 * @author AccelMR
 * @date 2026/11/16
 * @brief
 *  Values and colors given at a few times, joined by straight lines.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesUtilities.h"

#include "chLinearColor.h"

namespace chEngineSDK {

struct CurveKey
{
  float time = 0.0f;
  float value = 0.0f;
};

struct GradientKey
{
  float time = 0.0f;
  LinearColor color = LinearColor::WHITE;
};

/**
 * Exists so a value or a color that changes over time (a particle's size over its life)
 * is a short list of keys sorted by time, read the same way by code, shaders and the
 * editor: before the first key it is the first key, after the last the last, and between
 * two keys a straight line.
 */
class KeyCurve
{
 public:
  /**
   * emptyValue without keys.
   */
  NODISCARD static float
  evaluate(Span<const CurveKey> keys, float time, float emptyValue) noexcept
  {
    if (keys.empty()) {
      return emptyValue;
    }
    if (time <= keys.front().time) {
      return keys.front().value;
    }
    for (SIZE_T i = 1; i < keys.size(); ++i) {
      if (time <= keys[i].time) {
        const CurveKey& before = keys[i - 1];
        const float span = keys[i].time - before.time;
        const float weight = span > 0.0f ? (time - before.time) / span : 1.0f;
        return before.value + (keys[i].value - before.value) * weight;
      }
    }
    return keys.back().value;
  }

  /**
   * White without keys.
   */
  NODISCARD static LinearColor
  evaluate(Span<const GradientKey> keys, float time) noexcept
  {
    if (keys.empty()) {
      return LinearColor::WHITE;
    }
    if (time <= keys.front().time) {
      return keys.front().color;
    }
    for (SIZE_T i = 1; i < keys.size(); ++i) {
      if (time <= keys[i].time) {
        const GradientKey& before = keys[i - 1];
        const float span = keys[i].time - before.time;
        const float weight = span > 0.0f ? (time - before.time) / span : 1.0f;
        return before.color + (keys[i].color - before.color) * weight;
      }
    }
    return keys.back().color;
  }
};

} // namespace chEngineSDK
