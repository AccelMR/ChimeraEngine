/************************************************************************/
/**
 * @file chSpriteMaterialAsset.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Asset that shades sprites with a project shader and its values.
 */
/************************************************************************/
#include "chSpriteMaterialAsset.h"

#include "chAssetFile.h"
#include "chAssetManager.h"
#include "chFileStream.h"
#include "chLogger.h"
#include "chMath.h"
#include "chShaderAsset.h"

namespace chEngineSDK {

/*
 */
SpriteMaterialAsset::~SpriteMaterialAsset() = default;

/*
 */
bool
SpriteMaterialAsset::setShader(const SPtr<ShaderAsset>& shader)
{
  if (shader && shader->isLoaded() && shader->getKind() != ShaderAssetKind::Sprite) {
    CH_LOG_WARNING(AssetSystem, "Shader {0} is not a sprite shader (// CH_SHADER Sprite).",
                   shader->getName());
    return false;
  }
  m_shader = shader;
  m_shaderId = shader ? shader->getUUID() : UUID::null();
  m_bDirty = true;
  return true;
}

/*
 */
bool
SpriteMaterialAsset::setValue(const StringView name, const Vector4& value)
{
  if (!m_shader || m_shader->findParameter(name) == ShaderAsset::NOT_FOUND ||
      !Math::isFinite(value.x) || !Math::isFinite(value.y) || !Math::isFinite(value.z) ||
      !Math::isFinite(value.w)) {
    return false;
  }
  m_bDirty = true;
  for (ShaderValueOverride& entry : m_overrides) {
    if (entry.name == name) {
      entry.value = value;
      return true;
    }
  }
  m_overrides.push_back({.name = String(name), .value = value});
  return true;
}

/*
 */
void
SpriteMaterialAsset::resetValue(const StringView name)
{
  for (SIZE_T i = 0; i < m_overrides.size(); ++i) {
    if (m_overrides[i].name == name) {
      m_overrides.erase(m_overrides.begin() + i);
      m_bDirty = true;
      return;
    }
  }
}

/*
 */
bool
SpriteMaterialAsset::getValue(const StringView name, Vector4& outValue) const
{
  const uint32 parameter = m_shader ? m_shader->findParameter(name) : ShaderAsset::NOT_FOUND;
  if (parameter == ShaderAsset::NOT_FOUND) {
    return false;
  }
  const ShaderValueOverride* entry = findOverride(name);
  outValue = entry ? entry->value : m_shader->getParameters()[parameter].defaultValue;
  return true;
}

/*
 */
const ShaderValueOverride*
SpriteMaterialAsset::findOverride(const StringView name) const noexcept
{
  for (const ShaderValueOverride& entry : m_overrides) {
    if (entry.name == name) {
      return &entry;
    }
  }
  return nullptr;
}

/*
 */
bool
SpriteMaterialAsset::canDraw() const noexcept
{
  // An unloaded shader holds no code.
  return m_shader && !m_shader->getData().binaries.empty() &&
         m_shader->getKind() == ShaderAssetKind::Sprite && m_shader->isInterfaceCurrent();
}

/*
 */
Span<const uint8>
SpriteMaterialAsset::getParameterData() const
{
  if (!m_shader) {
    m_parameterData.clear();
    return m_parameterData;
  }
  if (m_bDirty || m_builtRevision != m_shader->getRevision()) {
    ShaderParameters::buildBuffer(m_shader->getParameters(), m_shader->getParameterBufferSize(),
                                  m_overrides, m_parameterData);
    m_builtRevision = m_shader->getRevision();
    m_bDirty = false;
  }
  return m_parameterData;
}

/*
 */
bool
SpriteMaterialAsset::serialize(SPtr<DataStream> stream)
{
  if (!stream) {
    return false;
  }
  const SpriteMaterialAssetHeader header{
      .overrideCount = static_cast<uint32>(m_overrides.size()), .shader = m_shaderId};
  *stream << header;
  for (const ShaderValueOverride& entry : m_overrides) {
    AssetFile::writeString(*stream, entry.name);
    const float value[4] = {entry.value.x, entry.value.y, entry.value.z, entry.value.w};
    stream->write(value, sizeof(value));
  }
  return true;
}

/*
 */
bool
SpriteMaterialAsset::deserialize(SPtr<DataStream> stream)
{
  SpriteMaterialAssetHeader header;
  if (!stream || !AssetFile::readValue(*stream, header)) {
    CH_LOG_ERROR(AssetSystem, "Sprite material {0} is cut short.", getName());
    return false;
  }
  if (header.version != SpriteMaterialAssetHeader::VERSION) {
    CH_LOG_ERROR(AssetSystem,
                 "Sprite material {0} has version {1}, this build reads {2}; make it again.",
                 getName(), header.version, SpriteMaterialAssetHeader::VERSION);
    return false;
  }
  if (header.overrideCount > MAX_OVERRIDES) {
    CH_LOG_ERROR(AssetSystem, "Sprite material {0} has broken values.", getName());
    return false;
  }
  Vector<ShaderValueOverride> overrides(header.overrideCount);
  for (ShaderValueOverride& entry : overrides) {
    float value[4];
    if (!AssetFile::readString(*stream, entry.name, ShaderParameters::MAX_NAME_LENGTH) ||
        !AssetFile::readValue(*stream, value)) {
      CH_LOG_ERROR(AssetSystem, "Sprite material {0} is cut short.", getName());
      return false;
    }
    entry.value = Vector4(value[0], value[1], value[2], value[3]);
  }

  // The shader is a reference, so it was loaded before this asset.
  SPtr<ShaderAsset> shader;
  if (!header.shader.isNull() && AssetManager::isStarted()) {
    const SPtr<IAsset> asset = AssetManager::instance().getAsset(header.shader);
    shader = asset ? asset->as<ShaderAsset>() : nullptr;
  }
  if (!shader && !header.shader.isNull()) {
    CH_LOG_WARNING(AssetSystem, "Sprite material {0} lost its shader {1}.", getName(),
                   header.shader);
  }
  m_shader = shader;
  m_shaderId = header.shader;
  m_overrides = std::move(overrides);
  m_bDirty = true;
  return true;
}

/*
 */
void
SpriteMaterialAsset::clearAssetData()
{
  m_shader.reset();
  m_overrides.clear();
  m_parameterData.clear();
  m_bDirty = true;
}

/*
 */
void
SpriteMaterialAsset::collectReferences(Vector<UUID>& outReferences) const
{
  if (!m_shaderId.isNull()) {
    outReferences.push_back(m_shaderId);
  }
}

} // namespace chEngineSDK
