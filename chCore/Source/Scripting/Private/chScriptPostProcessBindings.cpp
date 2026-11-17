/************************************************************************/
/**
 * @file chScriptPostProcessBindings.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief
 *  The PostProcess handle of a GameObject.
 */
/************************************************************************/
#include "chScriptBindings.h"

#include "chAssetManager.h"
#include "chGameObject.h"
#include "chPostProcessComponent.h"
#include "chScriptManager.h"
#include "chScriptMathTypes.h"
#include "chScriptObjectTypes.h"
#include "chShaderAsset.h"

namespace chEngineSDK {
CH_SCRIPT_HANDLE_TYPE(PostProcessComponent, "PostProcess");

/**
 * The post-process component of a GameObject: a handle kept with the object's like the
 * other parts, which finds the component each time it is used.
 */
template<>
struct ScriptTraits<PostProcessComponent>
{
  NODISCARD static PostProcessComponent&
  check(lua_State* state, const int32 index)
  {
    GameObject& gameObject = ScriptPartHandle::checkOwner(
        state, index, ScriptHandleType<PostProcessComponent>::NAME);
    PostProcessComponent* postProcess = gameObject.getComponent<PostProcessComponent>();
    if (!postProcess) {
      luaL_error(state, "GameObject '%s' has no PostProcess", gameObject.getName().c_str());
    }
    return *postProcess;
  }
};

namespace {
/*
 * The pass at index: its place from 1 or the name of its shader. Raises when there is none.
 */
NODISCARD uint32
checkPass(lua_State* state, const PostProcessComponent& postProcess, const int32 index)
{
  if (lua_type(state, index) == LUA_TNUMBER) {
    const lua_Integer place = luaL_checkinteger(state, index);
    if (place < 1 || place > static_cast<lua_Integer>(postProcess.getPassCount())) {
      luaL_error(state, "PostProcess has no pass %d", static_cast<int32>(place));
    }
    return static_cast<uint32>(place - 1);
  }
  const ANSICHAR* name = luaL_checkstring(state, index);
  const uint32 pass = postProcess.findPass(name);
  if (pass == PostProcessComponent::NOT_FOUND) {
    luaL_error(state, "PostProcess has no pass with the shader '%s'", name);
  }
  return pass;
}

/*
 * The shader asset named by a path or name, loaded. Raises when there is none.
 */
NODISCARD SPtr<ShaderAsset>
checkShader(lua_State* state, const int32 index)
{
  const ANSICHAR* name = luaL_checkstring(state, index);
  SPtr<ShaderAsset> shader;
  if (AssetManager::isStarted()) {
    const SPtr<IAsset> asset =
        AssetManager::instance().findAsset(AssetTypeTraits<ShaderAsset>::getTypeId(), name);
    shader = asset ? asset->as<ShaderAsset>() : nullptr;
  }
  if (!shader) {
    luaL_error(state, "There is no shader '%s'", name);
  }
  if (!shader->isLoaded() && !AssetManager::instance().syncLoadAsset(shader)) {
    luaL_error(state, "The shader '%s' could not be loaded", name);
  }
  return shader;
}

/*
 * The parameter of the pass's shader named at index, raising when there is none.
 */
NODISCARD const ShaderParameter&
checkParameter(lua_State* state,
               const PostProcessComponent& postProcess,
               const uint32 pass,
               const int32 index)
{
  const ANSICHAR* name = luaL_checkstring(state, index);
  const SPtr<ShaderAsset>& shader = postProcess.getPass(pass).shader;
  const uint32 parameter = shader ? shader->findParameter(name) : ShaderAsset::NOT_FOUND;
  if (parameter == ShaderAsset::NOT_FOUND) {
    luaL_error(state, "The pass's shader has no parameter '%s'", name);
  }
  return shader->getParameters()[parameter];
}

/*
 * The value at index for a parameter of that type, as the component keeps it.
 */
NODISCARD Vector4
checkValue(lua_State* state, const ShaderParameter& parameter, const int32 index)
{
  switch (parameter.type) {
    case ShaderParameterType::Float:
      return Vector4(static_cast<float>(luaL_checknumber(state, index)), 0.0f, 0.0f, 0.0f);
    case ShaderParameterType::Int:
      return Vector4(static_cast<float>(luaL_checkinteger(state, index)), 0.0f, 0.0f, 0.0f);
    case ShaderParameterType::Bool:
      return Vector4(lua_toboolean(state, index) != 0 ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
    case ShaderParameterType::Float2: {
      const Vector2& value = ScriptTraits<Vector2>::check(state, index);
      return Vector4(value.x, value.y, 0.0f, 0.0f);
    }
    case ShaderParameterType::Float3: {
      const Vector3& value = ScriptTraits<Vector3>::check(state, index);
      return Vector4(value.x, value.y, value.z, 0.0f);
    }
    case ShaderParameterType::Float4:
      return ScriptTraits<Vector4>::check(state, index);
    case ShaderParameterType::Color: {
      const LinearColor& value = ScriptTraits<LinearColor>::check(state, index);
      return Vector4(value.r, value.g, value.b, value.a);
    }
    case ShaderParameterType::COUNT:
      break;
  }
  return Vector4::ZERO;
}

void
pushValue(lua_State* state, const ShaderParameter& parameter, const Vector4& value)
{
  switch (parameter.type) {
    case ShaderParameterType::Float:
      lua_pushnumber(state, static_cast<lua_Number>(value.x));
      return;
    case ShaderParameterType::Int:
      lua_pushinteger(state, static_cast<lua_Integer>(value.x));
      return;
    case ShaderParameterType::Bool:
      lua_pushboolean(state, value.x != 0.0f ? 1 : 0);
      return;
    case ShaderParameterType::Float2:
      ScriptTraits<Vector2>::push(state, Vector2(value.x, value.y));
      return;
    case ShaderParameterType::Float3:
      ScriptTraits<Vector3>::push(state, Vector3(value.x, value.y, value.z));
      return;
    case ShaderParameterType::Float4:
      ScriptTraits<Vector4>::push(state, value);
      return;
    case ShaderParameterType::Color:
      ScriptTraits<LinearColor>::push(state, LinearColor(value.x, value.y, value.z, value.w));
      return;
    case ShaderParameterType::COUNT:
      break;
  }
  lua_pushnil(state);
}

int32
getPostProcessGameObject(lua_State* state)
{
  ScriptTraits<GameObject>::push(
      state,
      ScriptPartHandle::checkOwner(state, 1, ScriptHandleType<PostProcessComponent>::NAME));
  return 1;
}

int32
getPassCount(lua_State* state)
{
  lua_pushinteger(state, ScriptTraits<PostProcessComponent>::check(state, 1).getPassCount());
  return 1;
}

int32
addPass(lua_State* state)
{
  PostProcessComponent& postProcess = ScriptTraits<PostProcessComponent>::check(state, 1);
  const uint32 pass = postProcess.addPass(checkShader(state, 2));
  if (pass == PostProcessComponent::NOT_FOUND) {
    luaL_error(state, "PostProcess holds at most %d passes",
               static_cast<int32>(PostProcessComponent::MAX_PASSES));
  }
  lua_pushinteger(state, static_cast<lua_Integer>(pass) + 1);
  return 1;
}

int32
removePass(lua_State* state)
{
  PostProcessComponent& postProcess = ScriptTraits<PostProcessComponent>::check(state, 1);
  postProcess.removePass(checkPass(state, postProcess, 2));
  return 0;
}

int32
findPass(lua_State* state)
{
  const PostProcessComponent& postProcess = ScriptTraits<PostProcessComponent>::check(state, 1);
  const uint32 pass = postProcess.findPass(luaL_checkstring(state, 2));
  if (pass == PostProcessComponent::NOT_FOUND) {
    lua_pushnil(state);
  }
  else {
    lua_pushinteger(state, static_cast<lua_Integer>(pass) + 1);
  }
  return 1;
}

int32
setPassEnabled(lua_State* state)
{
  PostProcessComponent& postProcess = ScriptTraits<PostProcessComponent>::check(state, 1);
  postProcess.setPassEnabled(checkPass(state, postProcess, 2), lua_toboolean(state, 3) != 0);
  return 0;
}

int32
isPassEnabled(lua_State* state)
{
  const PostProcessComponent& postProcess = ScriptTraits<PostProcessComponent>::check(state, 1);
  lua_pushboolean(state, postProcess.getPass(checkPass(state, postProcess, 2)).bEnabled ? 1 : 0);
  return 1;
}

int32
setPassWeight(lua_State* state)
{
  PostProcessComponent& postProcess = ScriptTraits<PostProcessComponent>::check(state, 1);
  postProcess.setPassWeight(checkPass(state, postProcess, 2),
                            static_cast<float>(luaL_checknumber(state, 3)));
  return 0;
}

int32
getPassWeight(lua_State* state)
{
  const PostProcessComponent& postProcess = ScriptTraits<PostProcessComponent>::check(state, 1);
  lua_pushnumber(state, postProcess.getPass(checkPass(state, postProcess, 2)).weight);
  return 1;
}

int32
setPassValue(lua_State* state)
{
  PostProcessComponent& postProcess = ScriptTraits<PostProcessComponent>::check(state, 1);
  const uint32 pass = checkPass(state, postProcess, 2);
  const ShaderParameter& parameter = checkParameter(state, postProcess, pass, 3);
  const Vector4 value = checkValue(state, parameter, 4);
  static_cast<void>(postProcess.setValue(pass, parameter.name, value));
  return 0;
}

int32
getPassValue(lua_State* state)
{
  const PostProcessComponent& postProcess = ScriptTraits<PostProcessComponent>::check(state, 1);
  const uint32 pass = checkPass(state, postProcess, 2);
  const ShaderParameter& parameter = checkParameter(state, postProcess, pass, 3);
  Vector4 value = parameter.defaultValue;
  static_cast<void>(postProcess.getValue(pass, parameter.name, value));
  pushValue(state, parameter, value);
  return 1;
}

int32
resetPassValue(lua_State* state)
{
  PostProcessComponent& postProcess = ScriptTraits<PostProcessComponent>::check(state, 1);
  const uint32 pass = checkPass(state, postProcess, 2);
  postProcess.resetValue(pass, checkParameter(state, postProcess, pass, 3).name);
  return 0;
}

int32
setPostProcessPriority(lua_State* state)
{
  ScriptTraits<PostProcessComponent>::check(state, 1).setPriority(
      static_cast<int32>(luaL_checkinteger(state, 2)));
  return 0;
}

int32
getPostProcessPriority(lua_State* state)
{
  lua_pushinteger(state, ScriptTraits<PostProcessComponent>::check(state, 1).getPriority());
  return 1;
}

/*
 * gameObject:getPostProcess(): the object's PostProcess, nil without one.
 */
int32
getObjectPostProcess(lua_State* state)
{
  GameObject& gameObject = ScriptTraits<GameObject>::check(state, 1);
  if (!gameObject.getComponent<PostProcessComponent>()) {
    lua_pushnil(state);
    return 1;
  }
  ScriptPartHandle::push(state, gameObject, ScriptHandleType<PostProcessComponent>::NAME);
  return 1;
}

/*
 * gameObject:addPostProcess(): adds a PostProcess without passes and gives it; an object
 * that has one already gets that one.
 */
int32
addObjectPostProcess(lua_State* state)
{
  GameObject& gameObject = ScriptTraits<GameObject>::check(state, 1);
  if (!gameObject.getComponent<PostProcessComponent>()) {
    static_cast<void>(gameObject.addComponent<PostProcessComponent>());
  }
  ScriptPartHandle::push(state, gameObject, ScriptHandleType<PostProcessComponent>::NAME);
  return 1;
}
} // namespace

/*
 */
void
ScriptBindings::registerPostProcess(lua_State* state)
{
  ScriptClass<PostProcessComponent>(state)
      .rawMethod("getGameObject", &getPostProcessGameObject, "fun(): GameObject")
      .rawMethod("getPassCount", &getPassCount, "fun(): integer")
      .rawMethod("addPass", &addPass, "fun(shader: string): integer")
      .rawMethod("removePass", &removePass, "fun(pass: PostProcessPass)")
      .rawMethod("findPass", &findPass, "fun(shaderName: string): integer?")
      .rawMethod("setEnabled", &setPassEnabled, "fun(pass: PostProcessPass, enabled: boolean)")
      .rawMethod("isEnabled", &isPassEnabled, "fun(pass: PostProcessPass): boolean")
      .rawMethod("setWeight", &setPassWeight, "fun(pass: PostProcessPass, weight: number)")
      .rawMethod("getWeight", &getPassWeight, "fun(pass: PostProcessPass): number")
      .rawMethod("setValue", &setPassValue,
                 "fun(pass: PostProcessPass, name: string, value: PostProcessValue)")
      .rawMethod("getValue", &getPassValue,
                 "fun(pass: PostProcessPass, name: string): PostProcessValue")
      .rawMethod("resetValue", &resetPassValue, "fun(pass: PostProcessPass, name: string)")
      .rawMethod("setPriority", &setPostProcessPriority, "fun(priority: integer)")
      .rawMethod("getPriority", &getPostProcessPriority, "fun(): integer");

  ScriptClass<GameObject>(state)
      .rawMethod("getPostProcess", &getObjectPostProcess, "fun(): PostProcess?")
      .rawMethod("addPostProcess", &addObjectPostProcess, "fun(): PostProcess");

  if (ScriptApiDescription* api = ScriptApiDescription::find(state)) {
    api->addAlias("PostProcessPass", "integer|string");
    api->addAlias("PostProcessValue", "number|boolean|Vector2|Vector3|Vector4|Color");
  }
}

} // namespace chEngineSDK
