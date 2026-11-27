/************************************************************************/
/**
 * @file chShapeComponent.h
 * @author AccelMR
 * @date 2026/10/18
 * @brief
 *  Component that draws a simple shape (plane, box, sphere...) at its GameObject.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chComponent.h"
#include "chMeshGenerator.h"
#include "chTypeTraits.h"
#include "chUUID.h"

namespace chEngineSDK {
class DynamicMaterial;
class MaterialInterfaceAsset;

/**
 * Exists to put a simple shape in a scene without an asset file: it keeps the shape's sizes
 * and one material, and computes the mesh with MeshGenerator. Components with the same
 * sizes share one mesh, so many equal boxes cost one vertex buffer. While registered it
 * adds one render item to the scene and moves it with the GameObject.
 */
class CH_CORE_EXPORT ShapeComponent : public Component
{
 public:
  ShapeComponent() = default;

  explicit ShapeComponent(const ShapeDesc& shape);

  /**
   * Values outside the generator's ranges are clamped.
   */
  void
  setShape(const ShapeDesc& shape);

  NODISCARD FORCEINLINE const ShapeDesc&
  getShape() const noexcept { return m_shape; }

  /**
   * Built on first use, so a component read from a file builds it only once.
   */
  NODISCARD const SPtr<Mesh>&
  getMesh();

  /**
   * Null draws with the renderer's default material. Drops the dynamic material.
   */
  void
  setMaterial(const SPtr<MaterialInterfaceAsset>& material);

  /**
   * Draws with a material made at run time, which may be shared with other objects; null
   * goes back to the asset. Not saved.
   */
  void
  setDynamicMaterial(const SPtr<DynamicMaterial>& material);

  NODISCARD FORCEINLINE const SPtr<DynamicMaterial>&
  getDynamicMaterial() const noexcept { return m_dynamicMaterial; }

  /**
   * A dynamic material only this object uses, made the first time from the material (a copy
   * when it had a shared dynamic one), so changing it changes no other object.
   */
  DynamicMaterial&
  getOwnDynamicMaterial();

  NODISCARD FORCEINLINE const SPtr<MaterialInterfaceAsset>&
  getMaterial() const noexcept { return m_material; }

  /**
   * The material asked for, kept even when the asset is missing, so saving again keeps it.
   */
  NODISCARD FORCEINLINE const UUID&
  getMaterialId() const noexcept { return m_materialId; }

  NODISCARD bool
  getWorldBounds(AABox& outBounds) const override;

  void
  serialize(DataStream& stream) const override;

  NODISCARD bool
  deserialize(DataStream& stream) override;

  void
  collectReferences(Vector<UUID>& outReferences) const override;

  static constexpr uint32 SERIALIZATION_VERSION = 1;

 protected:
  void
  onRegister() override;

  void
  onUnregister() override;

  void
  onTransformChanged() override;

 private:
  void
  updateRenderItem();

  ShapeDesc m_shape;
  SPtr<Mesh> m_mesh;
  SPtr<MaterialInterfaceAsset> m_material;
  UUID m_materialId = UUID::null();
  SPtr<DynamicMaterial> m_dynamicMaterial;
  bool m_bOwnsDynamicMaterial = false;
  uint32 m_renderItemId = 0;
};

DECLARE_TYPE_TRAITS(ShapeComponent)

} // namespace chEngineSDK
