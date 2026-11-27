/************************************************************************/
/**
 * @file chMaterialInstanceAsset.h
 * @author AccelMR
 * @date 2026/11/27
 * @brief
 *  Asset that takes a parent material and changes only some of its values.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chMaterialInterfaceAsset.h"
#include "chMaterialOverrides.h"
#include "chTypeTraits.h"

namespace chEngineSDK {

// Written after the common asset start, before the overrides.
struct MaterialInstanceAssetHeader
{
  static constexpr uint32 VERSION = 1;

  uint32 version = VERSION;
  UUID parent;
};
static_assert(sizeof(MaterialInstanceAssetHeader) == 20,
              "MaterialInstanceAssetHeader must have no padding");

/**
 * Exists so variations of a material (another color, another texture) stay linked to it:
 * the instance keeps a parent, a material or another instance, and only the values it sets,
 * and reads the rest from the parent, so an edit of the parent still reaches it. It can be
 * used wherever a material is. A chain of instances never loops and is at most MAX_DEPTH
 * long.
 */
class CH_CORE_EXPORT MaterialInstanceAsset : public MaterialInterfaceAsset,
                                             public IMaterialListener
{
 public:
  MaterialInstanceAsset() = delete;

  MaterialInstanceAsset(const AssetMetadata& metadata) : MaterialInterfaceAsset(metadata) {}

  ~MaterialInstanceAsset() override;

  /**
   * Null takes the default material values. False, keeping the old parent, when the parent
   * would make a loop or a chain longer than MAX_DEPTH.
   */
  bool
  setParent(const SPtr<MaterialInterfaceAsset>& parent);

  NODISCARD SPtr<MaterialInterfaceAsset>
  getParentMaterial() const override;

  /**
   * Kept when the parent asset is missing, so saving again keeps the reference.
   */
  NODISCARD FORCEINLINE const UUID&
  getParentId() const noexcept
  {
    return m_parentId;
  }

  /**
   * True when parent could be set without a loop or a chain past MAX_DEPTH.
   */
  NODISCARD bool
  canUseParent(const MaterialInterfaceAsset* parent) const;

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

  void
  onParentMaterialChanged() override;

  static constexpr uint32 MAX_DEPTH = 16;

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
  /**
   * Lays the overrides over the parent and tells the listeners.
   */
  void
  refresh();

  void
  linkParent(const SPtr<MaterialInterfaceAsset>& parent);

  SPtr<MaterialInterfaceAsset> m_parent;
  UUID m_parentId = UUID::null();
  MaterialOverrides m_overrides;
};

DECLARE_ASSET_TYPE(MaterialInstanceAsset);

} // namespace chEngineSDK
