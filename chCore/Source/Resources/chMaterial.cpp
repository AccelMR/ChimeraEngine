/************************************************************************/
/**
 * @file chMaterial.cpp
 * @author AccelMR
 * @date 2026/10/07
 * @brief
 *  How a surface looks: colors and textures the renderer reads per mesh.
 */
/************************************************************************/
#include "chMaterial.h"

#include "chFileStream.h"
#include "chIBuffer.h"
#include "chIGraphicsAPI.h"
#include "chITexture.h"
#include "chMath.h"
#include "chTextureAsset.h"

namespace chEngineSDK {

namespace {
// Must match MaterialData in chMaterial.hlsli.
struct MaterialConstants
{
  float baseColorFactor[4];
  float emissiveFactor[3];
  float metallicFactor;
  float roughnessFactor;
  float normalScale;
  float occlusionStrength;
  uint32 baseColorTexture;
  uint32 metallicRoughnessTexture;
  uint32 normalTexture;
  uint32 emissiveTexture;
  uint32 occlusionTexture;
  float alphaCutoff;
  float padding[3];
};
static_assert(sizeof(MaterialConstants) == 80);

// Must match kNoTexture in chMaterial.hlsli: shaders skip the sample and use the factor.
constexpr uint32 kNoTexture = 0xFFFFFFFF;

constexpr uint32 kTextureCount = static_cast<uint32>(MaterialTexture::COUNT);

// What is written for each value after the version.
struct MaterialFileData
{
  float baseColorFactor[4];
  float emissiveFactor[3];
  float metallicFactor;
  float roughnessFactor;
  float normalScale;
  float occlusionStrength;
  UUID textures[kTextureCount];
  float alphaCutoff;
  uint32 alphaMode;
  uint32 twoSided;
};
static_assert(sizeof(MaterialFileData) == 56 + sizeof(UUID) * kTextureCount,
              "MaterialFileData must have no padding");

// Same order as kParameters.
enum class ParameterId : uint32
{
  BaseColor,
  Metallic,
  Roughness,
  Emissive,
  NormalScale,
  OcclusionStrength,
  AlphaMode,
  AlphaCutoff,
  TwoSided,

  COUNT
};

constexpr MaterialParameterInfo kParameters[] = {
    {.name = "baseColor", .type = MaterialParameterType::Color},
    {.name = "metallic", .type = MaterialParameterType::Float},
    {.name = "roughness", .type = MaterialParameterType::Float},
    {.name = "emissive", .type = MaterialParameterType::Color},
    {.name = "normalScale", .type = MaterialParameterType::Float},
    {.name = "occlusionStrength", .type = MaterialParameterType::Float},
    {.name = "alphaMode", .type = MaterialParameterType::AlphaMode},
    {.name = "alphaCutoff", .type = MaterialParameterType::Float},
    {.name = "twoSided", .type = MaterialParameterType::Bool},
};
static_assert(sizeof(kParameters) / sizeof(kParameters[0]) ==
              static_cast<SIZE_T>(ParameterId::COUNT));

// Same order as MaterialTexture.
constexpr const ANSICHAR* kTextureNames[] = {
    "baseColorTexture", "metallicRoughnessTexture", "normalTexture", "emissiveTexture",
    "occlusionTexture",
};
static_assert(sizeof(kTextureNames) / sizeof(kTextureNames[0]) == kTextureCount);

NODISCARD bool
isFinite(const Vector4& value)
{
  return Math::isFinite(value.x) && Math::isFinite(value.y) && Math::isFinite(value.z) &&
         Math::isFinite(value.w);
}

NODISCARD Vector4
toVector(const LinearColor& color)
{
  return Vector4(color.r, color.g, color.b, color.a);
}

NODISCARD LinearColor
toColor(const Vector4& value)
{
  return LinearColor(value.x, value.y, value.z, value.w);
}
} // namespace

/*
 */
void
Material::setBaseColorFactor(const LinearColor& color)
{
  m_baseColorFactor = color;
  m_bConstantsDirty = true;
}

/*
 */
void
Material::setMetallicFactor(float metallic)
{
  m_metallicFactor = metallic;
  m_bConstantsDirty = true;
}

/*
 */
void
Material::setRoughnessFactor(float roughness)
{
  m_roughnessFactor = roughness;
  m_bConstantsDirty = true;
}

/*
 */
void
Material::setNormalScale(float scale)
{
  m_normalScale = scale;
  m_bConstantsDirty = true;
}

/*
 */
void
Material::setOcclusionStrength(float strength)
{
  m_occlusionStrength = strength;
  m_bConstantsDirty = true;
}

/*
 */
void
Material::setEmissiveFactor(const LinearColor& color)
{
  m_emissiveFactor = color;
  m_bConstantsDirty = true;
}

/*
 */
void
Material::setAlphaMode(MaterialAlphaMode mode)
{
  CH_ASSERT(mode < MaterialAlphaMode::COUNT);
  m_alphaMode = mode;
}

/*
 */
void
Material::setAlphaCutoff(float cutoff)
{
  m_alphaCutoff = cutoff;
  m_bConstantsDirty = true;
}

/*
 */
void
Material::setTwoSided(bool bTwoSided)
{
  m_bTwoSided = bTwoSided;
}

/*
 */
void
Material::setTexture(MaterialTexture slot, const SPtr<TextureAsset>& texture)
{
  setTexture(slot, texture ? texture->getUUID() : UUID::null(), texture);
}

/*
 */
void
Material::setTexture(MaterialTexture slot,
                     const UUID& textureId,
                     const SPtr<TextureAsset>& texture)
{
  CH_ASSERT(slot < MaterialTexture::COUNT);
  TextureSlot& textureSlot = m_textures[static_cast<uint32>(slot)];
  textureSlot.id = textureId;
  textureSlot.asset = texture;
  textureSlot.gpuTexture = texture ? texture->getTexture().get() : nullptr;
  m_bConstantsDirty = true;
}

/*
 */
void
Material::resetValues()
{
  m_baseColorFactor = LinearColor(1.0f, 1.0f, 1.0f, 1.0f);
  m_emissiveFactor = LinearColor(0.0f, 0.0f, 0.0f, 1.0f);
  m_metallicFactor = 0.0f;
  m_roughnessFactor = 0.5f;
  m_normalScale = 1.0f;
  m_occlusionStrength = 1.0f;
  m_alphaCutoff = 0.5f;
  m_alphaMode = MaterialAlphaMode::Opaque;
  m_bTwoSided = false;
  for (TextureSlot& textureSlot : m_textures) {
    textureSlot = TextureSlot();
  }
  m_bConstantsDirty = true;
}

/*
 */
void
Material::copyValuesFrom(const Material& other)
{
  if (&other == this) {
    return;
  }
  m_baseColorFactor = other.m_baseColorFactor;
  m_emissiveFactor = other.m_emissiveFactor;
  m_metallicFactor = other.m_metallicFactor;
  m_roughnessFactor = other.m_roughnessFactor;
  m_normalScale = other.m_normalScale;
  m_occlusionStrength = other.m_occlusionStrength;
  m_alphaCutoff = other.m_alphaCutoff;
  m_alphaMode = other.m_alphaMode;
  m_bTwoSided = other.m_bTwoSided;
  for (uint32 i = 0; i < kTextureCount; ++i) {
    m_textures[i] = other.m_textures[i];
  }
  m_bConstantsDirty = true;
}

/*
 */
bool
Material::setValue(const StringView name, const Vector4& value)
{
  if (!isValidValue(name, value)) {
    return false;
  }
  switch (static_cast<ParameterId>(findParameter(name))) {
    case ParameterId::BaseColor:
      setBaseColorFactor(toColor(value));
      return true;
    case ParameterId::Metallic:
      setMetallicFactor(value.x);
      return true;
    case ParameterId::Roughness:
      setRoughnessFactor(value.x);
      return true;
    case ParameterId::NormalScale:
      setNormalScale(value.x);
      return true;
    case ParameterId::OcclusionStrength:
      setOcclusionStrength(value.x);
      return true;
    case ParameterId::Emissive:
      setEmissiveFactor(toColor(value));
      return true;
    case ParameterId::AlphaMode:
      setAlphaMode(static_cast<MaterialAlphaMode>(static_cast<uint32>(value.x)));
      return true;
    case ParameterId::AlphaCutoff:
      setAlphaCutoff(value.x);
      return true;
    case ParameterId::TwoSided:
      setTwoSided(value.x != 0.0f);
      return true;
    case ParameterId::COUNT:
      break;
  }
  return false;
}

/*
 */
bool
Material::getValue(const StringView name, Vector4& outValue) const
{
  const uint32 parameter = findParameter(name);
  if (parameter == NOT_FOUND) {
    return false;
  }
  switch (static_cast<ParameterId>(parameter)) {
    case ParameterId::BaseColor:
      outValue = toVector(m_baseColorFactor);
      return true;
    case ParameterId::Metallic:
      outValue = Vector4(m_metallicFactor, 0.0f, 0.0f, 0.0f);
      return true;
    case ParameterId::Roughness:
      outValue = Vector4(m_roughnessFactor, 0.0f, 0.0f, 0.0f);
      return true;
    case ParameterId::NormalScale:
      outValue = Vector4(m_normalScale, 0.0f, 0.0f, 0.0f);
      return true;
    case ParameterId::OcclusionStrength:
      outValue = Vector4(m_occlusionStrength, 0.0f, 0.0f, 0.0f);
      return true;
    case ParameterId::Emissive:
      outValue = toVector(m_emissiveFactor);
      return true;
    case ParameterId::AlphaMode:
      outValue = Vector4(static_cast<float>(m_alphaMode), 0.0f, 0.0f, 0.0f);
      return true;
    case ParameterId::AlphaCutoff:
      outValue = Vector4(m_alphaCutoff, 0.0f, 0.0f, 0.0f);
      return true;
    case ParameterId::TwoSided:
      outValue = Vector4(m_bTwoSided ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
      return true;
    case ParameterId::COUNT:
      break;
  }
  return false;
}

/*
 */
bool
Material::isValidValue(const StringView name, const Vector4& value) noexcept
{
  const uint32 parameter = findParameter(name);
  if (parameter == NOT_FOUND || !isFinite(value)) {
    return false;
  }
  if (static_cast<ParameterId>(parameter) == ParameterId::AlphaMode) {
    // Checked as a float: a negative or huge value cannot become a uint32.
    return value.x >= 0.0f && value.x < static_cast<float>(MaterialAlphaMode::COUNT) &&
           Math::floor(value.x) == value.x;
  }
  return true;
}

/*
 */
Span<const MaterialParameterInfo>
Material::getParameters() noexcept
{
  return kParameters;
}

/*
 */
uint32
Material::findParameter(const StringView name) noexcept
{
  for (uint32 i = 0; i < static_cast<uint32>(ParameterId::COUNT); ++i) {
    if (name == kParameters[i].name) {
      return i;
    }
  }
  return NOT_FOUND;
}

/*
 */
const ANSICHAR*
Material::getTextureName(const MaterialTexture slot) noexcept
{
  return slot < MaterialTexture::COUNT ? kTextureNames[static_cast<uint32>(slot)] : "";
}

/*
 */
MaterialTexture
Material::findTexture(const StringView name) noexcept
{
  for (uint32 i = 0; i < kTextureCount; ++i) {
    if (name == kTextureNames[i]) {
      return static_cast<MaterialTexture>(i);
    }
  }
  return MaterialTexture::COUNT;
}

/*
 */
const IBuffer&
Material::getConstantBuffer() const
{
  if (!m_constantBuffer) {
    m_constantBuffer =
        IGraphicsAPI::instance().createBuffer({.size = sizeof(MaterialConstants),
                                               .usage = BufferUsage::UniformBuffer,
                                               .memoryUsage = MemoryUsage::GpuOnly});
    m_bConstantsDirty = true;
  }
  if (m_bConstantsDirty) {
    updateConstantBuffer();
  }
  return *m_constantBuffer;
}

/*
 */
void
Material::updateConstantBuffer() const
{
  uint32 textureIndexes[kTextureCount];
  for (uint32 i = 0; i < kTextureCount; ++i) {
    const ITexture* texture = m_textures[i].gpuTexture;
    textureIndexes[i] = texture ? texture->getBindlessIndex() : kNoTexture;
  }

  const MaterialConstants constants{
      .baseColorFactor = {m_baseColorFactor.r, m_baseColorFactor.g, m_baseColorFactor.b,
                          m_baseColorFactor.a},
      .emissiveFactor = {m_emissiveFactor.r, m_emissiveFactor.g, m_emissiveFactor.b},
      .metallicFactor = m_metallicFactor,
      .roughnessFactor = m_roughnessFactor,
      .normalScale = m_normalScale,
      .occlusionStrength = m_occlusionStrength,
      .baseColorTexture = textureIndexes[static_cast<uint32>(MaterialTexture::BaseColor)],
      .metallicRoughnessTexture =
          textureIndexes[static_cast<uint32>(MaterialTexture::MetallicRoughness)],
      .normalTexture = textureIndexes[static_cast<uint32>(MaterialTexture::Normal)],
      .emissiveTexture = textureIndexes[static_cast<uint32>(MaterialTexture::Emissive)],
      .occlusionTexture = textureIndexes[static_cast<uint32>(MaterialTexture::Occlusion)],
      .alphaCutoff = m_alphaCutoff,
      .padding = {0.0f, 0.0f, 0.0f}};
  m_constantBuffer->update(&constants, sizeof(constants));
  m_bConstantsDirty = false;
}

/*
 */
void
Material::collectReferences(Vector<UUID>& outReferences) const
{
  for (const TextureSlot& textureSlot : m_textures) {
    if (!textureSlot.id.isNull()) {
      outReferences.push_back(textureSlot.id);
    }
  }
}

/*
 */
void
Material::serialize(DataStream& stream) const
{
  const uint32 version = SERIALIZATION_VERSION;
  MaterialFileData data{
      .baseColorFactor = {m_baseColorFactor.r, m_baseColorFactor.g, m_baseColorFactor.b,
                          m_baseColorFactor.a},
      .emissiveFactor = {m_emissiveFactor.r, m_emissiveFactor.g, m_emissiveFactor.b},
      .metallicFactor = m_metallicFactor,
      .roughnessFactor = m_roughnessFactor,
      .normalScale = m_normalScale,
      .occlusionStrength = m_occlusionStrength,
      .textures = {},
      .alphaCutoff = m_alphaCutoff,
      .alphaMode = static_cast<uint32>(m_alphaMode),
      .twoSided = m_bTwoSided ? 1u : 0u};
  for (uint32 i = 0; i < kTextureCount; ++i) {
    data.textures[i] = m_textures[i].id;
  }
  stream.write(&version, sizeof(version));
  stream.write(&data, sizeof(data));
}

/*
 */
bool
Material::deserialize(DataStream& stream)
{
  uint32 version = 0;
  MaterialFileData data;
  if (stream.read(&version, sizeof(version)) != sizeof(version) ||
      version != SERIALIZATION_VERSION || stream.read(&data, sizeof(data)) != sizeof(data) ||
      data.alphaMode >= static_cast<uint32>(MaterialAlphaMode::COUNT)) {
    return false;
  }

  const float* baseColor = data.baseColorFactor;
  const float* emissive = data.emissiveFactor;
  m_baseColorFactor = LinearColor(baseColor[0], baseColor[1], baseColor[2], baseColor[3]);
  m_emissiveFactor = LinearColor(emissive[0], emissive[1], emissive[2], 1.0f);
  m_metallicFactor = data.metallicFactor;
  m_roughnessFactor = data.roughnessFactor;
  m_normalScale = data.normalScale;
  m_occlusionStrength = data.occlusionStrength;
  m_alphaCutoff = data.alphaCutoff;
  m_alphaMode = static_cast<MaterialAlphaMode>(data.alphaMode);
  m_bTwoSided = data.twoSided != 0;
  for (uint32 i = 0; i < kTextureCount; ++i) {
    setTexture(static_cast<MaterialTexture>(i), data.textures[i], nullptr);
  }
  m_bConstantsDirty = true;
  return true;
}

} // namespace chEngineSDK
