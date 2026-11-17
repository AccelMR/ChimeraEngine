/************************************************************************/
/**
 * @file chShaderHotReload.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  Compiles project shaders again when their source files change.
 */
/************************************************************************/
#include "chShaderHotReload.h"

#include "chAssetCodecManager.h"
#include "chAssetManager.h"
#include "chConsoleVariable.h"
#include "chFileSystem.h"
#include "chHash.h"
#include "chLogger.h"
#include "chShaderAsset.h"

namespace chEngineSDK {

CH_LOG_DECLARE_STATIC(ShaderHotReloadLog, All);

namespace {
ConsoleVariable<bool> g_cvarShaderHotReload(
    "Editor.ShaderHotReload",
    true,
    "Compiles a project shader again when its source, or a file it includes, changes.",
    "ShaderHotReload");

ConsoleVariable<float> g_cvarShaderReloadInterval(
    "Editor.ShaderReloadInterval",
    0.25f,
    "Seconds between two checks of the source files of the loaded shaders.",
    "ShaderReloadInterval");

struct HotReloadState
{
  // What the source and its includes looked like at the last check, per shader.
  UnorderedMap<UUID, uint64> signatures;
  Vector<SPtr<IAsset>> shaders;
  float timeToCheck = 0.0f;
};

HotReloadState&
getState()
{
  static HotReloadState state;
  return state;
}

/**
 * One number from the write times of the source and of every file it included, so any
 * change shows; 0 when the source cannot be read (it is being saved, or was removed).
 */
NODISCARD uint64
computeSignature(const ShaderAsset& shader)
{
  const int64 sourceTime = FileSystem::getLastWriteTime(shader.getEditorData().sourceFile);
  if (sourceTime == 0) {
    return 0;
  }
  uint64 signature = static_cast<uint64>(sourceTime);
  for (const Path& file : shader.getIncludedFiles()) {
    signature =
        HashUtils::combine(signature, static_cast<uint64>(FileSystem::getLastWriteTime(file)));
  }
  return signature == 0 ? 1 : signature;
}

void
checkShaders(HotReloadState& state)
{
  state.shaders.clear();
  AssetManager::instance().getAssetsOfType(AssetTypeTraits<ShaderAsset>::getTypeId(),
                                           state.shaders);
  for (const SPtr<IAsset>& asset : state.shaders) {
    const SPtr<ShaderAsset> shader = asset->as<ShaderAsset>();
    if (!shader || !shader->isLoaded() || shader->getEditorData().sourceFile.empty()) {
      continue;
    }
    const uint64 signature = computeSignature(*shader);
    if (signature == 0) {
      continue;
    }
    const auto [it, bAdded] = state.signatures.try_emplace(shader->getUUID(), signature);
    if (!bAdded && it->second != signature) {
      it->second = signature;
      static_cast<void>(ShaderHotReload::compileAgain(shader));
    }
  }
  state.shaders.clear();
}
} // namespace

/*
 */
void
ShaderHotReload::shutDown()
{
  HotReloadState& state = getState();
  state.signatures.clear();
  state.shaders.clear();
}

/*
 */
void
ShaderHotReload::update(const float deltaTime)
{
  HotReloadState& state = getState();
  if (!g_cvarShaderHotReload.get() || !AssetManager::isStarted() ||
      !AssetCodecManager::isStarted()) {
    return;
  }
  state.timeToCheck -= deltaTime;
  if (state.timeToCheck > 0.0f) {
    return;
  }
  state.timeToCheck = g_cvarShaderReloadInterval.get();
  checkShaders(state);
}

/*
 */
bool
ShaderHotReload::compileAgain(const SPtr<ShaderAsset>& asset)
{
  if (!asset || asset->getEditorData().sourceFile.empty() || !AssetCodecManager::isStarted()) {
    return false;
  }
  const AssetEditorData& editorData = asset->getEditorData();
  const UniquePtr<IImportTask> task = AssetCodecManager::instance().createImportTask(
      editorData.sourceFile, asset->getName(), editorData.importSettings);
  if (!task) {
    CH_LOG_ERROR(ShaderHotReloadLog, "No codec compiles {0}.", editorData.sourceFile);
    return false;
  }
  // Short enough to run here: a project shader is one small file.
  ImportProgress progress;
  if (!task->readSource(progress)) {
    return false;
  }
  const bool bCompiled = task->createAssets(Path(asset->getAssetPath())) != nullptr;
  getState().signatures[asset->getUUID()] = computeSignature(*asset);
  return bCompiled;
}

} // namespace chEngineSDK
