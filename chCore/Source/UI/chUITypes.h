/************************************************************************/
/**
 * @file chUITypes.h
 * @author Swampertor
 * @date 2026/11/19
 * @brief Visibility, focus navigation and render transform of UI widgets.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chVector2.h"

namespace chEngineSDK {
class UIWidget;

/**
 * Whether a widget is drawn and hit-tested, as UMG's ESlateVisibility. Saved in UI
 * documents, so the values are fixed.
 */
enum class UIVisibility : uint8
{
  // Drawn and hit-tested; a hit blocks widgets drawn below.
  Visible = 0,
  // Not drawn, takes no layout space and is not hit-tested.
  Collapsed = 1,
  // Not drawn and not hit-tested, but still takes layout space.
  Hidden = 2,
  // Drawn, but neither it nor its children are hit-tested.
  HitTestInvisible = 3,
  // Drawn; it is not hit-tested, but its children are.
  SelfHitTestInvisible = 4,
  COUNT
};

/**
 * Direction a slider or a scroll box flows in. Saved in UI documents, so the values are
 * fixed.
 */
enum class UIOrientation : uint8
{
  Horizontal = 0,
  Vertical = 1,
  COUNT
};

/**
 * How a box slot sizes its child, as UMG's ESlateSizeRule. Auto asks the child for its
 * desired size; Fill gives it a share of the space the other children leave.
 */
enum class UISizeRule : uint8
{
  Auto = 0,
  Fill = 1,
  COUNT
};

/**
 * Where a child sits across an overlay, border or size box, as UMG's EHorizontalAlignment.
 * Fill takes the whole space; the others use the child's desired size.
 */
enum class UIHorizontalAlignment : uint8
{
  Left = 0,
  Center = 1,
  Right = 2,
  Fill = 3,
  COUNT
};

/**
 * The vertical counterpart of UIHorizontalAlignment.
 */
enum class UIVerticalAlignment : uint8
{
  Top = 0,
  Center = 1,
  Bottom = 2,
  Fill = 3,
  COUNT
};

/**
 * One direction focus can move in. Saved in UI documents, so the values are fixed.
 */
enum class UINavigation : uint8
{
  Up = 0,
  Down = 1,
  Left = 2,
  Right = 3,
  Next = 4,
  Previous = 5,
  COUNT
};

/**
 * How focus moves in one direction, as UMG's EUINavigationRule. Saved in UI documents, so
 * the values are fixed.
 */
enum class UINavigationRule : uint8
{
  // Find the nearest focusable widget in that direction.
  Escape = 0,
  // Go to the widget named in the link.
  Explicit = 1,
  // Find the nearest, wrapping to the opposite edge.
  Wrap = 2,
  // Stay on the current widget.
  Stop = 3,
  // Ask the link's callback.
  Custom = 4,
  COUNT
};

/**
 * Where one direction of focus goes. target is used by Explicit; custom by Custom.
 */
struct UINavigationLink
{
  UINavigationRule rule = UINavigationRule::Escape;
  UIWidget* target = nullptr;
  Function<UIWidget*()> custom;
};

/**
 * UMG's FWidgetTransform: a translation, rotation and scale around a pivot, applied when
 * the widget is painted and hit-tested. Saved in UI documents.
 */
struct UIRenderTransform
{
  Vector2 translation = Vector2::ZERO;
  // Counterclockwise, in radians.
  float angle = 0.0f;
  Vector2 scale{1.0f, 1.0f};
  // Fraction of the widget's size, (0, 0) top-left and (1, 1) bottom-right.
  Vector2 pivot{0.5f, 0.5f};
};

NODISCARD FORCEINLINE const ANSICHAR*
getUIVisibilityName(UIVisibility visibility) noexcept
{
  switch (visibility) {
    case UIVisibility::Visible: return "Visible";
    case UIVisibility::Collapsed: return "Collapsed";
    case UIVisibility::Hidden: return "Hidden";
    case UIVisibility::HitTestInvisible: return "HitTestInvisible";
    case UIVisibility::SelfHitTestInvisible: return "SelfHitTestInvisible";
    case UIVisibility::COUNT: break;
  }
  return "Unknown";
}

NODISCARD FORCEINLINE const ANSICHAR*
getUIOrientationName(UIOrientation orientation) noexcept
{
  switch (orientation) {
    case UIOrientation::Horizontal: return "Horizontal";
    case UIOrientation::Vertical: return "Vertical";
    case UIOrientation::COUNT: break;
  }
  return "Unknown";
}

NODISCARD FORCEINLINE const ANSICHAR*
getUISizeRuleName(UISizeRule rule) noexcept
{
  switch (rule) {
    case UISizeRule::Auto: return "Auto";
    case UISizeRule::Fill: return "Fill";
    case UISizeRule::COUNT: break;
  }
  return "Unknown";
}

NODISCARD FORCEINLINE const ANSICHAR*
getUIHorizontalAlignmentName(UIHorizontalAlignment alignment) noexcept
{
  switch (alignment) {
    case UIHorizontalAlignment::Left: return "Left";
    case UIHorizontalAlignment::Center: return "Center";
    case UIHorizontalAlignment::Right: return "Right";
    case UIHorizontalAlignment::Fill: return "Fill";
    case UIHorizontalAlignment::COUNT: break;
  }
  return "Unknown";
}

NODISCARD FORCEINLINE const ANSICHAR*
getUIVerticalAlignmentName(UIVerticalAlignment alignment) noexcept
{
  switch (alignment) {
    case UIVerticalAlignment::Top: return "Top";
    case UIVerticalAlignment::Center: return "Center";
    case UIVerticalAlignment::Bottom: return "Bottom";
    case UIVerticalAlignment::Fill: return "Fill";
    case UIVerticalAlignment::COUNT: break;
  }
  return "Unknown";
}

NODISCARD FORCEINLINE const ANSICHAR*
getUINavigationName(UINavigation direction) noexcept
{
  switch (direction) {
    case UINavigation::Up: return "Up";
    case UINavigation::Down: return "Down";
    case UINavigation::Left: return "Left";
    case UINavigation::Right: return "Right";
    case UINavigation::Next: return "Next";
    case UINavigation::Previous: return "Previous";
    case UINavigation::COUNT: break;
  }
  return "Unknown";
}

NODISCARD FORCEINLINE const ANSICHAR*
getUINavigationRuleName(UINavigationRule rule) noexcept
{
  switch (rule) {
    case UINavigationRule::Escape: return "Escape";
    case UINavigationRule::Explicit: return "Explicit";
    case UINavigationRule::Wrap: return "Wrap";
    case UINavigationRule::Stop: return "Stop";
    case UINavigationRule::Custom: return "Custom";
    case UINavigationRule::COUNT: break;
  }
  return "Unknown";
}

} // namespace chEngineSDK
