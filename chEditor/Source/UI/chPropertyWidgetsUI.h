/************************************************************************/
/**
 * @file chPropertyWidgetsUI.h
 * @author AccelMR
 * @date 2026/11/13
 * @brief The Inspector widget of each property type.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chComponentEditor.h"
#include "chScriptProperty.h"

namespace chEngineSDK {
class GameObject;

/**
 * How one property row looks besides its value.
 */
struct PropertyWidgetDesc {
  const ANSICHAR* label = "";
  // Numbers with minimum < maximum show a slider.
  double minimum = 0.0;
  double maximum = 0.0;
  ComponentPropertyWidget widget = ComponentPropertyWidget::Value;
  // The names of Enum and Flags.
  Span<const String> names{};
};

/**
 * Exists so script properties and the properties of plugin components are edited with the
 * same widgets: drags and sliders for numbers, pickers for colors, slots that take objects
 * from the Scene Graph and assets from the Content Browser, combos for enums and checkboxes
 * for flags.
 */
class PropertyWidgetsUI
{
 public:
  /**
   * Edits value, whose type picks the widget; owner is the object the property belongs to
   * (object slots look in its scene). True when the value changed.
   */
  static bool
  render(const PropertyWidgetDesc& desc, ScriptPropertyValue& value, const GameObject& owner);
};

} // namespace chEngineSDK
