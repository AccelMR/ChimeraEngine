/************************************************************************/
/**
 * @file chScriptMaterialBindings.cpp
 * @author AccelMR
 * @date 2026/11/27
 * @brief
 *  Materials and their instances in scripts, and the Model and Shape handles that change
 *  what an object draws with.
 */
/************************************************************************/
#include "chScriptBindings.h"

#include <new>

#include "chAssetManager.h"
#include "chDynamicMaterial.h"
#include "chGameObject.h"
#include "chMaterialAsset.h"
#include "chMaterialInstanceAsset.h"
#include "chModel.h"
#include "chModelComponent.h"
#include "chPath.h"
#include "chScriptMathTypes.h"
#include "chScriptObjectTypes.h"
#include "chShapeComponent.h"
#include "chStringUtils.h"
#include "chTextureAsset.h"

namespace chEngineSDK {
CH_SCRIPT_HANDLE_TYPE(DynamicMaterial, "MaterialInstance");
CH_SCRIPT_HANDLE_TYPE(ModelComponent, "Model");
CH_SCRIPT_HANDLE_TYPE(ShapeComponent, "Shape");

/**
 * A MaterialInstance is a userdata owning one reference to a DynamicMaterial, released by
 * __gc, so scripts and components share it for as long as either holds it.
 */
template<>
struct ScriptTraits<DynamicMaterial>
{
  using Reference = SPtr<DynamicMaterial>;

  static void
  push(lua_State* state, const Reference& material)
  {
    void* memory = lua_newuserdatauv(state, sizeof(Reference), 0);
    new (memory) Reference(material);
    luaL_setmetatable(state, ScriptHandleType<DynamicMaterial>::NAME);
  }

  NODISCARD static const Reference&
  checkReference(lua_State* state, const int32 index)
  {
    return *static_cast<Reference*>(
        luaL_checkudata(state, index, ScriptHandleType<DynamicMaterial>::NAME));
  }

  NODISCARD static const Reference*
  testReference(lua_State* state, const int32 index)
  {
    return static_cast<Reference*>(
        luaL_testudata(state, index, ScriptHandleType<DynamicMaterial>::NAME));
  }

  NODISCARD static DynamicMaterial&
  check(lua_State* state, const int32 index)
  {
    return *checkReference(state, index);
  }
};

/**
 * The model of a GameObject: a part handle that finds the component each time it is used.
 */
template<>
struct ScriptTraits<ModelComponent>
{
  NODISCARD static ModelComponent&
  check(lua_State* state, const int32 index)
  {
    GameObject& gameObject =
        ScriptPartHandle::checkOwner(state, index, ScriptHandleType<ModelComponent>::NAME);
    ModelComponent* model = gameObject.getComponent<ModelComponent>();
    if (!model) {
      luaL_error(state, "GameObject '%s' has no Model", gameObject.getName().c_str());
    }
    return *model;
  }
};

/**
 * The shape of a GameObject, found the same way.
 */
template<>
struct ScriptTraits<ShapeComponent>
{
  NODISCARD static ShapeComponent&
  check(lua_State* state, const int32 index)
  {
    GameObject& gameObject =
        ScriptPartHandle::checkOwner(state, index, ScriptHandleType<ShapeComponent>::NAME);
    ShapeComponent* shape = gameObject.getComponent<ShapeComponent>();
    if (!shape) {
      luaL_error(state, "GameObject '%s' has no Shape", gameObject.getName().c_str());
    }
    return *shape;
  }
};

namespace {
// In MaterialAlphaMode order.
constexpr const ANSICHAR* kAlphaModeNames[] = {"Opaque", "Mask"};
static_assert(sizeof(kAlphaModeNames) / sizeof(kAlphaModeNames[0]) ==
              static_cast<SIZE_T>(MaterialAlphaMode::COUNT));

/*
 * What an object draws with when it has no material.
 */
const Material&
getDefaultMaterial()
{
  static const Material material;
  return material;
}

/*
 * The asset of a Material value, or null when it no longer exists.
 */
SPtr<MaterialInterfaceAsset>
findMaterialAsset(const UUID& id)
{
  if (id.isNull() || !AssetManager::isStarted()) {
    return nullptr;
  }
  return MaterialInterfaceAsset::fromAsset(AssetManager::instance().getAsset(id));
}

/*
 * A material or instance by path or name; null when there is none. Walks every asset.
 */
SPtr<MaterialInterfaceAsset>
findMaterialByText(const StringView text)
{
  if (!AssetManager::isStarted()) {
    return nullptr;
  }
  const AssetManager& assetManager = AssetManager::instance();
  SPtr<IAsset> asset = assetManager.findAsset(AssetTypeTraits<MaterialAsset>::getTypeId(), text);
  if (!asset) {
    asset = assetManager.findAsset(AssetTypeTraits<MaterialInstanceAsset>::getTypeId(), text);
  }
  return MaterialInterfaceAsset::fromAsset(asset);
}

/*
 * The material at index, loaded: a Material value or a path or name. Raises when there is
 * none.
 */
SPtr<MaterialInterfaceAsset>
checkMaterialAsset(lua_State* state, const int32 index, const ANSICHAR* function)
{
  if (!AssetManager::isStarted()) {
    luaL_error(state, "%s: there are no assets to search", function);
  }
  SPtr<MaterialInterfaceAsset> material;
  if (const auto* value = static_cast<const ScriptMaterial*>(
          luaL_testudata(state, index, ScriptValueType<ScriptMaterial>::NAME))) {
    material = findMaterialAsset(value->material);
    if (!material) {
      luaL_error(state, "%s: the material was deleted", function);
    }
  }
  else {
    const StringView text = ScriptTraits<StringView>::check(state, index);
    material = findMaterialByText(text);
    if (!material) {
      luaL_error(state, "%s: there is no material %s", function, String(text).c_str());
    }
  }
  if (!material->isLoaded() && !AssetManager::instance().syncLoadAsset(material)) {
    luaL_error(state, "%s: material %s could not be loaded", function, material->getName());
  }
  return material;
}

/*
 * The texture at index, loaded: nil (no texture), a Texture value or a path or name.
 */
SPtr<TextureAsset>
checkTexture(lua_State* state, const int32 index, const ANSICHAR* function)
{
  if (lua_isnoneornil(state, index)) {
    return nullptr;
  }
  if (!AssetManager::isStarted()) {
    luaL_error(state, "%s: there are no assets to search", function);
  }
  AssetManager& assetManager = AssetManager::instance();
  SPtr<IAsset> asset;
  if (const auto* value = static_cast<const ScriptTexture*>(
          luaL_testudata(state, index, ScriptValueType<ScriptTexture>::NAME))) {
    asset = assetManager.getAsset(value->texture);
  }
  else {
    asset = assetManager.findAsset(AssetTypeTraits<TextureAsset>::getTypeId(),
                                   ScriptTraits<StringView>::check(state, index));
  }
  SPtr<TextureAsset> texture = asset ? asset->as<TextureAsset>() : nullptr;
  if (!texture) {
    luaL_error(state, "%s: there is no such texture", function);
  }
  if (!texture->isLoaded() && !assetManager.syncLoadAsset(texture)) {
    luaL_error(state, "%s: texture %s could not be loaded", function, texture->getName());
  }
  return texture;
}

/*
 * The name at index as a Material value; raises with the names it takes.
 */
const MaterialParameterInfo&
checkValueName(lua_State* state, const int32 index, const ANSICHAR* function)
{
  const StringView name = ScriptTraits<StringView>::check(state, index);
  const uint32 parameter = Material::findParameter(name);
  if (parameter == Material::NOT_FOUND) {
    luaL_error(state,
               "%s: '%s' is not a material value (baseColor, metallic, roughness, emissive, "
               "normalScale, occlusionStrength, alphaMode, alphaCutoff, twoSided)",
               function, String(name).c_str());
  }
  return Material::getParameters()[parameter];
}

/*
 * The name at index as a texture slot; raises with the names it takes.
 */
MaterialTexture
checkTextureName(lua_State* state, const int32 index, const ANSICHAR* function)
{
  const StringView name = ScriptTraits<StringView>::check(state, index);
  const MaterialTexture slot = Material::findTexture(name);
  if (slot == MaterialTexture::COUNT) {
    luaL_error(state,
               "%s: '%s' is not a material texture (baseColorTexture, "
               "metallicRoughnessTexture, normalTexture, emissiveTexture, occlusionTexture)",
               function, String(name).c_str());
  }
  return slot;
}

/*
 * The value at index for a parameter of that type: a number, a Color, a boolean, or the
 * alpha mode's name.
 */
Vector4
checkValue(lua_State* state,
           const int32 index,
           const MaterialParameterInfo& info,
           const ANSICHAR* function)
{
  switch (info.type) {
    case MaterialParameterType::Float:
      return Vector4(static_cast<float>(luaL_checknumber(state, index)), 0.0f, 0.0f, 0.0f);
    case MaterialParameterType::Color: {
      const LinearColor& color = ScriptTraits<LinearColor>::check(state, index);
      return Vector4(color.r, color.g, color.b, color.a);
    }
    case MaterialParameterType::Bool:
      luaL_checktype(state, index, LUA_TBOOLEAN);
      return Vector4(lua_toboolean(state, index) ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
    case MaterialParameterType::AlphaMode: {
      const StringView text = ScriptTraits<StringView>::check(state, index);
      for (uint32 i = 0; i < static_cast<uint32>(MaterialAlphaMode::COUNT); ++i) {
        if (StringUtils::equalsIgnoreCase(text, kAlphaModeNames[i])) {
          return Vector4(static_cast<float>(i), 0.0f, 0.0f, 0.0f);
        }
      }
      luaL_error(state, "%s: '%s' is not an alpha mode (Opaque, Mask)", function,
                 String(text).c_str());
      break;
    }
    case MaterialParameterType::COUNT:
      break;
  }
  return Vector4::ZERO;
}

void
pushValue(lua_State* state, const MaterialParameterInfo& info, const Vector4& value)
{
  switch (info.type) {
    case MaterialParameterType::Float:
      lua_pushnumber(state, value.x);
      return;
    case MaterialParameterType::Color:
      ScriptTraits<LinearColor>::push(state, LinearColor(value.x, value.y, value.z, value.w));
      return;
    case MaterialParameterType::Bool:
      lua_pushboolean(state, value.x != 0.0f ? 1 : 0);
      return;
    case MaterialParameterType::AlphaMode:
      lua_pushstring(state, kAlphaModeNames[static_cast<uint32>(value.x)]);
      return;
    case MaterialParameterType::COUNT:
      break;
  }
  lua_pushnil(state);
}

/*
 * Pushes the value of a parameter as the material holds it.
 */
void
pushMaterialValue(lua_State* state, const Material& material, const MaterialParameterInfo& info)
{
  Vector4 value;
  const bool bFound = material.getValue(info.name, value);
  CH_ASSERT(bFound);
  CH_PARAMETER_UNUSED(bFound);
  pushValue(state, info, value);
}

/*
 * Pushes a Material value of the asset, or nil without one.
 */
void
pushMaterial(lua_State* state, const SPtr<MaterialInterfaceAsset>& material)
{
  if (material) {
    ScriptTraits<ScriptMaterial>::push(state, ScriptMaterial{material->getUUID()});
  }
  else {
    lua_pushnil(state);
  }
}

/*
 * Material.find(pathOrName): a Material value, nil when there is none. Walks every asset,
 * so call it once (in onStart).
 */
int32
findMaterial(lua_State* state)
{
  const StringView text = ScriptTraits<StringView>::check(state, 1);
  if (!AssetManager::isStarted()) {
    return luaL_error(state, "Material.find: there are no assets to search");
  }
  pushMaterial(state, findMaterialByText(text));
  return 1;
}

/*
 * Material.createInstance(parent?): a MaterialInstance made now, never saved, that changes
 * only the values set on it.
 */
int32
createInstance(lua_State* state)
{
  SPtr<MaterialInterfaceAsset> parent;
  if (!lua_isnoneornil(state, 1)) {
    parent = checkMaterialAsset(state, 1, "Material.createInstance");
  }
  ScriptTraits<DynamicMaterial>::push(state, chMakeShared<DynamicMaterial>(parent));
  return 1;
}

int32
isMaterialValid(lua_State* state)
{
  const auto& material = ScriptTraits<ScriptMaterial>::check(state, 1);
  lua_pushboolean(state, findMaterialAsset(material.material) ? 1 : 0);
  return 1;
}

int32
getMaterialName(lua_State* state)
{
  const auto& material = ScriptTraits<ScriptMaterial>::check(state, 1);
  const SPtr<MaterialInterfaceAsset> asset = findMaterialAsset(material.material);
  if (asset) {
    lua_pushstring(state, asset->getName());
  }
  else {
    lua_pushnil(state);
  }
  return 1;
}

int32
getMaterialPath(lua_State* state)
{
  const auto& material = ScriptTraits<ScriptMaterial>::check(state, 1);
  const SPtr<MaterialInterfaceAsset> asset = findMaterialAsset(material.material);
  if (!asset) {
    lua_pushnil(state);
    return 1;
  }
  const String path = Path(asset->getAssetPath()).join(Path(asset->getName())).toString();
  lua_pushlstring(state, path.data(), path.size());
  return 1;
}

int32
isMaterialInstance(lua_State* state)
{
  const auto& material = ScriptTraits<ScriptMaterial>::check(state, 1);
  const SPtr<MaterialInterfaceAsset> asset = findMaterialAsset(material.material);
  lua_pushboolean(state, asset && asset->isTypeOf<MaterialInstanceAsset>() ? 1 : 0);
  return 1;
}

/*
 * material:getParent(): the parent of an instance asset (loads it), nil for a material.
 */
int32
getMaterialParent(lua_State* state)
{
  const SPtr<MaterialInterfaceAsset> asset = checkMaterialAsset(state, 1, "Material.getParent");
  pushMaterial(state, asset->getParentMaterial());
  return 1;
}

/*
 * material:getValue(name): the value the asset draws with; loads it.
 */
int32
getAssetValue(lua_State* state)
{
  const SPtr<MaterialInterfaceAsset> asset = checkMaterialAsset(state, 1, "Material.getValue");
  pushMaterialValue(state, asset->getMaterial(), checkValueName(state, 2, "Material.getValue"));
  return 1;
}

int32
materialToString(lua_State* state)
{
  const auto& material = ScriptTraits<ScriptMaterial>::check(state, 1);
  const SPtr<MaterialInterfaceAsset> asset = findMaterialAsset(material.material);
  lua_pushfstring(state, "Material(%s)", asset ? asset->getName() : "missing");
  return 1;
}

int32
setInstanceValue(lua_State* state)
{
  DynamicMaterial& material = ScriptTraits<DynamicMaterial>::check(state, 1);
  const MaterialParameterInfo& info = checkValueName(state, 2, "MaterialInstance.setValue");
  if (!material.setValue(info.name, checkValue(state, 3, info, "MaterialInstance.setValue"))) {
    return luaL_error(state, "MaterialInstance.setValue: %s takes no such value", info.name);
  }
  return 0;
}

int32
getInstanceValue(lua_State* state)
{
  const DynamicMaterial& material = ScriptTraits<DynamicMaterial>::check(state, 1);
  pushMaterialValue(state, material.getMaterial(),
                    checkValueName(state, 2, "MaterialInstance.getValue"));
  return 1;
}

int32
resetInstanceValue(lua_State* state)
{
  DynamicMaterial& material = ScriptTraits<DynamicMaterial>::check(state, 1);
  material.resetValue(checkValueName(state, 2, "MaterialInstance.resetValue").name);
  return 0;
}

int32
setInstanceTexture(lua_State* state)
{
  DynamicMaterial& material = ScriptTraits<DynamicMaterial>::check(state, 1);
  const MaterialTexture slot = checkTextureName(state, 2, "MaterialInstance.setTexture");
  material.setTexture(slot, checkTexture(state, 3, "MaterialInstance.setTexture"));
  return 0;
}

int32
resetInstanceTexture(lua_State* state)
{
  DynamicMaterial& material = ScriptTraits<DynamicMaterial>::check(state, 1);
  material.resetTexture(checkTextureName(state, 2, "MaterialInstance.resetTexture"));
  return 0;
}

int32
setInstanceParent(lua_State* state)
{
  DynamicMaterial& material = ScriptTraits<DynamicMaterial>::check(state, 1);
  material.setParent(lua_isnoneornil(state, 2)
                         ? nullptr
                         : checkMaterialAsset(state, 2, "MaterialInstance.setParent"));
  return 0;
}

int32
getInstanceParent(lua_State* state)
{
  pushMaterial(state, ScriptTraits<DynamicMaterial>::check(state, 1).getParent());
  return 1;
}

int32
collectInstance(lua_State* state)
{
  using Reference = ScriptTraits<DynamicMaterial>::Reference;
  static_cast<Reference*>(lua_touserdata(state, 1))->~Reference();
  return 0;
}

int32
instancesEqual(lua_State* state)
{
  const auto* left = ScriptTraits<DynamicMaterial>::testReference(state, 1);
  const auto* right = ScriptTraits<DynamicMaterial>::testReference(state, 2);
  lua_pushboolean(state, left && right && left->get() == right->get() ? 1 : 0);
  return 1;
}

int32
instanceToString(lua_State* state)
{
  const DynamicMaterial& material = ScriptTraits<DynamicMaterial>::check(state, 1);
  lua_pushfstring(state, "MaterialInstance(%s)",
                  material.getParent() ? material.getParent()->getName() : "no parent");
  return 1;
}

/*
 * A model slot: an index from 1 or a slot name. Raises for a slot the model lacks or the
 * object does not draw.
 */
uint32
checkSlot(lua_State* state, const ModelComponent& model, const int32 index,
          const ANSICHAR* function)
{
  if (!model.getModel()) {
    luaL_error(state, "%s: the object has no model loaded", function);
  }
  const Vector<ModelMaterialSlot>& slots = model.getModel()->getMaterialSlots();
  uint32 slot = ModelComponent::ALL;
  if (lua_type(state, index) == LUA_TNUMBER) {
    const lua_Integer number = luaL_checkinteger(state, index);
    if (number >= 1 && number <= static_cast<lua_Integer>(slots.size())) {
      slot = static_cast<uint32>(number - 1);
    }
  }
  else {
    const StringView name = ScriptTraits<StringView>::check(state, index);
    for (uint32 i = 0; i < slots.size(); ++i) {
      if (slots[i].name == name) {
        slot = i;
        break;
      }
    }
  }
  bool bUsed = false;
  for (const uint32 used : model.getUsedMaterialSlots()) {
    bUsed |= used == slot;
  }
  if (!bUsed) {
    luaL_error(state, "%s: the object draws no material slot %s", function,
               luaL_tolstring(state, index, nullptr));
  }
  return slot;
}

/*
 * What the slot draws with: its dynamic material, else its asset, else the default.
 */
const Material&
getSlotMaterial(const ModelComponent& model, const uint32 slot)
{
  if (const SPtr<DynamicMaterial>& dynamic = model.getDynamicMaterial(slot)) {
    return dynamic->getMaterial();
  }
  const SPtr<MaterialInterfaceAsset>& asset = model.getMaterial(slot);
  return asset ? asset->getMaterial() : getDefaultMaterial();
}

int32
getModelGameObject(lua_State* state)
{
  ScriptTraits<GameObject>::push(state, *ScriptTraits<ModelComponent>::check(state, 1).getOwner());
  return 1;
}

int32
getModelSlotCount(lua_State* state)
{
  const ModelComponent& model = ScriptTraits<ModelComponent>::check(state, 1);
  lua_pushinteger(state, model.getModel() ? static_cast<lua_Integer>(
                                                model.getModel()->getMaterialSlots().size())
                                          : 0);
  return 1;
}

int32
getModelSlotName(lua_State* state)
{
  const ModelComponent& model = ScriptTraits<ModelComponent>::check(state, 1);
  const lua_Integer number = luaL_checkinteger(state, 2);
  if (!model.getModel() || number < 1 ||
      number > static_cast<lua_Integer>(model.getModel()->getMaterialSlots().size())) {
    lua_pushnil(state);
    return 1;
  }
  const String& name = model.getModel()->getMaterialSlots()[number - 1].name;
  lua_pushlstring(state, name.data(), name.size());
  return 1;
}

/*
 * model:setMaterial(slot, material?): a Material (or its path or name) on this object
 * only, a MaterialInstance, or nil to go back to the model's.
 */
int32
setModelMaterial(lua_State* state)
{
  ModelComponent& model = ScriptTraits<ModelComponent>::check(state, 1);
  const uint32 slot = checkSlot(state, model, 2, "Model.setMaterial");
  if (lua_isnoneornil(state, 3)) {
    model.setMaterial(slot, nullptr);
  }
  else if (const auto* instance = ScriptTraits<DynamicMaterial>::testReference(state, 3)) {
    model.setDynamicMaterial(slot, *instance);
  }
  else {
    model.setMaterial(slot, checkMaterialAsset(state, 3, "Model.setMaterial"));
  }
  return 0;
}

/*
 * model:getMaterial(slot): the slot's MaterialInstance when it has one, else its Material,
 * else nil.
 */
int32
getModelMaterial(lua_State* state)
{
  const ModelComponent& model = ScriptTraits<ModelComponent>::check(state, 1);
  const uint32 slot = checkSlot(state, model, 2, "Model.getMaterial");
  if (const SPtr<DynamicMaterial>& dynamic = model.getDynamicMaterial(slot)) {
    ScriptTraits<DynamicMaterial>::push(state, dynamic);
  }
  else {
    pushMaterial(state, model.getMaterial(slot));
  }
  return 1;
}

int32
setModelValue(lua_State* state)
{
  ModelComponent& model = ScriptTraits<ModelComponent>::check(state, 1);
  const uint32 slot = checkSlot(state, model, 2, "Model.setMaterialValue");
  const MaterialParameterInfo& info = checkValueName(state, 3, "Model.setMaterialValue");
  const Vector4 value = checkValue(state, 4, info, "Model.setMaterialValue");
  model.getOwnDynamicMaterial(slot)->setValue(info.name, value);
  return 0;
}

int32
getModelValue(lua_State* state)
{
  const ModelComponent& model = ScriptTraits<ModelComponent>::check(state, 1);
  const uint32 slot = checkSlot(state, model, 2, "Model.getMaterialValue");
  pushMaterialValue(state, getSlotMaterial(model, slot),
                    checkValueName(state, 3, "Model.getMaterialValue"));
  return 1;
}

int32
resetModelValue(lua_State* state)
{
  ModelComponent& model = ScriptTraits<ModelComponent>::check(state, 1);
  const uint32 slot = checkSlot(state, model, 2, "Model.resetMaterialValue");
  const MaterialParameterInfo& info = checkValueName(state, 3, "Model.resetMaterialValue");
  if (model.getDynamicMaterial(slot)) {
    model.getOwnDynamicMaterial(slot)->resetValue(info.name);
  }
  return 0;
}

int32
setModelTexture(lua_State* state)
{
  ModelComponent& model = ScriptTraits<ModelComponent>::check(state, 1);
  const uint32 slot = checkSlot(state, model, 2, "Model.setMaterialTexture");
  const MaterialTexture texture = checkTextureName(state, 3, "Model.setMaterialTexture");
  model.getOwnDynamicMaterial(slot)->setTexture(
      texture, checkTexture(state, 4, "Model.setMaterialTexture"));
  return 0;
}

int32
resetModelTexture(lua_State* state)
{
  ModelComponent& model = ScriptTraits<ModelComponent>::check(state, 1);
  const uint32 slot = checkSlot(state, model, 2, "Model.resetMaterialTexture");
  const MaterialTexture texture = checkTextureName(state, 3, "Model.resetMaterialTexture");
  if (model.getDynamicMaterial(slot)) {
    model.getOwnDynamicMaterial(slot)->resetTexture(texture);
  }
  return 0;
}

/*
 * What the shape draws with: its dynamic material, else its asset, else the default.
 */
const Material&
getShapeMaterial(const ShapeComponent& shape)
{
  if (shape.getDynamicMaterial()) {
    return shape.getDynamicMaterial()->getMaterial();
  }
  return shape.getMaterial() ? shape.getMaterial()->getMaterial() : getDefaultMaterial();
}

int32
getShapeGameObject(lua_State* state)
{
  ScriptTraits<GameObject>::push(state, *ScriptTraits<ShapeComponent>::check(state, 1).getOwner());
  return 1;
}

int32
setShapeMaterial(lua_State* state)
{
  ShapeComponent& shape = ScriptTraits<ShapeComponent>::check(state, 1);
  if (lua_isnoneornil(state, 2)) {
    shape.setMaterial(nullptr);
  }
  else if (const auto* instance = ScriptTraits<DynamicMaterial>::testReference(state, 2)) {
    shape.setDynamicMaterial(*instance);
  }
  else {
    shape.setMaterial(checkMaterialAsset(state, 2, "Shape.setMaterial"));
  }
  return 0;
}

int32
getShapeMaterialValue(lua_State* state)
{
  const ShapeComponent& shape = ScriptTraits<ShapeComponent>::check(state, 1);
  if (shape.getDynamicMaterial()) {
    ScriptTraits<DynamicMaterial>::push(state, shape.getDynamicMaterial());
  }
  else {
    pushMaterial(state, shape.getMaterial());
  }
  return 1;
}

int32
setShapeValue(lua_State* state)
{
  ShapeComponent& shape = ScriptTraits<ShapeComponent>::check(state, 1);
  const MaterialParameterInfo& info = checkValueName(state, 2, "Shape.setMaterialValue");
  const Vector4 value = checkValue(state, 3, info, "Shape.setMaterialValue");
  shape.getOwnDynamicMaterial().setValue(info.name, value);
  return 0;
}

int32
getShapeValue(lua_State* state)
{
  const ShapeComponent& shape = ScriptTraits<ShapeComponent>::check(state, 1);
  pushMaterialValue(state, getShapeMaterial(shape),
                    checkValueName(state, 2, "Shape.getMaterialValue"));
  return 1;
}

int32
resetShapeValue(lua_State* state)
{
  ShapeComponent& shape = ScriptTraits<ShapeComponent>::check(state, 1);
  const MaterialParameterInfo& info = checkValueName(state, 2, "Shape.resetMaterialValue");
  if (shape.getDynamicMaterial()) {
    shape.getOwnDynamicMaterial().resetValue(info.name);
  }
  return 0;
}

int32
setShapeTexture(lua_State* state)
{
  ShapeComponent& shape = ScriptTraits<ShapeComponent>::check(state, 1);
  const MaterialTexture texture = checkTextureName(state, 2, "Shape.setMaterialTexture");
  shape.getOwnDynamicMaterial().setTexture(texture,
                                           checkTexture(state, 3, "Shape.setMaterialTexture"));
  return 0;
}

int32
resetShapeTexture(lua_State* state)
{
  ShapeComponent& shape = ScriptTraits<ShapeComponent>::check(state, 1);
  const MaterialTexture texture = checkTextureName(state, 2, "Shape.resetMaterialTexture");
  if (shape.getDynamicMaterial()) {
    shape.getOwnDynamicMaterial().resetTexture(texture);
  }
  return 0;
}

/*
 * gameObject:getModel() / getShape(): the part handle, nil without the component.
 */
int32
getObjectModel(lua_State* state)
{
  GameObject& gameObject = ScriptTraits<GameObject>::check(state, 1);
  if (!gameObject.getComponent<ModelComponent>()) {
    lua_pushnil(state);
    return 1;
  }
  ScriptPartHandle::push(state, gameObject, ScriptHandleType<ModelComponent>::NAME);
  return 1;
}

int32
getObjectShape(lua_State* state)
{
  GameObject& gameObject = ScriptTraits<GameObject>::check(state, 1);
  if (!gameObject.getComponent<ShapeComponent>()) {
    lua_pushnil(state);
    return 1;
  }
  ScriptPartHandle::push(state, gameObject, ScriptHandleType<ShapeComponent>::NAME);
  return 1;
}

/*
 * Adds a function to the metatable of a script type.
 */
void
setMetamethod(lua_State* state, const ANSICHAR* typeName, const ANSICHAR* name,
              const lua_CFunction function)
{
  luaL_getmetatable(state, typeName);
  lua_pushcfunction(state, function);
  lua_setfield(state, -2, name);
  lua_pop(state, 1);
}
} // namespace

/*
 */
void
ScriptBindings::registerMaterials(lua_State* state)
{
  ScriptClass<ScriptMaterial>(state)
      .rawStaticFunction("find", &findMaterial, "fun(pathOrName: string): Material?")
      .rawStaticFunction("createInstance", &createInstance,
                         "fun(parent?: Material|string): MaterialInstance")
      .rawMethod("isValid", &isMaterialValid, "fun(): boolean")
      .rawMethod("getName", &getMaterialName, "fun(): string?")
      .rawMethod("getPath", &getMaterialPath, "fun(): string?")
      .rawMethod("isInstance", &isMaterialInstance, "fun(): boolean")
      .rawMethod("getParent", &getMaterialParent, "fun(): Material?")
      .rawMethod("getValue", &getAssetValue, "fun(name: MaterialValueName): MaterialValue")
      .operators<ScriptOperator::Equal>();
  setMetamethod(state, ScriptValueType<ScriptMaterial>::NAME, "__tostring", &materialToString);

  ScriptClass<DynamicMaterial>(state)
      .rawMethod("setValue", &setInstanceValue,
                 "fun(name: MaterialValueName, value: MaterialValue)")
      .rawMethod("getValue", &getInstanceValue, "fun(name: MaterialValueName): MaterialValue")
      .rawMethod("resetValue", &resetInstanceValue, "fun(name: MaterialValueName)")
      .rawMethod("setTexture", &setInstanceTexture,
                 "fun(name: MaterialTextureName, texture?: Texture|string)")
      .rawMethod("resetTexture", &resetInstanceTexture, "fun(name: MaterialTextureName)")
      .rawMethod("setParent", &setInstanceParent, "fun(parent?: Material|string)")
      .rawMethod("getParent", &getInstanceParent, "fun(): Material?");
  const ANSICHAR* instanceName = ScriptHandleType<DynamicMaterial>::NAME;
  setMetamethod(state, instanceName, "__gc", &collectInstance);
  setMetamethod(state, instanceName, "__eq", &instancesEqual);
  setMetamethod(state, instanceName, "__tostring", &instanceToString);

  ScriptClass<ModelComponent>(state)
      .rawMethod("getGameObject", &getModelGameObject, "fun(): GameObject")
      .rawMethod("getMaterialSlotCount", &getModelSlotCount, "fun(): integer")
      .rawMethod("getMaterialSlotName", &getModelSlotName, "fun(slot: integer): string?")
      .rawMethod("setMaterial", &setModelMaterial,
                 "fun(slot: MaterialSlot, material?: Material|MaterialInstance|string)")
      .rawMethod("getMaterial", &getModelMaterial,
                 "fun(slot: MaterialSlot): Material|MaterialInstance|nil")
      .rawMethod("setMaterialValue", &setModelValue,
                 "fun(slot: MaterialSlot, name: MaterialValueName, value: MaterialValue)")
      .rawMethod("getMaterialValue", &getModelValue,
                 "fun(slot: MaterialSlot, name: MaterialValueName): MaterialValue")
      .rawMethod("resetMaterialValue", &resetModelValue,
                 "fun(slot: MaterialSlot, name: MaterialValueName)")
      .rawMethod("setMaterialTexture", &setModelTexture,
                 "fun(slot: MaterialSlot, name: MaterialTextureName, texture?: "
                 "Texture|string)")
      .rawMethod("resetMaterialTexture", &resetModelTexture,
                 "fun(slot: MaterialSlot, name: MaterialTextureName)");

  ScriptClass<ShapeComponent>(state)
      .rawMethod("getGameObject", &getShapeGameObject, "fun(): GameObject")
      .rawMethod("setMaterial", &setShapeMaterial,
                 "fun(material?: Material|MaterialInstance|string)")
      .rawMethod("getMaterial", &getShapeMaterialValue, "fun(): Material|MaterialInstance|nil")
      .rawMethod("setMaterialValue", &setShapeValue,
                 "fun(name: MaterialValueName, value: MaterialValue)")
      .rawMethod("getMaterialValue", &getShapeValue,
                 "fun(name: MaterialValueName): MaterialValue")
      .rawMethod("resetMaterialValue", &resetShapeValue, "fun(name: MaterialValueName)")
      .rawMethod("setMaterialTexture", &setShapeTexture,
                 "fun(name: MaterialTextureName, texture?: Texture|string)")
      .rawMethod("resetMaterialTexture", &resetShapeTexture, "fun(name: MaterialTextureName)");

  ScriptClass<GameObject>(state)
      .rawMethod("getModel", &getObjectModel, "fun(): Model?")
      .rawMethod("getShape", &getObjectShape, "fun(): Shape?");

  if (ScriptApiDescription* api = ScriptApiDescription::find(state)) {
    api->addAlias("MaterialValueName",
                  "\"baseColor\"|\"metallic\"|\"roughness\"|\"emissive\"|\"normalScale\"|"
                  "\"occlusionStrength\"|\"alphaMode\"|\"alphaCutoff\"|\"twoSided\"");
    api->addAlias("MaterialTextureName",
                  "\"baseColorTexture\"|\"metallicRoughnessTexture\"|\"normalTexture\"|"
                  "\"emissiveTexture\"|\"occlusionTexture\"");
    api->addAlias("MaterialValue", "number|boolean|Color|\"Opaque\"|\"Mask\"");
    api->addAlias("MaterialSlot", "integer|string");
  }
}

} // namespace chEngineSDK
