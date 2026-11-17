/************************************************************************/
/**
 * @file chKeyCurveWidgetsUI.h
 * @author AccelMR
 * @date 2026/11/16
 * @brief
 *  Widgets that edit colors and values over time as keys.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chKeyCurve.h"

namespace chEngineSDK {

/**
 * Exists so gradients and curves (a particle's color and size over its life) are edited by
 * dragging keys on a bar or a small graph instead of typing numbers: a double click adds a
 * key, a drag moves it between its neighbors, a right click removes it, and the selected
 * key's values are fields under it. Keys stay sorted by time from 0 to 1.
 */
class KeyCurveWidgetsUI
{
 public:
  /**
   * True when keys changed.
   */
  static bool
  renderGradient(const ANSICHAR* label, Vector<GradientKey>& keys, uint32 maxKeys);

  /**
   * The graph shows at least minimum to maximum and grows to fit the keys; emptyValue is
   * the value without keys. True when keys changed.
   */
  static bool
  renderCurve(const ANSICHAR* label,
              Vector<CurveKey>& keys,
              float minimum,
              float maximum,
              float emptyValue,
              uint32 maxKeys);
};

} // namespace chEngineSDK
