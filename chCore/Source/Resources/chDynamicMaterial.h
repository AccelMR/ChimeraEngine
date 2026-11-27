/************************************************************************/
/**
 * @file chDynamicMaterial.h
 * @author AccelMR
 * @date 2026/11/27
 * @brief
 *  Instance of a material made while the game runs, never saved.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chMaterialInterfaceAsset.h"
#include "chMaterialOverrides.h"

namespace chEngineSDK {

/**
 * Exists so a game changes the look of some objects (a hit flash, a team color) without
 * touching the material other objects share: it keeps a parent asset and the values set on
 * it, follows the parent's edits like an instance asset, and is shared by SPtr between the
 * components that draw with it. It lives only in memory.
 */
class CH_CORE_EXPORT DynamicMaterial : public IMaterialListener
{
 public:
  explicit DynamicMaterial(const SPtr<MaterialInterfaceAsset>& parent = nullptr);

  DynamicMaterial(const DynamicMaterial&) = delete;

  DynamicMaterial&
  operator=(const DynamicMaterial&) = delete;

  ~DynamicMaterial();

  /**
   * Null takes the default material values.
   */
  void
  setParent(const SPtr<MaterialInterfaceAsset>& parent);

  NODISCARD FORCEINLINE const SPtr<MaterialInterfaceAsset>&
  getParent() const noexcept
  {
    return m_parent;
  }

  /**
   * Takes the parent and every value and texture of other.
   */
  void
  copyFrom(const DynamicMaterial& other);

  /**
   * False for a name or value Material refuses.
   */
  bool
  setValue(StringView name, const Vector4& value);

  bool
  resetValue(StringView name);

  /**
   * Null clears the texture instead of using the parent's.
   */
  void
  setTexture(MaterialTexture slot, const SPtr<TextureAsset>& texture);

  bool
  resetTexture(MaterialTexture slot);

  NODISCARD FORCEINLINE const MaterialOverrides&
  getOverrides() const noexcept
  {
    return m_overrides;
  }

  NODISCARD FORCEINLINE const Material&
  getMaterial() const noexcept
  {
    return m_material;
  }

  void
  onParentMaterialChanged() override;

 private:
  void
  refresh();

  SPtr<MaterialInterfaceAsset> m_parent;
  MaterialOverrides m_overrides;
  Material m_material;
};

} // namespace chEngineSDK
