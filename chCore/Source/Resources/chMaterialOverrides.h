/************************************************************************/
/**
 * @file chMaterialOverrides.h
 * @author AccelMR
 * @date 2026/11/27
 * @brief
 *  Values and textures a material instance sets in place of its parent's.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chMaterial.h"
#include "chShaderParameters.h"

namespace chEngineSDK {
class TextureAsset;

/**
 * A texture slot set by an instance. A null id clears the parent's texture.
 */
struct MaterialTextureOverride
{
  MaterialTexture slot = MaterialTexture::BaseColor;
  UUID id = UUID::null();
  SPtr<TextureAsset> asset;
};

/**
 * Exists so instance assets and instances made at run time keep the changes they make to a
 * parent material the same way: only what was set is stored, by name, and apply() lays it
 * over the parent, so an edit of the parent still reaches whatever the instance leaves.
 */
class CH_CORE_EXPORT MaterialOverrides
{
 public:
  /**
   * False for a name Material does not know or a value it refuses.
   */
  bool
  setValue(StringView name, const Vector4& value);

  /**
   * False when the name was not set.
   */
  bool
  resetValue(StringView name);

  NODISCARD const ShaderValueOverride*
  findValue(StringView name) const noexcept;

  NODISCARD FORCEINLINE Span<const ShaderValueOverride>
  getValues() const noexcept
  {
    return m_values;
  }

  /**
   * Null clears the slot (the parent's texture is not used).
   */
  void
  setTexture(MaterialTexture slot, const SPtr<TextureAsset>& texture);

  /**
   * Used while loading: keeps the id even when the asset is missing.
   */
  void
  setTexture(MaterialTexture slot, const UUID& textureId, const SPtr<TextureAsset>& texture);

  bool
  resetTexture(MaterialTexture slot);

  NODISCARD const MaterialTextureOverride*
  findTexture(MaterialTexture slot) const noexcept;

  NODISCARD FORCEINLINE Span<const MaterialTextureOverride>
  getTextures() const noexcept
  {
    return m_textures;
  }

  NODISCARD FORCEINLINE bool
  isEmpty() const noexcept
  {
    return m_values.empty() && m_textures.empty();
  }

  void
  clear();

  /**
   * Writes the parent's values (the defaults without one) and then these into out.
   */
  void
  apply(const Material* parent, Material& out) const;

  void
  collectReferences(Vector<UUID>& outReferences) const;

  void
  serialize(DataStream& stream) const;

  /**
   * Reads the values and texture ids; textures stay null until resolveTextures runs.
   * Unknown names are skipped with a warning naming owner, so a value removed from Material
   * does not break old files.
   */
  NODISCARD bool
  deserialize(DataStream& stream, StringView owner);

  /**
   * Finds every texture in the asset manager (they are references, so already loaded).
   */
  void
  resolveTextures(StringView owner);

  static constexpr uint32 MAX_VALUES = 64;
  static constexpr uint32 MAX_NAME_LENGTH = 64;

 private:
  Vector<ShaderValueOverride> m_values;
  Vector<MaterialTextureOverride> m_textures;
};

} // namespace chEngineSDK
