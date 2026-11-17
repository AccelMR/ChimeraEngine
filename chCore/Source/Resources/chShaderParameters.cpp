/************************************************************************/
/**
 * @file chShaderParameters.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Values a project shader declares for the editor and scripts to set.
 */
/************************************************************************/
#include "chShaderParameters.h"

#include <charconv>
#include <cstring>

#include "chMath.h"
#include "chStringUtils.h"

namespace chEngineSDK {

namespace {
struct TypeEntry
{
  StringView name;
  uint32 componentCount;
};

// In the order of ShaderParameterType.
constexpr TypeEntry kTypes[] = {
    {"float", 1}, {"float2", 2}, {"float3", 3}, {"float4", 4},
    {"color", 4}, {"int", 1},    {"bool", 1},
};
static_assert(sizeof(kTypes) / sizeof(kTypes[0]) ==
              static_cast<SIZE_T>(ShaderParameterType::COUNT));

// In the order of PostProcessStage.
constexpr StringView kStageNames[] = {"AfterTonemap", "BeforeTonemap"};
static_assert(sizeof(kStageNames) / sizeof(kStageNames[0]) ==
              static_cast<SIZE_T>(PostProcessStage::COUNT));

#if USING(CH_EDITOR)
constexpr StringView kParamKeyword = "CH_PARAM";
constexpr StringView kStageKeyword = "CH_STAGE";

/**
 * Splits a declaration into words; a word in double quotes keeps its spaces.
 */
NODISCARD bool
splitWords(StringView text, Vector<StringView>& outWords)
{
  outWords.clear();
  SIZE_T position = 0;
  while (position < text.size()) {
    while (position < text.size() && (text[position] == ' ' || text[position] == '\t')) {
      ++position;
    }
    if (position >= text.size()) {
      break;
    }
    if (text[position] == '"') {
      const SIZE_T close = text.find('"', position + 1);
      if (close == StringView::npos) {
        return false;
      }
      outWords.push_back(text.substr(position, close - position + 1));
      position = close + 1;
      continue;
    }
    const SIZE_T start = position;
    while (position < text.size() && text[position] != ' ' && text[position] != '\t') {
      ++position;
    }
    outWords.push_back(text.substr(start, position - start));
  }
  return true;
}

NODISCARD bool
parseFloat(StringView text, float& outValue)
{
  const ANSICHAR* end = text.data() + text.size();
  const std::from_chars_result result = std::from_chars(text.data(), end, outValue);
  return result.ec == std::errc() && result.ptr == end && Math::isFinite(outValue);
}

NODISCARD bool
isIdentifier(StringView name)
{
  if (name.empty() || name.size() > ShaderParameters::MAX_NAME_LENGTH) {
    return false;
  }
  for (SIZE_T i = 0; i < name.size(); ++i) {
    const ANSICHAR c = name[i];
    const bool bLetter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    const bool bDigit = c >= '0' && c <= '9';
    if (!bLetter && !(bDigit && i > 0)) {
      return false;
    }
  }
  // Names starting with an underscore are left for the padding of the generated struct.
  return name.front() != '_';
}

/**
 * The text after "//" when the line is a comment that starts with keyword, else empty.
 */
NODISCARD StringView
getDeclaration(StringView line, StringView keyword)
{
  line = StringUtils::trimView(line);
  if (line.size() < 2 || line[0] != '/' || line[1] != '/') {
    return StringView();
  }
  line = StringUtils::trimView(line.substr(2));
  if (line.size() <= keyword.size() || line.substr(0, keyword.size()) != keyword) {
    return StringView();
  }
  const ANSICHAR next = line[keyword.size()];
  if (next != ' ' && next != '\t') {
    return StringView();
  }
  return StringUtils::trimView(line.substr(keyword.size()));
}

NODISCARD bool
parseParameter(Span<const StringView> words, ShaderParameter& outParameter, String& outError)
{
  if (words.size() < 2) {
    outError = "CH_PARAM needs a type and a name.";
    return false;
  }
  if (!ShaderParameters::parseTypeName(words[0], outParameter.type)) {
    outError = StringUtils::format("'{0}' is not a parameter type (float, float2, float3, "
                                   "float4, color, int, bool).",
                                   words[0]);
    return false;
  }
  if (!isIdentifier(words[1])) {
    outError = StringUtils::format("'{0}' is not a valid parameter name.", words[1]);
    return false;
  }
  outParameter.name = String(words[1]);

  Vector<float> numbers;
  for (SIZE_T i = 2; i < words.size(); ++i) {
    const StringView word = words[i];
    if (word.front() == '"') {
      if (i + 1 != words.size()) {
        outError = "The tooltip must be the last thing on the line.";
        return false;
      }
      const StringView tooltip = word.substr(1, word.size() - 2);
      if (tooltip.size() > ShaderParameters::MAX_TOOLTIP_LENGTH) {
        outError = "The tooltip is too long.";
        return false;
      }
      outParameter.tooltip = String(tooltip);
      break;
    }
    float value = 0.0f;
    if (!parseFloat(word, value)) {
      outError = StringUtils::format("'{0}' is not a number.", word);
      return false;
    }
    numbers.push_back(value);
  }

  const ShaderParameterType type = outParameter.type;
  const uint32 components = ShaderParameters::getComponentCount(type);
  const bool bScalar = type == ShaderParameterType::Float || type == ShaderParameterType::Int;
  outParameter.defaultValue =
      type == ShaderParameterType::Color ? Vector4(1.0f, 1.0f, 1.0f, 1.0f) : Vector4::ZERO;

  SIZE_T defaults = Math::min(numbers.size(), static_cast<SIZE_T>(components));
  if (type == ShaderParameterType::Color && numbers.size() == 3) {
    defaults = 3;
  }
  const SIZE_T extra = numbers.size() - defaults;
  if (!numbers.empty() && defaults < components &&
      !(type == ShaderParameterType::Color && defaults == 3)) {
    outError = StringUtils::format("A {0} default needs {1} numbers.",
                                   ShaderParameters::getTypeName(type), components);
    return false;
  }
  if (extra != 0 && (!bScalar || extra != 2)) {
    outError = bScalar ? String("Give a minimum and a maximum after the default, or neither.")
                       : String("Only float and int parameters take a minimum and maximum.");
    return false;
  }

  const Vector4& start = outParameter.defaultValue;
  float value[4] = {start.x, start.y, start.z, start.w};
  for (SIZE_T i = 0; i < defaults; ++i) {
    value[i] = numbers[i];
  }
  outParameter.defaultValue = Vector4(value[0], value[1], value[2], value[3]);
  if (type == ShaderParameterType::Int || type == ShaderParameterType::Bool) {
    if (value[0] != Math::floor(value[0])) {
      outError = "An int or bool default must be a whole number.";
      return false;
    }
  }
  if (type == ShaderParameterType::Bool && value[0] != 0.0f && value[0] != 1.0f) {
    outError = "A bool default is 0 or 1.";
    return false;
  }
  if (extra == 2) {
    outParameter.minimum = numbers[defaults];
    outParameter.maximum = numbers[defaults + 1];
    if (!(outParameter.minimum < outParameter.maximum)) {
      outError = "The minimum must be below the maximum.";
      return false;
    }
  }
  return true;
}
#endif // USING(CH_EDITOR)
} // namespace

/*
 */
uint32
ShaderParameters::getComponentCount(const ShaderParameterType type) noexcept
{
  const SIZE_T index = static_cast<SIZE_T>(type);
  return index < static_cast<SIZE_T>(ShaderParameterType::COUNT) ? kTypes[index].componentCount
                                                                   : 0;
}

/*
 */
StringView
ShaderParameters::getTypeName(const ShaderParameterType type) noexcept
{
  const SIZE_T index = static_cast<SIZE_T>(type);
  return index < static_cast<SIZE_T>(ShaderParameterType::COUNT) ? kTypes[index].name
                                                                   : StringView();
}

/*
 */
bool
ShaderParameters::parseTypeName(const StringView name, ShaderParameterType& outType) noexcept
{
  for (SIZE_T i = 0; i < static_cast<SIZE_T>(ShaderParameterType::COUNT); ++i) {
    if (kTypes[i].name == name) {
      outType = static_cast<ShaderParameterType>(i);
      return true;
    }
  }
  return false;
}

/*
 */
StringView
ShaderParameters::getStageName(const PostProcessStage stage) noexcept
{
  const SIZE_T index = static_cast<SIZE_T>(stage);
  return index < static_cast<SIZE_T>(PostProcessStage::COUNT) ? kStageNames[index]
                                                                : StringView();
}

/*
 */
bool
ShaderParameters::parseStageName(const StringView name, PostProcessStage& outStage) noexcept
{
  for (SIZE_T i = 0; i < static_cast<SIZE_T>(PostProcessStage::COUNT); ++i) {
    if (StringUtils::equalsIgnoreCase(kStageNames[i], name)) {
      outStage = static_cast<PostProcessStage>(i);
      return true;
    }
  }
  return false;
}

/*
 */
uint32
ShaderParameters::computeLayout(Vector<ShaderParameter>& parameters) noexcept
{
  uint32 offset = 0;
  for (ShaderParameter& parameter : parameters) {
    parameter.offset = offset;
    offset += SLOT_SIZE;
  }
  return offset;
}

/*
 */
void
ShaderParameters::writeValue(const ShaderParameter& parameter,
                             const Vector4& value,
                             Span<uint8> buffer) noexcept
{
  if (static_cast<SIZE_T>(parameter.offset) + SLOT_SIZE > buffer.size()) {
    return;
  }
  uint8* slot = buffer.data() + parameter.offset;
  if (parameter.type == ShaderParameterType::Int || parameter.type == ShaderParameterType::Bool) {
    const int32 number = static_cast<int32>(value.x);
    std::memset(slot, 0, SLOT_SIZE);
    std::memcpy(slot, &number, sizeof(number));
    return;
  }
  const float values[4] = {value.x, value.y, value.z, value.w};
  std::memcpy(slot, values, sizeof(values));
}

/*
 */
void
ShaderParameters::writeDefaults(Span<const ShaderParameter> parameters,
                                Span<uint8> buffer) noexcept
{
  for (const ShaderParameter& parameter : parameters) {
    writeValue(parameter, parameter.defaultValue, buffer);
  }
}

#if USING(CH_EDITOR)
/*
 */
bool
ShaderParameters::parseSource(const StringView text, SourceInfo& outInfo, String& outError)
{
  outInfo = SourceInfo();
  Vector<StringView> words;
  bool bStageSet = false;
  uint32 lineNumber = 0;
  SIZE_T lineStart = 0;
  while (lineStart <= text.size()) {
    SIZE_T lineEnd = text.find('\n', lineStart);
    if (lineEnd == StringView::npos) {
      lineEnd = text.size();
    }
    ++lineNumber;
    const StringView line = text.substr(lineStart, lineEnd - lineStart);
    lineStart = lineEnd + 1;

    const StringView parameterText = getDeclaration(line, kParamKeyword);
    const StringView stageText = getDeclaration(line, kStageKeyword);
    if (parameterText.empty() && stageText.empty()) {
      continue;
    }

    String lineError;
    if (!parameterText.empty()) {
      ShaderParameter parameter;
      if (!splitWords(parameterText, words)) {
        lineError = "A quote is not closed.";
      }
      else if (parseParameter(words, parameter, lineError)) {
        for (const ShaderParameter& other : outInfo.parameters) {
          if (StringUtils::equalsIgnoreCase(other.name, parameter.name)) {
            lineError = StringUtils::format("'{0}' is declared twice.", parameter.name);
            break;
          }
        }
        if (lineError.empty() && outInfo.parameters.size() >= MAX_PARAMETERS) {
          lineError = StringUtils::format("A shader has at most {0} parameters.", MAX_PARAMETERS);
        }
        if (lineError.empty()) {
          outInfo.parameters.push_back(std::move(parameter));
        }
      }
    }
    else if (bStageSet) {
      lineError = "CH_STAGE is given twice.";
    }
    else if (!parseStageName(stageText, outInfo.stage)) {
      lineError = StringUtils::format("'{0}' is not a stage (AfterTonemap, BeforeTonemap).",
                                      stageText);
    }
    else {
      bStageSet = true;
    }

    if (!lineError.empty()) {
      outError = StringUtils::format("Line {0}: {1}", lineNumber, lineError);
      return false;
    }
  }
  outInfo.bufferSize = computeLayout(outInfo.parameters);
  return true;
}

/*
 */
String
ShaderParameters::makeHlslStruct(Span<const ShaderParameter> parameters)
{
  String text = "// Made by the shader codec from the CH_PARAM lines.\nstruct ShaderParams\n{\n";
  if (parameters.empty()) {
    text += "  float4 _unused;\n";
  }
  uint32 padding = 0;
  for (const ShaderParameter& parameter : parameters) {
    const uint32 components = getComponentCount(parameter.type);
    switch (parameter.type) {
    case ShaderParameterType::Int:
      text += StringUtils::format("  int {0};\n", parameter.name);
      break;
    case ShaderParameterType::Bool:
      text += StringUtils::format("  uint {0};\n", parameter.name);
      break;
    case ShaderParameterType::Color:
      text += StringUtils::format("  float4 {0};\n", parameter.name);
      break;
    default:
      text += components == 1 ? StringUtils::format("  float {0};\n", parameter.name)
                              : StringUtils::format("  float{0} {1};\n", components,
                                                    parameter.name);
      break;
    }
    const uint32 slotComponents =
        parameter.type == ShaderParameterType::Int || parameter.type == ShaderParameterType::Bool
            ? 1
            : components;
    if (slotComponents < 4) {
      const uint32 left = 4 - slotComponents;
      text += left == 1 ? StringUtils::format("  float _pad{0};\n", padding)
                        : StringUtils::format("  float{0} _pad{1};\n", left, padding);
      ++padding;
    }
  }
  text += "};\n";
  return text;
}
#endif // USING(CH_EDITOR)

} // namespace chEngineSDK
