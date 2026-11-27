/************************************************************************/
/**
 * @file chMaterialInstanceAsset.cpp
 * @author AccelMR
 * @date 2026/11/27
 * @brief
 *  Asset that takes a parent material and changes only some of its values.
 */
/************************************************************************/
#include "chMaterialInstanceAsset.h"

#include "chAlgorithm.h"
#include "chAssetFile.h"
#include "chAssetManager.h"
#include "chFileStream.h"
#include "chLogger.h"

namespace chEngineSDK {

/*
 */
MaterialInstanceAsset::~MaterialInstanceAsset()
{
  linkParent(nullptr);
}

/*
 */
bool
MaterialInstanceAsset::setParent(const SPtr<MaterialInterfaceAsset>& parent)
{
  if (!canUseParent(parent.get())) {
    return false;
  }
  linkParent(parent);
  m_parentId = parent ? parent->getUUID() : UUID::null();
  refresh();
  return true;
}

/*
 */
SPtr<MaterialInterfaceAsset>
MaterialInstanceAsset::getParentMaterial() const
{
  return m_parent;
}

/*
 */
bool
MaterialInstanceAsset::canUseParent(const MaterialInterfaceAsset* parent) const
{
  // This instance counts as the first link, so a chain of MAX_DEPTH instances is the limit.
  SPtr<MaterialInterfaceAsset> current;
  const MaterialInterfaceAsset* link = parent;
  for (uint32 depth = 1; link; ++depth) {
    if (link == this || depth >= MAX_DEPTH) {
      return false;
    }
    current = link->getParentMaterial();
    link = current.get();
  }
  return true;
}

/*
 */
bool
MaterialInstanceAsset::setValue(const StringView name, const Vector4& value)
{
  if (!m_overrides.setValue(name, value)) {
    return false;
  }
  refresh();
  return true;
}

/*
 */
bool
MaterialInstanceAsset::resetValue(const StringView name)
{
  if (!m_overrides.resetValue(name)) {
    return false;
  }
  refresh();
  return true;
}

/*
 */
void
MaterialInstanceAsset::setTexture(const MaterialTexture slot,
                                  const SPtr<TextureAsset>& texture)
{
  m_overrides.setTexture(slot, texture);
  refresh();
}

/*
 */
bool
MaterialInstanceAsset::resetTexture(const MaterialTexture slot)
{
  if (!m_overrides.resetTexture(slot)) {
    return false;
  }
  refresh();
  return true;
}

/*
 */
void
MaterialInstanceAsset::onParentMaterialChanged()
{
  refresh();
}

/*
 */
void
MaterialInstanceAsset::refresh()
{
  m_overrides.apply(m_parent ? &m_parent->getMaterial() : nullptr, m_material);
  notifyMaterialChanged();
}

/*
 */
void
MaterialInstanceAsset::linkParent(const SPtr<MaterialInterfaceAsset>& parent)
{
  if (parent == m_parent) {
    return;
  }
  if (m_parent) {
    m_parent->removeListener(*this);
  }
  m_parent = parent;
  if (m_parent) {
    m_parent->addListener(*this);
  }
}

/*
 */
bool
MaterialInstanceAsset::serialize(SPtr<DataStream> stream)
{
  if (!stream) {
    return false;
  }
  const MaterialInstanceAssetHeader header{.parent = m_parentId};
  *stream << header;
  m_overrides.serialize(*stream);
  return true;
}

/*
 */
bool
MaterialInstanceAsset::deserialize(SPtr<DataStream> stream)
{
  MaterialInstanceAssetHeader header;
  if (!stream || !AssetFile::readValue(*stream, header)) {
    CH_LOG_ERROR(AssetSystem, "Material instance {0} is cut short.", getName());
    return false;
  }
  if (header.version != MaterialInstanceAssetHeader::VERSION) {
    CH_LOG_ERROR(AssetSystem,
                 "Material instance {0} has version {1}, this build reads {2}; make it again.",
                 getName(), header.version, MaterialInstanceAssetHeader::VERSION);
    return false;
  }
  if (!m_overrides.deserialize(*stream, getName())) {
    CH_LOG_ERROR(AssetSystem, "Material instance {0} has broken values.", getName());
    m_overrides.clear();
    return false;
  }
  m_overrides.resolveTextures(getName());

  // The parent is a reference, so it was loaded before this asset.
  SPtr<MaterialInterfaceAsset> parent;
  if (!header.parent.isNull() && AssetManager::isStarted()) {
    parent = fromAsset(AssetManager::instance().getAsset(header.parent));
    if (!parent) {
      CH_LOG_WARNING(AssetSystem, "Material instance {0} lost its parent {1}.", getName(),
                     header.parent);
    }
    else if (!canUseParent(parent.get())) {
      CH_LOG_WARNING(AssetSystem,
                     "Material instance {0} would loop or chain too far through {1}; it "
                     "uses no parent.",
                     getName(), parent->getName());
      parent.reset();
    }
  }
  linkParent(parent);
  m_parentId = header.parent;
  refresh();
  return true;
}

/*
 */
void
MaterialInstanceAsset::clearAssetData()
{
  linkParent(nullptr);
  m_overrides.clear();
  m_material.resetValues();
  notifyMaterialChanged();
}

/*
 */
void
MaterialInstanceAsset::collectReferences(Vector<UUID>& outReferences) const
{
  if (!m_parentId.isNull()) {
    outReferences.push_back(m_parentId);
  }
  m_overrides.collectReferences(outReferences);
}

} // namespace chEngineSDK
