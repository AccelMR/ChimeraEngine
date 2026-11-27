/************************************************************************/
/**
 * @file chMaterialInterfaceAsset.cpp
 * @author AccelMR
 * @date 2026/11/27
 * @brief
 *  Base of every asset that gives a surface a Material: materials and their instances.
 */
/************************************************************************/
#include "chMaterialInterfaceAsset.h"

#include "chAlgorithm.h"
#include "chMaterialAsset.h"
#include "chMaterialInstanceAsset.h"

namespace chEngineSDK {

/*
 */
MaterialInterfaceAsset::~MaterialInterfaceAsset()
{
  // Listeners keep their parent alive, so none can be left when the parent dies.
  CH_ASSERT(m_listeners.empty());
}

/*
 */
SPtr<MaterialInterfaceAsset>
MaterialInterfaceAsset::getParentMaterial() const
{
  return nullptr;
}

/*
 */
void
MaterialInterfaceAsset::addListener(IMaterialListener& listener)
{
  if (!Algorithm::contains(m_listeners, &listener)) {
    m_listeners.push_back(&listener);
  }
}

/*
 */
void
MaterialInterfaceAsset::removeListener(IMaterialListener& listener)
{
  Algorithm::removeFirst(m_listeners, &listener);
}

/*
 */
void
MaterialInterfaceAsset::notifyMaterialChanged()
{
  // By index: an instance told here tells its own listeners, never these.
  for (SIZE_T i = 0; i < m_listeners.size(); ++i) {
    m_listeners[i]->onParentMaterialChanged();
  }
}

/*
 */
SPtr<MaterialInterfaceAsset>
MaterialInterfaceAsset::fromAsset(const SPtr<IAsset>& asset)
{
  if (!asset || !isMaterialType(asset->getAssetTypeId())) {
    return nullptr;
  }
  return std::static_pointer_cast<MaterialInterfaceAsset>(asset);
}

/*
 */
bool
MaterialInterfaceAsset::isMaterialType(const UUID& assetType)
{
  return assetType == AssetTypeTraits<MaterialAsset>::getTypeId() ||
         assetType == AssetTypeTraits<MaterialInstanceAsset>::getTypeId();
}

} // namespace chEngineSDK
