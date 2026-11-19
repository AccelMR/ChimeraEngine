/************************************************************************/
/**
 * @file chSpriteMaterialAsset.h
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Asset that shades sprites with a project shader and its values.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chIAsset.h"
#include "chShaderParameters.h"
#include "chTypeTraits.h"

namespace chEngineSDK {

class ShaderAsset;

// Written after the common asset start, before the overrides.
struct SpriteMaterialAssetHeader
{
  static constexpr uint32 VERSION = 1;

  uint32 version = VERSION;
  uint32 overrideCount = 0;
  UUID shader;
};
static_assert(sizeof(SpriteMaterialAssetHeader) == 24,
              "SpriteMaterialAssetHeader must have no padding");

/**
 * Exists so many sprites share one look made by a shader of the project (a hit flash, a
 * dissolve, an outline): the asset keeps the sprite shader and the values set in place of
 * its defaults, and sprites, sprite styles, projectiles and particles point at it. Sprites
 * with the same material draw together; what varies per sprite is its color, which the
 * shader may read as data of its own. A value changed here changes every sprite using it.
 */
class CH_CORE_EXPORT SpriteMaterialAsset : public IAsset
{
 public:
  SpriteMaterialAsset() = delete;

  SpriteMaterialAsset(const AssetMetadata& metadata) : IAsset(metadata) {}

  ~SpriteMaterialAsset() override;

  /**
   * Null keeps the material but draws sprites as the engine does. A shader of another kind
   * is refused.
   */
  bool
  setShader(const SPtr<ShaderAsset>& shader);

  NODISCARD FORCEINLINE const SPtr<ShaderAsset>&
  getShader() const noexcept
  {
    return m_shader;
  }

  /**
   * Kept when the asset is missing, so saving again keeps the reference.
   */
  NODISCARD FORCEINLINE const UUID&
  getShaderId() const noexcept
  {
    return m_shaderId;
  }

  /**
   * False when the shader has no parameter of that name.
   */
  bool
  setValue(StringView name, const Vector4& value);

  void
  resetValue(StringView name);

  /**
   * The override, else the shader's default. False when the shader has no such parameter.
   */
  NODISCARD bool
  getValue(StringView name, Vector4& outValue) const;

  NODISCARD const ShaderValueOverride*
  findOverride(StringView name) const noexcept;

  NODISCARD FORCEINLINE Span<const ShaderValueOverride>
  getOverrides() const noexcept
  {
    return m_overrides;
  }

  /**
   * True when the shader can draw (loaded, compiled for this engine).
   */
  NODISCARD bool
  canDraw() const noexcept;

  /**
   * The bytes the shader reads, built again only when a value or the shader changed.
   */
  NODISCARD Span<const uint8>
  getParameterData() const;

  static constexpr uint32 MAX_OVERRIDES = ShaderParameters::MAX_PARAMETERS * 4;

 protected:
  bool
  serialize(SPtr<DataStream> stream) override;

  bool
  deserialize(SPtr<DataStream> stream) override;

  void
  clearAssetData() override;

  void
  collectReferences(Vector<UUID>& outReferences) const override;

 private:
  SPtr<ShaderAsset> m_shader;
  UUID m_shaderId = UUID::null();
  Vector<ShaderValueOverride> m_overrides;
  mutable Vector<uint8> m_parameterData;
  mutable uint32 m_builtRevision = 0;
  mutable bool m_bDirty = true;
};

DECLARE_ASSET_TYPE(SpriteMaterialAsset);

} // namespace chEngineSDK
