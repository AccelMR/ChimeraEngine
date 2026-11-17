/************************************************************************/
/**
 * @file chKeyCurveWidgetsUI.cpp
 * @author AccelMR
 * @date 2026/11/16
 * @brief
 *  Widgets that edit colors and values over time as keys.
 */
/************************************************************************/
#include "chKeyCurveWidgetsUI.h"

#include "chMath.h"
#include "chStringUtils.h"

#include "imgui.h"

#include "../../Content/Fonts/IconsFontAwsome5.h"

namespace chEngineSDK {
namespace {
// How close, in pixels, the mouse must be to pick a key.
constexpr float kPickDistance = 7.0f;
constexpr float kCurveHeight = 90.0f;
constexpr float kCheckerSize = 6.0f;
constexpr const ANSICHAR* kHelp = "Double click to add a key, drag a key to move it, right "
                                  "click a key to remove it";

/*
 * The selected key of the widget with id, kept in ImGui's storage; -1 for none.
 */
int32
getSelected(const ImGuiID id)
{
  return ImGui::GetStateStorage()->GetInt(id, -1);
}

void
setSelected(const ImGuiID id, const int32 index)
{
  ImGui::GetStateStorage()->SetInt(id, index);
}

ImU32
toColor(const LinearColor& color)
{
  return ImGui::ColorConvertFloat4ToU32(ImVec4(color.r, color.g, color.b, color.a));
}

/*
 * The times a key may move between: its neighbors'.
 */
template <typename Key>
void
getTimeLimits(const Vector<Key>& keys, const SIZE_T index, float& outMinimum,
              float& outMaximum)
{
  outMinimum = index > 0 ? keys[index - 1].time : 0.0f;
  outMaximum = index + 1 < keys.size() ? keys[index + 1].time : 1.0f;
}

/*
 * Where a key at time goes so the list stays sorted.
 */
template <typename Key>
SIZE_T
findInsertIndex(const Vector<Key>& keys, const float time)
{
  SIZE_T index = 0;
  while (index < keys.size() && keys[index].time <= time) {
    ++index;
  }
  return index;
}

/*
 * The key under the mouse along x (and y for curves when given), or -1.
 */
template <typename Key, typename Position>
int32
pickKey(const Vector<Key>& keys, const ImVec2& mouse, Position&& getPosition)
{
  int32 picked = -1;
  float best = kPickDistance * kPickDistance;
  for (SIZE_T i = 0; i < keys.size(); ++i) {
    const ImVec2 point = getPosition(keys[i]);
    const float dx = point.x - mouse.x;
    const float dy = point.y - mouse.y;
    const float distance = dx * dx + dy * dy;
    if (distance <= best) {
      best = distance;
      picked = static_cast<int32>(i);
    }
  }
  return picked;
}

/*
 * Delete Key and the time field of the selected key. True when keys changed.
 */
template <typename Key>
bool
renderSelectedTime(Vector<Key>& keys, const ImGuiID id, int32& selected)
{
  bool bChanged = false;
  float minimum = 0.0f;
  float maximum = 1.0f;
  getTimeLimits(keys, static_cast<SIZE_T>(selected), minimum, maximum);
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
  bChanged |= ImGui::DragFloat("Time", &keys[selected].time, 0.002f, minimum, maximum,
                               "%.3f", ImGuiSliderFlags_AlwaysClamp);
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_FA_TRASH "  Remove Key")) {
    keys.erase(keys.begin() + selected);
    selected = -1;
    setSelected(id, selected);
    bChanged = true;
  }
  return bChanged;
}
} // namespace

/*
 */
bool
KeyCurveWidgetsUI::renderGradient(const ANSICHAR* label, Vector<GradientKey>& keys,
                                  const uint32 maxKeys)
{
  ImGui::PushID(label);
  const ImGuiID id = ImGui::GetID("selected");
  int32 selected = getSelected(id);
  if (selected >= static_cast<int32>(keys.size())) {
    selected = -1;
  }
  bool bChanged = false;

  ImGui::TextUnformatted(label);
  const float width = Math::max(ImGui::GetContentRegionAvail().x, 10.0f);
  const float barHeight = ImGui::GetFrameHeight();
  const float markerHeight = barHeight * 0.6f;
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const ImVec2 barEnd(origin.x + width, origin.y + barHeight);
  ImDrawList* drawList = ImGui::GetWindowDrawList();

  // A checker under the colors shows their transparency.
  const ImU32 light = IM_COL32(200, 200, 200, 255);
  const ImU32 dark = IM_COL32(120, 120, 120, 255);
  for (float y = origin.y; y < barEnd.y; y += kCheckerSize) {
    for (float x = origin.x; x < barEnd.x; x += kCheckerSize) {
      const bool bLight =
          (static_cast<int32>((x - origin.x) / kCheckerSize) +
           static_cast<int32>((y - origin.y) / kCheckerSize)) % 2 == 0;
      drawList->AddRectFilled(ImVec2(x, y),
                              ImVec2(Math::min(x + kCheckerSize, barEnd.x),
                                     Math::min(y + kCheckerSize, barEnd.y)),
                              bLight ? light : dark);
    }
  }
  const auto timeToX = [&origin, width](const float time) { return origin.x + time * width; };
  if (keys.empty()) {
    drawList->AddRectFilled(origin, barEnd, toColor(LinearColor::WHITE));
  }
  else {
    drawList->AddRectFilled(origin, ImVec2(timeToX(keys.front().time), barEnd.y),
                            toColor(keys.front().color));
    for (SIZE_T i = 1; i < keys.size(); ++i) {
      const ImU32 left = toColor(keys[i - 1].color);
      const ImU32 right = toColor(keys[i].color);
      drawList->AddRectFilledMultiColor(ImVec2(timeToX(keys[i - 1].time), origin.y),
                                        ImVec2(timeToX(keys[i].time), barEnd.y), left, right,
                                        right, left);
    }
    drawList->AddRectFilled(ImVec2(timeToX(keys.back().time), origin.y), barEnd,
                            toColor(keys.back().color));
  }
  drawList->AddRect(origin, barEnd, ImGui::GetColorU32(ImGuiCol_Border));

  // Markers under the bar.
  const ImU32 markerColor = ImGui::GetColorU32(ImGuiCol_Text);
  const ImU32 selectedColor = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
  for (SIZE_T i = 0; i < keys.size(); ++i) {
    const float x = timeToX(keys[i].time);
    const ImU32 color = static_cast<int32>(i) == selected ? selectedColor : markerColor;
    drawList->AddTriangleFilled(ImVec2(x, barEnd.y), ImVec2(x - markerHeight * 0.6f,
                                                            barEnd.y + markerHeight),
                                ImVec2(x + markerHeight * 0.6f, barEnd.y + markerHeight),
                                color);
  }

  ImGui::InvisibleButton("bar", ImVec2(width, barHeight + markerHeight),
                         ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
  ImGui::SetItemTooltip("%s", kHelp);
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  const float markerY = barEnd.y + markerHeight * 0.5f;
  const auto keyPosition = [&timeToX, markerY](const GradientKey& key) {
    return ImVec2(timeToX(key.time), markerY);
  };
  const auto pickAlongX = [&keys, &mouse, &keyPosition, markerY]() {
    // Picked by x only, so a key is easy to grab anywhere on the bar's height.
    return pickKey(keys, ImVec2(mouse.x, markerY), keyPosition);
  };
  const float mouseTime = Math::clamp((mouse.x - origin.x) / width, 0.0f, 1.0f);

  if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    if (pickAlongX() < 0 && keys.size() < maxKeys) {
      const SIZE_T index = findInsertIndex(keys, mouseTime);
      keys.insert(keys.begin() + static_cast<std::ptrdiff_t>(index),
                  GradientKey{.time = mouseTime, .color = KeyCurve::evaluate(keys, mouseTime)});
      selected = static_cast<int32>(index);
      bChanged = true;
    }
  }
  else if (ImGui::IsItemActivated() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    selected = pickAlongX();
  }
  else if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    const int32 picked = pickAlongX();
    if (picked >= 0) {
      keys.erase(keys.begin() + picked);
      selected = -1;
      bChanged = true;
    }
  }
  if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0f)
      && selected >= 0) {
    float minimum = 0.0f;
    float maximum = 1.0f;
    getTimeLimits(keys, static_cast<SIZE_T>(selected), minimum, maximum);
    const float time = Math::clamp(mouseTime, minimum, maximum);
    if (time != keys[selected].time) {
      keys[selected].time = time;
      bChanged = true;
    }
  }
  setSelected(id, selected);

  if (selected >= 0) {
    bChanged |= ImGui::ColorEdit4("Color", keys[selected].color.getData(),
                                  ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar);
    bChanged |= renderSelectedTime(keys, id, selected);
  }
  else if (keys.empty()) {
    ImGui::TextDisabled("White all its life; double click the bar to add a key");
  }
  ImGui::PopID();
  return bChanged;
}

/*
 */
bool
KeyCurveWidgetsUI::renderCurve(const ANSICHAR* label, Vector<CurveKey>& keys,
                               const float minimum, const float maximum,
                               const float emptyValue, const uint32 maxKeys)
{
  ImGui::PushID(label);
  const ImGuiID id = ImGui::GetID("selected");
  const ImGuiID rangeLowId = ImGui::GetID("low");
  const ImGuiID rangeHighId = ImGui::GetID("high");
  int32 selected = getSelected(id);
  if (selected >= static_cast<int32>(keys.size())) {
    selected = -1;
  }
  bool bChanged = false;

  ImGui::TextUnformatted(label);
  const float width = Math::max(ImGui::GetContentRegionAvail().x, 10.0f);
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const ImVec2 end(origin.x + width, origin.y + kCurveHeight);
  ImGui::InvisibleButton("graph", ImVec2(width, kCurveHeight),
                         ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
  ImGui::SetItemTooltip("%s", kHelp);
  const bool bDragging = ImGui::IsItemActive();

  // The range fits the keys, but stays as it was while a key is dragged, so the graph does
  // not move under the mouse.
  ImGuiStorage* storage = ImGui::GetStateStorage();
  float low = minimum;
  float high = maximum;
  if (bDragging && !ImGui::IsItemActivated()) {
    low = storage->GetFloat(rangeLowId, minimum);
    high = storage->GetFloat(rangeHighId, maximum);
  }
  else {
    for (const CurveKey& key : keys) {
      low = Math::min(low, key.value);
      high = Math::max(high, key.value);
    }
    if (high - low < 1e-4f) {
      high = low + 1.0f;
    }
    storage->SetFloat(rangeLowId, low);
    storage->SetFloat(rangeHighId, high);
  }

  const auto toScreen = [&origin, width, low, high](const float time, const float value) {
    return ImVec2(origin.x + time * width,
                  origin.y + (1.0f - (value - low) / (high - low)) * kCurveHeight);
  };
  const auto keyPosition = [&toScreen](const CurveKey& key) {
    return toScreen(key.time, key.value);
  };

  ImDrawList* drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(origin, end, ImGui::GetColorU32(ImGuiCol_FrameBg));
  const ImU32 gridColor = ImGui::GetColorU32(ImGuiCol_Border);
  for (uint32 i = 1; i < 4; ++i) {
    const float x = origin.x + width * static_cast<float>(i) * 0.25f;
    drawList->AddLine(ImVec2(x, origin.y), ImVec2(x, end.y), gridColor);
  }
  if (low < 0.0f && high > 0.0f) {
    const float zeroY = toScreen(0.0f, 0.0f).y;
    drawList->AddLine(ImVec2(origin.x, zeroY), ImVec2(end.x, zeroY), gridColor, 2.0f);
  }
  drawList->AddRect(origin, end, gridColor);

  const ImU32 lineColor = ImGui::GetColorU32(ImGuiCol_PlotLines);
  if (keys.empty()) {
    const float y = toScreen(0.0f, emptyValue).y;
    drawList->AddLine(ImVec2(origin.x, y), ImVec2(end.x, y), lineColor, 2.0f);
  }
  else {
    drawList->AddLine(toScreen(0.0f, keys.front().value), keyPosition(keys.front()),
                      lineColor, 2.0f);
    for (SIZE_T i = 1; i < keys.size(); ++i) {
      drawList->AddLine(keyPosition(keys[i - 1]), keyPosition(keys[i]), lineColor, 2.0f);
    }
    drawList->AddLine(keyPosition(keys.back()), toScreen(1.0f, keys.back().value), lineColor,
                      2.0f);
  }
  const ImU32 pointColor = ImGui::GetColorU32(ImGuiCol_Text);
  const ImU32 selectedColor = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
  for (SIZE_T i = 0; i < keys.size(); ++i) {
    drawList->AddCircleFilled(keyPosition(keys[i]), 4.5f,
                              static_cast<int32>(i) == selected ? selectedColor : pointColor);
  }

  // The ends of the range, written without allocating.
  ANSICHAR text[StringUtils::MAX_FLOAT_CHARS];
  const ImU32 textColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);
  StringView number = StringUtils::toChars(text, high);
  drawList->AddText(ImVec2(origin.x + 3.0f, origin.y + 1.0f), textColor, number.data(),
                    number.data() + number.size());
  number = StringUtils::toChars(text, low);
  drawList->AddText(ImVec2(origin.x + 3.0f, end.y - ImGui::GetTextLineHeight() - 1.0f),
                    textColor, number.data(), number.data() + number.size());

  const ImVec2 mouse = ImGui::GetIO().MousePos;
  const float mouseTime = Math::clamp((mouse.x - origin.x) / width, 0.0f, 1.0f);
  const float mouseValue =
      Math::clamp(low + (1.0f - (mouse.y - origin.y) / kCurveHeight) * (high - low), low, high);

  if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    if (pickKey(keys, mouse, keyPosition) < 0 && keys.size() < maxKeys) {
      const SIZE_T index = findInsertIndex(keys, mouseTime);
      keys.insert(keys.begin() + static_cast<std::ptrdiff_t>(index),
                  CurveKey{.time = mouseTime, .value = mouseValue});
      selected = static_cast<int32>(index);
      bChanged = true;
    }
  }
  else if (ImGui::IsItemActivated() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    selected = pickKey(keys, mouse, keyPosition);
  }
  else if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    const int32 picked = pickKey(keys, mouse, keyPosition);
    if (picked >= 0) {
      keys.erase(keys.begin() + picked);
      selected = -1;
      bChanged = true;
    }
  }
  if (bDragging && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0f) && selected >= 0) {
    float timeMinimum = 0.0f;
    float timeMaximum = 1.0f;
    getTimeLimits(keys, static_cast<SIZE_T>(selected), timeMinimum, timeMaximum);
    CurveKey& key = keys[selected];
    const float time = Math::clamp(mouseTime, timeMinimum, timeMaximum);
    if (time != key.time || mouseValue != key.value) {
      key.time = time;
      key.value = mouseValue;
      bChanged = true;
    }
  }
  setSelected(id, selected);

  if (selected >= 0) {
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
    bChanged |= ImGui::DragFloat("Value", &keys[selected].value, 0.01f);
    bChanged |= renderSelectedTime(keys, id, selected);
  }
  ImGui::PopID();
  return bChanged;
}

} // namespace chEngineSDK
