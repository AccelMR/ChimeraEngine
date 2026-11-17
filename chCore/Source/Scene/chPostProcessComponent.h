/************************************************************************/
/**
 * @file chPostProcessComponent.h
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Component that runs project shaders over the rendered image.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chComponent.h"
#include "chPostProcessItem.h"
#include "chTypeTraits.h"
#include "chUUID.h"
#include "chVector4.h"

namespace chEngineSDK {

class ShaderAsset;

/**
 * A value set on a pass in place of its shader's default; numbers, vectors and colors use
 * as many components as the parameter has, ints and bools x.
 */
struct PostProcessOverride
{
  String name;
  Vector4 value = Vector4::ZERO;
};

struct PostProcessPass
{
  SPtr<ShaderAsset> shader;
  // Kept when the asset is missing, so saving again keeps the reference.
  UUID shaderId = UUID::null();
  bool bEnabled = true;
  float weight = 1.0f;
  Vector<PostProcessOverride> overrides;
};

/**
 * Exists so a scene can change how its image looks (CRT, vignette, color grading) with
 * shaders the project brings. Each pass reads what the one before wrote; a pass runs before
 * or after the tonemap as its shader says. Every enabled component of the scene adds its
 * passes, lower priority first. The parameter bytes of each pass are built again only when
 * a value or the shader changes, so the render frame copies them as they are.
 */
class CH_CORE_EXPORT PostProcessComponent : public Component
{
 public:
  PostProcessComponent() = default;

  /**
   * Adds a pass at the end; returns its index.
   */
  uint32
  addPass(const SPtr<ShaderAsset>& shader);

  void
  removePass(uint32 index);

  /**
   * Moves a pass to another place of the list, shifting the ones between.
   */
  void
  movePass(uint32 from, uint32 to);

  void
  setPassShader(uint32 index, const SPtr<ShaderAsset>& shader);

  void
  setPassEnabled(uint32 index, bool bEnabled);

  void
  setPassWeight(uint32 index, float weight);

  NODISCARD FORCEINLINE uint32
  getPassCount() const noexcept
  {
    return static_cast<uint32>(m_passes.size());
  }

  NODISCARD FORCEINLINE const PostProcessPass&
  getPass(uint32 index) const
  {
    return m_passes[index].pass;
  }

  /**
   * The first pass whose shader has that name, or NOT_FOUND.
   */
  NODISCARD uint32
  findPass(StringView shaderName) const noexcept;

  /**
   * False when the pass's shader has no parameter of that name.
   */
  bool
  setValue(uint32 index, StringView name, const Vector4& value);

  /**
   * Goes back to the shader's default.
   */
  void
  resetValue(uint32 index, StringView name);

  /**
   * The value the pass uses: its override, else the shader's default. False when the
   * shader has no such parameter.
   */
  NODISCARD bool
  getValue(uint32 index, StringView name, Vector4& outValue) const;

  NODISCARD const PostProcessOverride*
  findOverride(uint32 index, StringView name) const noexcept;

  /**
   * Components of a scene run lower priority first; equals keep the order they registered.
   */
  void
  setPriority(int32 priority);

  NODISCARD FORCEINLINE int32
  getPriority() const noexcept
  {
    return m_priority;
  }

  /**
   * Appends every enabled pass that can draw (shader with code, weight above 0) and its
   * parameter bytes. Called by the render frame.
   */
  void
  appendPasses(Vector<PostProcessPassItem>& outPasses, Vector<uint8>& outParameters) const;

  void
  serialize(DataStream& stream) const override;

  NODISCARD bool
  deserialize(DataStream& stream) override;

  void
  collectReferences(Vector<UUID>& outReferences) const override;

  static constexpr uint32 SERIALIZATION_VERSION = 1;
  static constexpr uint32 NOT_FOUND = 0xFFFFFFFFu;
  static constexpr uint32 MAX_PASSES = 32;

 protected:
  void
  onRegister() override;

  void
  onUnregister() override;

 private:
  struct PassState
  {
    PostProcessPass pass;
    // Defaults plus overrides as the shader reads them; built again when they change.
    mutable Vector<uint8> parameterData;
    mutable uint32 builtRevision = 0;
    mutable bool bDirty = true;
  };

  void
  buildParameters(const PassState& state) const;

  Vector<PassState> m_passes;
  int32 m_priority = 0;
};

DECLARE_TYPE_TRAITS(PostProcessComponent)

} // namespace chEngineSDK
