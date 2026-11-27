/************************************************************************/
/**
 * @file chMaterialOverrides.cpp
 * @author AccelMR
 * @date 2026/11/27
 * @brief
 *  Values and textures a material instance sets in place of its parent's.
 */
/************************************************************************/
#include "chMaterialOverrides.h"

#include "chAssetFile.h"
#include "chAssetManager.h"
#include "chFileStream.h"
#include "chLogger.h"
#include "chTextureAsset.h"

namespace chEngineSDK {

/*
 */
bool
MaterialOverrides::setValue(const StringView name, const Vector4& value)
{
  if (!Material::isValidValue(name, value)) {
    return false;
  }
  for (ShaderValueOverride& entry : m_values) {
    if (entry.name == name) {
      entry.value = value;
      return true;
    }
  }
  m_values.push_back({.name = String(name), .value = value});
  return true;
}

/*
 */
bool
MaterialOverrides::resetValue(const StringView name)
{
  for (SIZE_T i = 0; i < m_values.size(); ++i) {
    if (m_values[i].name == name) {
      m_values.erase(m_values.begin() + i);
      return true;
    }
  }
  return false;
}

/*
 */
const ShaderValueOverride*
MaterialOverrides::findValue(const StringView name) const noexcept
{
  for (const ShaderValueOverride& entry : m_values) {
    if (entry.name == name) {
      return &entry;
    }
  }
  return nullptr;
}

/*
 */
void
MaterialOverrides::setTexture(const MaterialTexture slot, const SPtr<TextureAsset>& texture)
{
  setTexture(slot, texture ? texture->getUUID() : UUID::null(), texture);
}

/*
 */
void
MaterialOverrides::setTexture(const MaterialTexture slot,
                              const UUID& textureId,
                              const SPtr<TextureAsset>& texture)
{
  CH_ASSERT(slot < MaterialTexture::COUNT);
  for (MaterialTextureOverride& entry : m_textures) {
    if (entry.slot == slot) {
      entry.id = textureId;
      entry.asset = texture;
      return;
    }
  }
  m_textures.push_back({.slot = slot, .id = textureId, .asset = texture});
}

/*
 */
bool
MaterialOverrides::resetTexture(const MaterialTexture slot)
{
  for (SIZE_T i = 0; i < m_textures.size(); ++i) {
    if (m_textures[i].slot == slot) {
      m_textures.erase(m_textures.begin() + i);
      return true;
    }
  }
  return false;
}

/*
 */
const MaterialTextureOverride*
MaterialOverrides::findTexture(const MaterialTexture slot) const noexcept
{
  for (const MaterialTextureOverride& entry : m_textures) {
    if (entry.slot == slot) {
      return &entry;
    }
  }
  return nullptr;
}

/*
 */
void
MaterialOverrides::clear()
{
  m_values.clear();
  m_textures.clear();
}

/*
 */
void
MaterialOverrides::apply(const Material* parent, Material& out) const
{
  if (parent) {
    out.copyValuesFrom(*parent);
  }
  else {
    out.resetValues();
  }
  for (const ShaderValueOverride& entry : m_values) {
    out.setValue(entry.name, entry.value);
  }
  for (const MaterialTextureOverride& entry : m_textures) {
    out.setTexture(entry.slot, entry.id, entry.asset);
  }
}

/*
 */
void
MaterialOverrides::collectReferences(Vector<UUID>& outReferences) const
{
  for (const MaterialTextureOverride& entry : m_textures) {
    if (!entry.id.isNull()) {
      outReferences.push_back(entry.id);
    }
  }
}

/*
 */
void
MaterialOverrides::serialize(DataStream& stream) const
{
  const uint32 counts[2] = {static_cast<uint32>(m_values.size()),
                            static_cast<uint32>(m_textures.size())};
  stream.write(counts, sizeof(counts));
  for (const ShaderValueOverride& entry : m_values) {
    AssetFile::writeString(stream, entry.name);
    const float value[4] = {entry.value.x, entry.value.y, entry.value.z, entry.value.w};
    stream.write(value, sizeof(value));
  }
  for (const MaterialTextureOverride& entry : m_textures) {
    AssetFile::writeString(stream, Material::getTextureName(entry.slot));
    stream.write(&entry.id, sizeof(entry.id));
  }
}

/*
 */
bool
MaterialOverrides::deserialize(DataStream& stream, const StringView owner)
{
  uint32 counts[2];
  if (!AssetFile::readValue(stream, counts) || counts[0] > MAX_VALUES ||
      counts[1] > static_cast<uint32>(MaterialTexture::COUNT)) {
    return false;
  }

  Vector<ShaderValueOverride> values;
  values.reserve(counts[0]);
  String name;
  for (uint32 i = 0; i < counts[0]; ++i) {
    float value[4];
    if (!AssetFile::readString(stream, name, MAX_NAME_LENGTH) ||
        !AssetFile::readValue(stream, value)) {
      return false;
    }
    const Vector4 vector(value[0], value[1], value[2], value[3]);
    if (!Material::isValidValue(name, vector)) {
      CH_LOG_WARNING(AssetSystem, "{0} skips the value {1}, which materials do not take.",
                     owner, name);
      continue;
    }
    values.push_back({.name = name, .value = vector});
  }

  Vector<MaterialTextureOverride> textures;
  textures.reserve(counts[1]);
  for (uint32 i = 0; i < counts[1]; ++i) {
    UUID id;
    if (!AssetFile::readString(stream, name, MAX_NAME_LENGTH) ||
        !AssetFile::readValue(stream, id)) {
      return false;
    }
    const MaterialTexture slot = Material::findTexture(name);
    if (slot == MaterialTexture::COUNT) {
      CH_LOG_WARNING(AssetSystem, "{0} skips the texture {1}, which materials do not take.",
                     owner, name);
      continue;
    }
    textures.push_back({.slot = slot, .id = id, .asset = nullptr});
  }

  m_values = std::move(values);
  m_textures = std::move(textures);
  return true;
}

/*
 */
void
MaterialOverrides::resolveTextures(const StringView owner)
{
  for (MaterialTextureOverride& entry : m_textures) {
    entry.asset.reset();
    if (entry.id.isNull() || !AssetManager::isStarted()) {
      continue;
    }
    const SPtr<IAsset> asset = AssetManager::instance().getAsset(entry.id);
    entry.asset = asset ? asset->as<TextureAsset>() : nullptr;
    if (!entry.asset) {
      CH_LOG_WARNING(AssetSystem, "{0} lost its texture {1}.", owner, entry.id);
    }
  }
}

} // namespace chEngineSDK
