/************************************************************************/
/**
 * @file chShaderParameters.h
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Values a project shader declares for the editor and scripts to set.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chVector4.h"

namespace chEngineSDK {

// Stored in shader assets, so the values are fixed.
enum class ShaderParameterType : uint32
{
  Float = 0,
  Float2 = 1,
  Float3 = 2,
  Float4 = 3,
  Color = 4,
  Int = 5,
  Bool = 6,
  COUNT
};

// Stored in shader assets, so the values are fixed.
enum class PostProcessStage : uint32
{
  // On the final image, after the tonemap: CRT, vignette, pixelation.
  AfterTonemap = 0,
  // On the linear HDR scene color, before the tonemap: bloom, color grading.
  BeforeTonemap = 1,
  COUNT
};

struct ShaderParameter
{
  String name;
  ShaderParameterType type = ShaderParameterType::Float;
  // Ints and bools keep their number in x.
  Vector4 defaultValue = Vector4::ZERO;
  // The editor shows a slider when minimum is below maximum.
  float minimum = 0.0f;
  float maximum = 0.0f;
  // Byte offset inside the parameter buffer.
  uint32 offset = 0;
  String tooltip;
};

/**
 * Exists so a shader written for a project can say which values it reads, and the engine
 * can place them in a constant buffer the same way on every graphics API. Each parameter
 * takes its own 16-byte slot, which keeps the layout free of packing rules that differ
 * between shader targets. The editor reads the declarations from comment lines of the
 * source:
 *   // CH_PARAM float intensity 1.0 0 2 "How strong the effect is"
 *   // CH_PARAM color tint 1 1 1 1
 *   // CH_STAGE BeforeTonemap
 */
class CH_CORE_EXPORT ShaderParameters
{
 public:
  NODISCARD static uint32
  getComponentCount(ShaderParameterType type) noexcept;

  /**
   * The name a CH_PARAM line uses, such as "float2" or "color".
   */
  NODISCARD static StringView
  getTypeName(ShaderParameterType type) noexcept;

  NODISCARD static bool
  parseTypeName(StringView name, ShaderParameterType& outType) noexcept;

  NODISCARD static StringView
  getStageName(PostProcessStage stage) noexcept;

  NODISCARD static bool
  parseStageName(StringView name, PostProcessStage& outStage) noexcept;

  /**
   * Gives every parameter its offset; returns the size of the buffer that holds them.
   */
  static uint32
  computeLayout(Vector<ShaderParameter>& parameters) noexcept;

  /**
   * Writes value into the parameter's slot of buffer, as the shader reads it: floats as they
   * are, ints and bools as 32-bit integers.
   */
  static void
  writeValue(const ShaderParameter& parameter, const Vector4& value, Span<uint8> buffer) noexcept;

  /**
   * Fills buffer with the default of every parameter.
   */
  static void
  writeDefaults(Span<const ShaderParameter> parameters, Span<uint8> buffer) noexcept;

#if USING(CH_EDITOR)
  struct SourceInfo
  {
    Vector<ShaderParameter> parameters;
    PostProcessStage stage = PostProcessStage::AfterTonemap;
    uint32 bufferSize = 0;
  };

  /**
   * Reads the CH_PARAM and CH_STAGE comment lines of a shader source. outError names the
   * line of the first mistake.
   */
  NODISCARD static bool
  parseSource(StringView text, SourceInfo& outInfo, String& outError);

  /**
   * HLSL text of the ShaderParams struct that matches the layout of parameters.
   */
  NODISCARD static String
  makeHlslStruct(Span<const ShaderParameter> parameters);
#endif // USING(CH_EDITOR)

  static constexpr uint32 SLOT_SIZE = 16;
  static constexpr uint32 MAX_PARAMETERS = 16;
  static constexpr uint32 MAX_BUFFER_SIZE = SLOT_SIZE * MAX_PARAMETERS;
  static constexpr uint32 MAX_NAME_LENGTH = 64;
  static constexpr uint32 MAX_TOOLTIP_LENGTH = 256;
};

} // namespace chEngineSDK
