/************************************************************************/
/**
 * @file chAssetManager.h
 * @author AccelMR
 * @date 2025/04/26
 * @brief
 * AssetManager class for managing assets in the engine.
 * It handles loading, unloading, and saving assets.
 * It also provides functionality for managing asset references and states.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chAssetManifest.h"
#include "chAssetRegister.h"
#include "chEnginePaths.h"
#include "chEventSystem.h"
#include "chFileSystem.h"
#include "chIAsset.h"
#include "chLogger.h"
#include "chModelAsset.h"
#include "chModule.h"
#include "chStringUtils.h"

#include "chSceneAsset.h"

namespace chEngineSDK {

CH_LOG_DECLARE_EXTERN(CH_CORE_EXPORT, AssetSystem);

class CH_CORE_EXPORT AssetManager : public Module<AssetManager>
{
 public:
  /**
   * Default constructor
   */
  AssetManager();

  /**
   * Destructor
   */
  // Waits for the reads still running, which point into this manager.
  ~AssetManager();

  void
  initialize();

  bool
  syncLoadAsset(const SPtr<IAsset>& asset);

  bool
  unloadAsset(const SPtr<IAsset>& asset);

  bool
  syncLoadAsset(const UUID& assetUUID);

  bool
  unloadAsset(const UUID& assetUUID);

  /**
   * The asset with this UUID, loaded or not, or null when there is none.
   */
  NODISCARD SPtr<IAsset>
  getAsset(const UUID& assetUUID) const;

  /**
   * Loads the asset and what it references without stopping the frame: the files are read
   * on Background jobs and update() finishes each asset on the main thread, references
   * first, within a time and size budget per frame. True when the asset is loaded or on its
   * way; false when it is missing or failed before. syncLoadAsset on an asset on its way
   * finishes it at once.
   */
  bool
  requestLoad(const SPtr<IAsset>& asset);

  bool
  requestLoad(const UUID& assetUUID);

  NODISCARD bool
  isLoadPending(const UUID& assetUUID) const;

  NODISCARD FORCEINLINE SIZE_T
  getPendingLoadCount() const noexcept
  {
    return m_pendingOrder.size();
  }

  /**
   * Finishes the requested loads whose files were read and whose references are loaded,
   * until Asset.LoadBudgetMs passed or Asset.UploadBudgetMB of data went through (at least
   * one asset always finishes). The application calls it once a frame.
   */
  void
  update();

  /**
   * Registers every asset under directory without loading it. For a mount root ("/Game")
   * whose every layer has an AssetManifest (packaged games), the manifests are read instead
   * of the files: layers mounted over others replace assets with the same UUID.
   */
  void
  lazyLoadAssetsFromDirectory(const Path& directory);

  NODISCARD bool
  saveAsset(const SPtr<IAsset>& asset){
    if (!asset) {
      CH_LOG_ERROR(AssetSystem, "Cannot save null asset");
      return false;
    }
    if (!asset->save()) {
      CH_LOG_ERROR(AssetSystem, "Failed to save asset {0}", asset->getName());
      return false;
    }
    CH_LOG_DEBUG(AssetSystem, "Saved asset {0}", asset->getName());
    return true;
  }

  /**
   * The scene asset whose file is "<path>.chAss" for a virtual path such as
   * /Game/Maps/Main, or the first one with that name for a text without '/'. Null when
   * there is none. Walks every asset.
   */
  NODISCARD SPtr<SceneAsset>
  findScene(StringView pathOrName) const;

  /**
   * Same as findScene for any asset type.
   */
  NODISCARD SPtr<IAsset>
  findAsset(const UUID& typeId, StringView pathOrName) const;

  /**
   * Appends every asset of that type to outAssets. Walks every asset.
   */
  void
  getAssetsOfType(const UUID& typeId, Vector<SPtr<IAsset>>& outAssets) const;

#if USING(CH_EDITOR)
  bool
  renameAsset(const UUID& assetUUID, const ANSICHAR* newName);

  bool
  renameAsset(const SPtr<IAsset>& asset, const ANSICHAR* newName);

  bool
  removeAsset(const UUID& assetUUID);

  /**
   * The asset imported from this source file and part (as stored in its editor data), or
   * null. Lets an import reuse what an earlier one made instead of making a copy. Walks
   * every asset.
   */
  NODISCARD SPtr<IAsset>
  findAssetBySource(const Path& sourceFile, StringView sourcePart) const;

  /**
   * The name, cut to fit the metadata, plus "_1", "_2"... when an asset in that folder (in
   * memory or on disk) already has it.
   */
  NODISCARD String
  makeUniqueAssetName(const Path& folder, StringView name) const;

  /**
   * Moves the asset file into another virtual folder. Assets reference each other by UUID,
   * so nothing that uses it needs to change.
   */
  bool
  moveAsset(const SPtr<IAsset>& asset, const Path& folder);

  /**
   * Renames or moves a folder under /Game with everything in it, and updates the folder of
   * every asset inside.
   */
  bool
  moveFolder(const Path& folder, const Path& newFolder);

  /**
   * Deletes a folder under /Game with everything in it and forgets the assets that were
   * inside. Refused while one of them is loaded, since something may still use it.
   */
  bool
  removeFolder(const Path& folder);
#endif // Editor-specific functionality

  /*
   *
   */
  template <typename TAsset = IAsset>
  WeakPtr<TAsset>
  createAsset(const String& name,
              const Path& assetPath); // Must be relative to the asset directory

  NODISCARD Vector<SPtr<IAsset>>
  getAllAssets() const {
    Vector<SPtr<IAsset>> assets;
    assets.reserve(m_assets.size());
    for (const auto& pair : m_assets) {
      assets.push_back(pair.second);
    }
    return assets;
  }

  NODISCARD FORCEINLINE const String&
  getAssetTypeName(const UUID& assetUUID) const {
    CH_ASSERT(m_assetRegister && "AssetRegister must be initialized before accessing asset types.");
    return m_assetRegister->getAssetTypeName(assetUUID);
  }

 private:
  friend class IAssetCodec;

  void
  registerNewAsset(const SPtr<IAsset>& asset) {
    CH_ASSERT(asset && "Asset cannot be null");
    m_assets[asset->getUUID()] = asset;
  }

  /**
   * Reads only the start of the file; the name and the folder come from where it is.
   */
  SPtr<IAsset>
  lazyLoadAssetFile(const Path& file);

  /**
   * A script is its .lua file; its UUID is the first line of the file.
   */
  SPtr<IAsset>
  lazyLoadScriptFile(const Path& file);

  struct PendingLoad;

  /**
   * Waits for the pending load's read and finishes it, and the pending loads it references
   * first. Removes it from the pending loads.
   */
  bool
  finishPendingNow(const UUID& assetUUID);

  bool
  finishPending(PendingLoad& pending);

  NODISCARD bool
  areReferencesReady(const PendingLoad& pending) const;

  /**
   * False, having registered nothing, when directory is not a mount root or one of its
   * layers has no readable manifest.
   */
  bool
  lazyLoadFromManifests(const Path& directory);

  /**
   * An asset of a manifest entry, its file at mountRoot/entry.path; null when the entry
   * names no known type.
   */
  SPtr<IAsset>
  createFromManifestEntry(const AssetManifestEntry& entry, StringView mountRoot);

 private:
  // Requested loads by asset, and their UUIDs in the order they may finish (references
  // before what references them).
  UnorderedMap<UUID, UniquePtr<PendingLoad>> m_pendingLoads;
  Vector<UUID> m_pendingOrder;

  Map<UUID, SPtr<IAsset>> m_assets; ///< Map of all assets by UUID, both loaded and unloaded
  Map<UUID, SPtr<IAsset>> m_loadedAssets; ///< Map of currently loaded assets by UUID

  Event<bool(const SPtr<IAsset>&)> m_onAssetLoaded; ///< Event triggered when an asset is loaded
  Event<bool(const SPtr<IAsset>&)> m_onAssetUnloaded; ///< Event triggered

  SPtr<AssetRegister> m_assetRegister; ///< Asset registry for creating assets
}; // class AssetManager

/******************************************************************************************* */

/*
 */
template <typename TAsset>
WeakPtr<TAsset>
AssetManager::createAsset(const String& name, const Path& assetPath) {

  bool validationPassed = true;
  auto validateInfo = [&validationPassed](bool condition, const String& errorMessage) {
    if (!condition) {
      CH_LOG(AssetSystem, Error, errorMessage);
      validationPassed = false;
    }
  };
  validateInfo(!name.empty(), "Asset name cannot be empty");
  validateInfo(!assetPath.empty(), "Asset path cannot be empty");
  validateInfo(FileSystem::exists(assetPath),
                "Asset path does not exist: " + assetPath.toString());
  validateInfo(FileSystem::isSubPath(EnginePaths::getGameAssetDirectory(), assetPath),
                StringUtils::format("Asset path must be relative to the asset directory: {0}",
                                    EnginePaths::getGameAssetDirectory().toString()));
  if (!validationPassed) {
    CH_LOG(AssetSystem, Error, "Failed to create asset due to validation errors");
    return WeakPtr<TAsset>();
  }
  const UUID assetUUID = AssetTypeTraits<TAsset>::getTypeId();
  AssetCreatorFunc assetCreator = m_assetRegister->getAssetCreator(assetUUID);
  if (assetCreator == nullptr) {
    CH_LOG(AssetSystem, Error, "No asset creator found for UUID: {0}", assetUUID.toString());
    return WeakPtr<TAsset>();
  }

  const String AssetTypeName = AssetTypeTraits<TAsset>::getTypeName();

  const UUID refUUID = UUID::createRandom();
  AssetMetadata metadata;
  metadata.uuid = refUUID;
  metadata.assetType = assetUUID;
  StringUtils::copyToBuffer(metadata.name, name);
  StringUtils::copyToBuffer(metadata.assetPath, assetPath.toString());

  SPtr<IAsset> asset = assetCreator(metadata);

  if (!asset) {
    CH_LOG_ERROR(AssetSystem, "Failed to create asset of type {0} with UUID {1}",
                 AssetTypeName, assetUUID.toString());
    return WeakPtr<TAsset>();
  }

  CH_LOG(AssetSystem, Debug, "Asset {0} created with UUID {1} at path {2}", name,
         refUUID.toString(), assetPath.toString());

  // Its data is the default made in memory, so loading it from a file that does not exist
  // yet must not be tried.
  asset->m_state = AssetState::Loaded;
#if USING(CH_EDITOR)
  asset->m_editorData = AssetMeta::makeNew(Path(), StringView(), ImportSettings{});
#endif // USING(CH_EDITOR)

  // DeleteMe
  m_loadedAssets[refUUID] = asset;
  m_assets[refUUID] = asset;

  return std::static_pointer_cast<TAsset>(asset);
}

} // namespace chEngineSDK
