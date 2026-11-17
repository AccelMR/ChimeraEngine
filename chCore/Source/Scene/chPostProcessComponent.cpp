/************************************************************************/
/**
 * @file chPostProcessComponent.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Component that runs project shaders over the rendered image.
 */
/************************************************************************/
#include "chPostProcessComponent.h"

#include "chAssetFile.h"
#include "chAssetManager.h"
#include "chFileStream.h"
#include "chGameObject.h"
#include "chMath.h"
#include "chScene.h"
#include "chSceneManager.h"
#include "chShaderAsset.h"

namespace chEngineSDK {

namespace {
struct PostProcessFileData
{
  int32 priority = 0;
  uint32 passCount = 0;
};
static_assert(sizeof(PostProcessFileData) == 8, "PostProcessFileData must have no padding");

// Written per pass, then its overrides as a name and four floats each.
struct PassFileData
{
  UUID shader;
  uint32 enabled = 1;
  float weight = 1.0f;
  uint32 overrideCount = 0;
  uint32 reserved = 0;
};
static_assert(sizeof(PassFileData) == 32, "PassFileData must have no padding");

// Above what any shader declares, so values a shader no longer has are still kept.
constexpr uint32 kMaxOverrides = ShaderParameters::MAX_PARAMETERS * 4;

NODISCARD SPtr<ShaderAsset>
findShader(const UUID& id)
{
  if (id.isNull() || !AssetManager::isStarted()) {
    return nullptr;
  }
  const SPtr<IAsset> asset = AssetManager::instance().getAsset(id);
  return asset ? asset->as<ShaderAsset>() : nullptr;
}

/**
 * Shaders given at run time load here, so a pass never waits for a scene load to draw.
 */
void
loadShader(const SPtr<ShaderAsset>& shader)
{
  if (shader && !shader->isLoaded() && AssetManager::isStarted()) {
    static_cast<void>(AssetManager::instance().syncLoadAsset(shader));
  }
}
} // namespace

/*
 */
uint32
PostProcessComponent::addPass(const SPtr<ShaderAsset>& shader)
{
  if (m_passes.size() >= MAX_PASSES) {
    CH_LOG_WARNING(SceneManagerLog, "A post-process component holds at most {0} passes.",
                   MAX_PASSES);
    return NOT_FOUND;
  }
  loadShader(shader);
  PassState& state = m_passes.emplace_back();
  state.pass.shader = shader;
  state.pass.shaderId = shader ? shader->getUUID() : UUID::null();
  return static_cast<uint32>(m_passes.size() - 1);
}

/*
 */
void
PostProcessComponent::removePass(const uint32 index)
{
  if (index < m_passes.size()) {
    m_passes.erase(m_passes.begin() + index);
  }
}

/*
 */
void
PostProcessComponent::movePass(const uint32 from, const uint32 to)
{
  if (from >= m_passes.size() || to >= m_passes.size() || from == to) {
    return;
  }
  PassState moved = std::move(m_passes[from]);
  m_passes.erase(m_passes.begin() + from);
  m_passes.insert(m_passes.begin() + to, std::move(moved));
}

/*
 */
void
PostProcessComponent::setPassShader(const uint32 index, const SPtr<ShaderAsset>& shader)
{
  if (index >= m_passes.size()) {
    return;
  }
  loadShader(shader);
  PassState& state = m_passes[index];
  state.pass.shader = shader;
  state.pass.shaderId = shader ? shader->getUUID() : UUID::null();
  state.bDirty = true;
}

/*
 */
void
PostProcessComponent::setPassEnabled(const uint32 index, const bool bEnabled)
{
  if (index < m_passes.size()) {
    m_passes[index].pass.bEnabled = bEnabled;
  }
}

/*
 */
void
PostProcessComponent::setPassWeight(const uint32 index, const float weight)
{
  if (index < m_passes.size() && Math::isFinite(weight)) {
    m_passes[index].pass.weight = Math::clamp(weight, 0.0f, 1.0f);
  }
}

/*
 */
uint32
PostProcessComponent::findPass(const StringView shaderName) const noexcept
{
  for (SIZE_T i = 0; i < m_passes.size(); ++i) {
    const SPtr<ShaderAsset>& shader = m_passes[i].pass.shader;
    if (shader && StringView(shader->getName()) == shaderName) {
      return static_cast<uint32>(i);
    }
  }
  return NOT_FOUND;
}

/*
 */
bool
PostProcessComponent::setValue(const uint32 index, const StringView name, const Vector4& value)
{
  if (index >= m_passes.size() || !Math::isFinite(value.x) || !Math::isFinite(value.y) ||
      !Math::isFinite(value.z) || !Math::isFinite(value.w)) {
    return false;
  }
  PassState& state = m_passes[index];
  const SPtr<ShaderAsset>& shader = state.pass.shader;
  if (!shader || shader->findParameter(name) == ShaderAsset::NOT_FOUND) {
    return false;
  }
  state.bDirty = true;
  for (PostProcessOverride& entry : state.pass.overrides) {
    if (entry.name == name) {
      entry.value = value;
      return true;
    }
  }
  state.pass.overrides.push_back({.name = String(name), .value = value});
  return true;
}

/*
 */
void
PostProcessComponent::resetValue(const uint32 index, const StringView name)
{
  if (index >= m_passes.size()) {
    return;
  }
  PassState& state = m_passes[index];
  Vector<PostProcessOverride>& overrides = state.pass.overrides;
  for (SIZE_T i = 0; i < overrides.size(); ++i) {
    if (overrides[i].name == name) {
      overrides.erase(overrides.begin() + i);
      state.bDirty = true;
      return;
    }
  }
}

/*
 */
bool
PostProcessComponent::getValue(const uint32 index,
                               const StringView name,
                               Vector4& outValue) const
{
  if (index >= m_passes.size()) {
    return false;
  }
  const SPtr<ShaderAsset>& shader = m_passes[index].pass.shader;
  const uint32 parameter = shader ? shader->findParameter(name) : ShaderAsset::NOT_FOUND;
  if (parameter == ShaderAsset::NOT_FOUND) {
    return false;
  }
  const PostProcessOverride* entry = findOverride(index, name);
  outValue = entry ? entry->value : shader->getParameters()[parameter].defaultValue;
  return true;
}

/*
 */
const PostProcessOverride*
PostProcessComponent::findOverride(const uint32 index, const StringView name) const noexcept
{
  if (index >= m_passes.size()) {
    return nullptr;
  }
  for (const PostProcessOverride& entry : m_passes[index].pass.overrides) {
    if (entry.name == name) {
      return &entry;
    }
  }
  return nullptr;
}

/*
 */
void
PostProcessComponent::setPriority(const int32 priority)
{
  m_priority = priority;
  if (isRegistered()) {
    getOwner()->getScene()->addPostProcess(*this);
  }
}

/*
 */
void
PostProcessComponent::buildParameters(const PassState& state) const
{
  const ShaderAsset& shader = *state.pass.shader;
  // A shader without parameters still reads one slot (its struct is never empty).
  const uint32 size = Math::max(shader.getParameterBufferSize(), ShaderParameters::SLOT_SIZE);
  state.parameterData.assign(size, 0);
  const Span<const ShaderParameter> parameters = shader.getParameters();
  ShaderParameters::writeDefaults(parameters, state.parameterData);
  for (const PostProcessOverride& entry : state.pass.overrides) {
    const uint32 parameter = shader.findParameter(entry.name);
    if (parameter != ShaderAsset::NOT_FOUND) {
      ShaderParameters::writeValue(parameters[parameter], entry.value, state.parameterData);
    }
  }
  state.builtRevision = shader.getRevision();
  state.bDirty = false;
}

/*
 */
void
PostProcessComponent::appendPasses(Vector<PostProcessPassItem>& outPasses,
                                   Vector<uint8>& outParameters) const
{
  for (const PassState& state : m_passes) {
    const PostProcessPass& pass = state.pass;
    ShaderAsset* shader = pass.shader.get();
    // An unloaded shader holds no code.
    if (!pass.bEnabled || pass.weight <= 0.0f || !shader || shader->getData().binaries.empty() ||
        shader->getKind() != ShaderAssetKind::PostProcess) {
      continue;
    }
    if (state.bDirty || state.builtRevision != shader->getRevision()) {
      buildParameters(state);
    }
    outPasses.push_back({.shader = shader,
                         .stage = shader->getStage(),
                         .weight = pass.weight,
                         .parameterOffset = static_cast<uint32>(outParameters.size()),
                         .parameterSize = static_cast<uint32>(state.parameterData.size())});
    outParameters.insert(outParameters.end(), state.parameterData.begin(),
                         state.parameterData.end());
  }
}

/*
 */
void
PostProcessComponent::serialize(DataStream& stream) const
{
  const uint32 version = SERIALIZATION_VERSION;
  const PostProcessFileData data{.priority = m_priority,
                                 .passCount = static_cast<uint32>(m_passes.size())};
  stream.write(&version, sizeof(version));
  stream.write(&data, sizeof(data));
  for (const PassState& state : m_passes) {
    const PostProcessPass& pass = state.pass;
    const PassFileData passData{.shader = pass.shaderId,
                                .enabled = pass.bEnabled ? 1u : 0u,
                                .weight = pass.weight,
                                .overrideCount = static_cast<uint32>(pass.overrides.size()),
                                .reserved = 0};
    stream.write(&passData, sizeof(passData));
    for (const PostProcessOverride& entry : pass.overrides) {
      AssetFile::writeString(stream, entry.name);
      const float value[4] = {entry.value.x, entry.value.y, entry.value.z, entry.value.w};
      stream.write(value, sizeof(value));
    }
  }
}

/*
 */
bool
PostProcessComponent::deserialize(DataStream& stream)
{
  uint32 version = 0;
  PostProcessFileData data;
  if (!AssetFile::readValue(stream, version) || version != SERIALIZATION_VERSION ||
      !AssetFile::readValue(stream, data) || data.passCount > MAX_PASSES) {
    return false;
  }

  Vector<PassState> passes(data.passCount);
  for (PassState& state : passes) {
    PassFileData passData;
    if (!AssetFile::readValue(stream, passData) || passData.overrideCount > kMaxOverrides ||
        !Math::isFinite(passData.weight)) {
      return false;
    }
    PostProcessPass& pass = state.pass;
    pass.shaderId = passData.shader;
    pass.shader = findShader(passData.shader);
    pass.bEnabled = passData.enabled != 0;
    pass.weight = Math::clamp(passData.weight, 0.0f, 1.0f);
    pass.overrides.resize(passData.overrideCount);
    for (PostProcessOverride& entry : pass.overrides) {
      float value[4];
      if (!AssetFile::readString(stream, entry.name, ShaderParameters::MAX_NAME_LENGTH) ||
          !AssetFile::readValue(stream, value)) {
        return false;
      }
      entry.value = Vector4(value[0], value[1], value[2], value[3]);
    }
    if (!pass.shader && !pass.shaderId.isNull()) {
      CH_LOG_WARNING(SceneManagerLog, "A post-process pass lost its shader {0}.",
                     pass.shaderId);
    }
  }
  m_passes = std::move(passes);
  m_priority = data.priority;
  return true;
}

/*
 */
void
PostProcessComponent::collectReferences(Vector<UUID>& outReferences) const
{
  for (const PassState& state : m_passes) {
    if (!state.pass.shaderId.isNull()) {
      outReferences.push_back(state.pass.shaderId);
    }
  }
}

/*
 */
void
PostProcessComponent::onRegister()
{
  getOwner()->getScene()->addPostProcess(*this);
}

/*
 */
void
PostProcessComponent::onUnregister()
{
  getOwner()->getScene()->removePostProcess(*this);
}

} // namespace chEngineSDK
