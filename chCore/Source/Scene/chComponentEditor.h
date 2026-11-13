/************************************************************************/
/**
 * @file chComponentEditor.h
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  What the Inspector shows of a component it has no section of its own for.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#if USING(CH_EDITOR)

#include "chComponent.h"
#include "chLinearColor.h"
#include "chScriptProperty.h"
#include "chTypeTraits.h"
#include "chUUID.h"
#include "chVector2.h"
#include "chVector3.h"
#include "chVector4.h"

namespace chEngineSDK {

/**
 * How a property is edited. Value is the widget of its type; Enum and Flags are for integers
 * and Multiline for strings.
 */
enum class ComponentPropertyWidget : uint8 {
  Value = 0,
  // A combo of names, the value being the index of the name.
  Enum = 1,
  // A checkbox per name, the value having bit i for name i.
  Flags = 2,
  // A box of several lines, for texts that hold line breaks.
  Multiline = 3
};

enum class ComponentEditorEntryKind : uint8 {
  Property = 0,
  // A button that calls a function of the component.
  Action = 1,
  // A line of text that starts a group of entries.
  Header = 2
};

using ComponentPropertyGetter = void (*)(const Component& component,
                                         ScriptPropertyValue& outValue);
using ComponentPropertySetter = void (*)(Component& component,
                                         const ScriptPropertyValue& value);
using ComponentActionFunction = void (*)(Component& component);

/**
 * What ComponentEditor::property takes besides the getter and setter.
 */
struct ComponentPropertyOptions {
  // Numbers with minimum < maximum show a slider and are kept inside.
  double minimum = 0.0;
  double maximum = 0.0;
  const ANSICHAR* tooltip = "";
  // What a UUID names: GameObject, Prefab, Scene or Texture.
  ScriptPropertyType objectType = ScriptPropertyType::GameObject;
  ComponentPropertyWidget widget = ComponentPropertyWidget::Value;
  // The names of Enum and Flags.
  std::initializer_list<const ANSICHAR*> names = std::initializer_list<const ANSICHAR*>();
};

struct ComponentEditorEntry {
  ComponentEditorEntryKind kind = ComponentEditorEntryKind::Property;
  String label{};
  String tooltip{};
  ScriptPropertyType type = ScriptPropertyType::Number;
  ComponentPropertyWidget widget = ComponentPropertyWidget::Value;
  double minimum = 0.0;
  double maximum = 0.0;
  Vector<String> names{};
  ComponentPropertyGetter getter = nullptr;
  ComponentPropertySetter setter = nullptr;
  ComponentActionFunction action = nullptr;
};

struct ComponentEditorDesc {
  UUID typeId = UUID::null();
  String displayName{};
  // What groups the type in Add Component, usually its plugin.
  String category{};
  Vector<ComponentEditorEntry> entries{};
  bool bAllowMultiple = false;
};

/**
 * Exists so the editor can show and add components of modules that cannot draw UI
 * themselves (plugins do not link ImGui): each type is described as data, which the
 * Inspector turns into rows. Fill it with ComponentEditor when the module loads. Editor
 * builds only; main thread.
 */
class CH_CORE_EXPORT ComponentEditorRegister
{
 public:
  /**
   * Replaces the description the type had.
   */
  static void
  add(ComponentEditorDesc&& desc);

  static void
  remove(const UUID& typeId);

  /**
   * Null for a type without a description.
   */
  NODISCARD static const ComponentEditorDesc*
  find(const UUID& typeId);

  /**
   * In the order they were added.
   */
  NODISCARD static const Vector<UniquePtr<ComponentEditorDesc>>&
  getAll();
};

namespace ComponentEditorDetail {
template <typename V> struct ValueTraits {
  static_assert(sizeof(V) == 0,
                "The editor cannot show this type: use a number, bool, String, vector, "
                "LinearColor, UUID or enum.");
};

template <typename V>
  requires(std::is_floating_point_v<V>)
struct ValueTraits<V> {
  static constexpr ScriptPropertyType TYPE = ScriptPropertyType::Number;

  static void
  write(const V& value, ScriptPropertyValue& out)
  {
    out.number = static_cast<double>(value);
  }

  NODISCARD static V
  read(const ScriptPropertyValue& in)
  {
    return static_cast<V>(in.number);
  }
};

template <typename V>
  requires(std::is_integral_v<V> && !std::is_same_v<V, bool>)
struct ValueTraits<V> {
  static constexpr ScriptPropertyType TYPE = ScriptPropertyType::Integer;

  static void
  write(const V& value, ScriptPropertyValue& out)
  {
    out.integer = static_cast<int64>(value);
  }

  /*
   * Kept inside the range of V, so an edit never wraps around.
   */
  NODISCARD static V
  read(const ScriptPropertyValue& in)
  {
    constexpr uint32 kBits = static_cast<uint32>(sizeof(V) * 8);
    if constexpr (std::is_signed_v<V>) {
      constexpr int64 kMaximum =
          static_cast<int64>((static_cast<uint64>(1) << (kBits - 1)) - 1);
      constexpr int64 kMinimum = -kMaximum - 1;
      return static_cast<V>(in.integer < kMinimum   ? kMinimum
                            : in.integer > kMaximum ? kMaximum
                                                    : in.integer);
    }
    else {
      if (in.integer < 0) {
        return 0;
      }
      const uint64 value = static_cast<uint64>(in.integer);
      if constexpr (kBits >= 64) {
        return static_cast<V>(value);
      }
      else {
        constexpr uint64 kMaximum = (static_cast<uint64>(1) << kBits) - 1;
        return static_cast<V>(value > kMaximum ? kMaximum : value);
      }
    }
  }
};

template <typename V>
  requires(std::is_enum_v<V>)
struct ValueTraits<V> {
  static constexpr ScriptPropertyType TYPE = ScriptPropertyType::Integer;
  using Underlying = std::underlying_type_t<V>;

  static void
  write(const V& value, ScriptPropertyValue& out)
  {
    out.integer = static_cast<int64>(static_cast<Underlying>(value));
  }

  NODISCARD static V
  read(const ScriptPropertyValue& in)
  {
    return static_cast<V>(ValueTraits<Underlying>::read(in));
  }
};

template <> struct ValueTraits<bool> {
  static constexpr ScriptPropertyType TYPE = ScriptPropertyType::Boolean;

  static void
  write(const bool value, ScriptPropertyValue& out)
  {
    out.boolean = value;
  }

  NODISCARD static bool
  read(const ScriptPropertyValue& in)
  {
    return in.boolean;
  }
};

template <> struct ValueTraits<String> {
  static constexpr ScriptPropertyType TYPE = ScriptPropertyType::String;

  static void
  write(const String& value, ScriptPropertyValue& out)
  {
    out.text = value;
  }

  NODISCARD static String
  read(const ScriptPropertyValue& in)
  {
    return in.text;
  }
};

template <> struct ValueTraits<Vector2> {
  static constexpr ScriptPropertyType TYPE = ScriptPropertyType::Vector2;

  static void
  write(const Vector2& value, ScriptPropertyValue& out)
  {
    out.components[0] = value.x;
    out.components[1] = value.y;
  }

  NODISCARD static Vector2
  read(const ScriptPropertyValue& in)
  {
    return Vector2(in.components[0], in.components[1]);
  }
};

template <> struct ValueTraits<Vector3> {
  static constexpr ScriptPropertyType TYPE = ScriptPropertyType::Vector3;

  static void
  write(const Vector3& value, ScriptPropertyValue& out)
  {
    out.components[0] = value.x;
    out.components[1] = value.y;
    out.components[2] = value.z;
  }

  NODISCARD static Vector3
  read(const ScriptPropertyValue& in)
  {
    return Vector3(in.components[0], in.components[1], in.components[2]);
  }
};

template <> struct ValueTraits<Vector4> {
  static constexpr ScriptPropertyType TYPE = ScriptPropertyType::Vector4;

  static void
  write(const Vector4& value, ScriptPropertyValue& out)
  {
    out.components[0] = value.x;
    out.components[1] = value.y;
    out.components[2] = value.z;
    out.components[3] = value.w;
  }

  NODISCARD static Vector4
  read(const ScriptPropertyValue& in)
  {
    return Vector4(in.components[0], in.components[1], in.components[2], in.components[3]);
  }
};

template <> struct ValueTraits<LinearColor> {
  static constexpr ScriptPropertyType TYPE = ScriptPropertyType::Color;

  static void
  write(const LinearColor& value, ScriptPropertyValue& out)
  {
    out.components[0] = value.r;
    out.components[1] = value.g;
    out.components[2] = value.b;
    out.components[3] = value.a;
  }

  NODISCARD static LinearColor
  read(const ScriptPropertyValue& in)
  {
    return LinearColor(in.components[0], in.components[1], in.components[2], in.components[3]);
  }
};

template <> struct ValueTraits<UUID> {
  // The real type comes from ComponentPropertyOptions::objectType.
  static constexpr ScriptPropertyType TYPE = ScriptPropertyType::GameObject;

  static void
  write(const UUID& value, ScriptPropertyValue& out)
  {
    out.object = value;
  }

  NODISCARD static UUID
  read(const ScriptPropertyValue& in)
  {
    return in.object;
  }
};

template <typename T, auto Getter>
using GetterValue = std::remove_cvref_t<std::invoke_result_t<decltype(Getter), const T&>>;

template <typename Struct, auto Member>
using MemberValue = std::remove_cvref_t<decltype(std::declval<Struct&>().*Member)>;

template <typename T, auto Getter>
void
get(const Component& component, ScriptPropertyValue& outValue)
{
  using Value = GetterValue<T, Getter>;
  ValueTraits<Value>::write(std::invoke(Getter, static_cast<const T&>(component)), outValue);
}

template <typename T, auto Getter, auto Setter>
void
set(Component& component, const ScriptPropertyValue& value)
{
  using Value = GetterValue<T, Getter>;
  std::invoke(Setter, static_cast<T&>(component), ValueTraits<Value>::read(value));
}

template <typename T, auto Getter, auto Member>
void
getMember(const Component& component, ScriptPropertyValue& outValue)
{
  using Struct = GetterValue<T, Getter>;
  using Value = MemberValue<Struct, Member>;
  const Struct whole = std::invoke(Getter, static_cast<const T&>(component));
  ValueTraits<Value>::write(whole.*Member, outValue);
}

template <typename T, auto Getter, auto Setter, auto Member>
void
setMember(Component& component, const ScriptPropertyValue& value)
{
  using Struct = GetterValue<T, Getter>;
  using Value = MemberValue<Struct, Member>;
  T& typed = static_cast<T&>(component);
  Struct whole = std::invoke(Getter, static_cast<const T&>(typed));
  whole.*Member = ValueTraits<Value>::read(value);
  std::invoke(Setter, typed, whole);
}

template <typename T, auto Function>
void
runAction(Component& component)
{
  std::invoke(Function, static_cast<T&>(component));
}
} // namespace ComponentEditorDetail

/**
 * Exists so describing a component for the editor is one line per row, with the C++ types
 * checked when it compiles, the way ScriptClass describes types for scripts:
 *
 *   ComponentEditor<ProjectileTargetComponent>("Projectile Target", "Projectiles")
 *     .property<&ProjectileTargetComponent::getRadius,
 *               &ProjectileTargetComponent::setRadius>("Radius", {.minimum = 0.001})
 *     .action<&ProjectileTargetComponent::reset>("Reset");
 *
 * A getter is a const member function (or a free function taking const T&) returning the
 * value; a setter takes T and the value. memberProperty edits one member of a struct a
 * getter and setter pair moves as a whole. The description is registered when the object
 * dies, so make it, chain the calls and drop it.
 */
template <typename T> class ComponentEditor
{
 public:
  ComponentEditor(const ANSICHAR* displayName, const ANSICHAR* category)
  {
    static_assert(std::is_base_of_v<Component, T>, "T must derive from Component.");
    static_assert(TypeTraits<T>::DECLARED, "T needs DECLARE_TYPE_TRAITS.");
    m_desc.typeId = TypeTraits<T>::getTypeId();
    m_desc.displayName = displayName;
    m_desc.category = category;
  }

  ~ComponentEditor() { ComponentEditorRegister::add(std::move(m_desc)); }

  ComponentEditor(const ComponentEditor&) = delete;

  ComponentEditor&
  operator=(const ComponentEditor&) = delete;

  template <auto Getter, auto Setter>
  ComponentEditor&
  property(const ANSICHAR* label, const ComponentPropertyOptions& options = {})
  {
    using Value = ComponentEditorDetail::GetterValue<T, Getter>;
    addProperty<Value>(label, options, &ComponentEditorDetail::get<T, Getter>,
                       &ComponentEditorDetail::set<T, Getter, Setter>);
    return *this;
  }

  template <auto Getter, auto Setter, auto Member>
  ComponentEditor&
  memberProperty(const ANSICHAR* label, const ComponentPropertyOptions& options = {})
  {
    using Struct = ComponentEditorDetail::GetterValue<T, Getter>;
    using Value = ComponentEditorDetail::MemberValue<Struct, Member>;
    addProperty<Value>(label, options, &ComponentEditorDetail::getMember<T, Getter, Member>,
                       &ComponentEditorDetail::setMember<T, Getter, Setter, Member>);
    return *this;
  }

  /**
   * A button; what it changes is not an undo step.
   */
  template <auto Function>
  ComponentEditor&
  action(const ANSICHAR* label, const ANSICHAR* tooltip = "")
  {
    ComponentEditorEntry& entry = m_desc.entries.emplace_back();
    entry.kind = ComponentEditorEntryKind::Action;
    entry.label = label;
    entry.tooltip = tooltip;
    entry.action = &ComponentEditorDetail::runAction<T, Function>;
    return *this;
  }

  ComponentEditor&
  header(const ANSICHAR* label)
  {
    ComponentEditorEntry& entry = m_desc.entries.emplace_back();
    entry.kind = ComponentEditorEntryKind::Header;
    entry.label = label;
    return *this;
  }

  /**
   * Add Component offers the type also on objects that already have one.
   */
  ComponentEditor&
  allowMultiple()
  {
    m_desc.bAllowMultiple = true;
    return *this;
  }

 private:
  template <typename Value>
  void
  addProperty(const ANSICHAR* label, const ComponentPropertyOptions& options,
              const ComponentPropertyGetter getter, const ComponentPropertySetter setter)
  {
    using Traits = ComponentEditorDetail::ValueTraits<Value>;
    ComponentEditorEntry& entry = m_desc.entries.emplace_back();
    entry.label = label;
    entry.tooltip = options.tooltip;
    entry.type = std::is_same_v<Value, UUID> ? options.objectType : Traits::TYPE;
    entry.widget = options.widget;
    entry.minimum = options.minimum;
    entry.maximum = options.maximum;
    for (const ANSICHAR* name : options.names) {
      entry.names.emplace_back(name);
    }
    entry.getter = getter;
    entry.setter = setter;
    CH_ASSERT((entry.widget == ComponentPropertyWidget::Value
               || (entry.widget == ComponentPropertyWidget::Multiline
                   && entry.type == ScriptPropertyType::String)
               || (entry.widget != ComponentPropertyWidget::Multiline
                   && entry.type == ScriptPropertyType::Integer))
              && "Enum and Flags edit integers and enums, Multiline strings.");
  }

  ComponentEditorDesc m_desc;
};

} // namespace chEngineSDK

#endif // USING(CH_EDITOR)
