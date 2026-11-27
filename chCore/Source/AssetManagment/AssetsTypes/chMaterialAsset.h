/************************************************************************/
/**
 * @file chMaterialAsset.h
 * @author AccelMR
 * @date 2026/10/07
 * @brief
 *  A Material saved as an asset, with references to its textures.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chMaterialInterfaceAsset.h"
#include "chTypeTraits.h"

namespace chEngineSDK {

/**
 * Exists to save a Material and share it between models. Editing it changes every object
 * that uses it and every instance made from it (call notifyMaterialChanged after an edit).
 */
class CH_CORE_EXPORT MaterialAsset : public MaterialInterfaceAsset
{
 public:
  MaterialAsset() = delete;
  MaterialAsset(const AssetMetadata& metadata) : MaterialInterfaceAsset(metadata) {}
  ~MaterialAsset() override = default;

  using MaterialInterfaceAsset::getMaterial;

  NODISCARD FORCEINLINE Material&
  getMaterial() noexcept { return m_material; }

 protected:
  bool
  serialize(SPtr<DataStream> stream) override;

  bool
  deserialize(SPtr<DataStream> stream) override;

  void
  clearAssetData() override;

  void
  collectReferences(Vector<UUID>& outReferences) const override;
};
DECLARE_ASSET_TYPE(MaterialAsset);

} // namespace chEngineSDK
