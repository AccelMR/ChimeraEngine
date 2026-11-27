/************************************************************************/
/**
 * @file chDynamicMaterial.cpp
 * @author AccelMR
 * @date 2026/11/27
 * @brief
 *  Instance of a material made while the game runs, never saved.
 */
/************************************************************************/
#include "chDynamicMaterial.h"

namespace chEngineSDK {

/*
 */
DynamicMaterial::DynamicMaterial(const SPtr<MaterialInterfaceAsset>& parent)
{
  setParent(parent);
}

/*
 */
DynamicMaterial::~DynamicMaterial()
{
  if (m_parent) {
    m_parent->removeListener(*this);
  }
}

/*
 */
void
DynamicMaterial::setParent(const SPtr<MaterialInterfaceAsset>& parent)
{
  if (parent != m_parent) {
    if (m_parent) {
      m_parent->removeListener(*this);
    }
    m_parent = parent;
    if (m_parent) {
      m_parent->addListener(*this);
    }
  }
  refresh();
}

/*
 */
void
DynamicMaterial::copyFrom(const DynamicMaterial& other)
{
  if (&other == this) {
    return;
  }
  m_overrides = other.m_overrides;
  setParent(other.m_parent);
}

/*
 */
bool
DynamicMaterial::setValue(const StringView name, const Vector4& value)
{
  if (!m_overrides.setValue(name, value)) {
    return false;
  }
  // Only this value changed, so the rest of the material stays as resolved.
  m_material.setValue(name, value);
  return true;
}

/*
 */
bool
DynamicMaterial::resetValue(const StringView name)
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
DynamicMaterial::setTexture(const MaterialTexture slot, const SPtr<TextureAsset>& texture)
{
  m_overrides.setTexture(slot, texture);
  m_material.setTexture(slot, texture);
}

/*
 */
bool
DynamicMaterial::resetTexture(const MaterialTexture slot)
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
DynamicMaterial::onParentMaterialChanged()
{
  refresh();
}

/*
 */
void
DynamicMaterial::refresh()
{
  m_overrides.apply(m_parent ? &m_parent->getMaterial() : nullptr, m_material);
}

} // namespace chEngineSDK
