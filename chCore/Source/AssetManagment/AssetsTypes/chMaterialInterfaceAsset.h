/************************************************************************/
/**
 * @file chMaterialInterfaceAsset.h
 * @author AccelMR
 * @date 2026/11/27
 * @brief
 *  Base of every asset that gives a surface a Material: materials and their instances.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chIAsset.h"
#include "chMaterial.h"

namespace chEngineSDK {

/**
 * Told when the material it follows changed its values, so it can lay its own changes over
 * them again. Material instances use it; the listener must remove itself before it dies.
 */
class CH_CORE_EXPORT IMaterialListener
{
 public:
  virtual void
  onParentMaterialChanged() = 0;

 protected:
  ~IMaterialListener() = default;
};

/**
 * Exists so models, shapes and scripts take any asset that gives a Material without caring
 * whether it is a material or an instance of one. The Material is kept by value and never
 * replaced (loading, unloading and parent changes write its contents), so render items can
 * point at it while the asset lives.
 */
class CH_CORE_EXPORT MaterialInterfaceAsset : public IAsset
{
 public:
  MaterialInterfaceAsset() = delete;

  explicit MaterialInterfaceAsset(const AssetMetadata& metadata) : IAsset(metadata) {}

  ~MaterialInterfaceAsset() override;

  /**
   * The values the renderer draws with (an instance's include its parent's).
   */
  NODISCARD FORCEINLINE const Material&
  getMaterial() const noexcept
  {
    return m_material;
  }

  /**
   * Null for a material; the parent of an instance.
   */
  NODISCARD virtual SPtr<MaterialInterfaceAsset>
  getParentMaterial() const;

  /**
   * Main thread only. A listener is never added twice.
   */
  void
  addListener(IMaterialListener& listener);

  void
  removeListener(IMaterialListener& listener);

  /**
   * Tells every listener that the Material changed. Call it after editing a material in
   * place; loading, unloading and instance edits call it themselves.
   */
  void
  notifyMaterialChanged();

  /**
   * The asset as a material or instance, or null for any other type.
   */
  NODISCARD static SPtr<MaterialInterfaceAsset>
  fromAsset(const SPtr<IAsset>& asset);

  /**
   * True for a material or an instance of one.
   */
  NODISCARD static bool
  isMaterialType(const UUID& assetType);

 protected:
  Material m_material;

 private:
  Vector<IMaterialListener*> m_listeners;
};

} // namespace chEngineSDK
