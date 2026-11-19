/************************************************************************/
/**
 * @file chComponentEditorTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of the component descriptions the Inspector draws.
 */
/************************************************************************/
#include "chComponentEditor.h"
#include "chComponentRegister.h"
#include "chGraphicsTypes.h"
#include "chKeyCurve.h"
#include "chLinearColor.h"
#include "chScene.h"
#include "chScriptMathTypes.h"
#include "chScriptObjectTypes.h"
#include "chUUID.h"
#include "chVector2.h"

#include "catch.hpp"

using namespace chEngineSDK;

#if USING(CH_EDITOR)
namespace {
enum class TestEditMode : uint8
{
  First = 0,
  Second = 1,
  Third = 2
};

struct TestEditSettings
{
  Vector2 offset = Vector2::ZERO;
  LinearColor color = LinearColor::WHITE;
};

/*
 * A component with one value of each kind the editor descriptions handle.
 */
class TestEditableComponent : public Component
{
 public:
  NODISCARD float
  getSpeed() const
  {
    return m_speed;
  }

  void
  setSpeed(const float speed)
  {
    m_speed = speed;
  }

  NODISCARD uint8
  getCount() const
  {
    return m_count;
  }

  void
  setCount(const uint8 count)
  {
    m_count = count;
  }

  NODISCARD TestEditMode
  getMode() const
  {
    return m_mode;
  }

  void
  setMode(const TestEditMode mode)
  {
    m_mode = mode;
  }

  NODISCARD const TestEditSettings&
  getSettings() const
  {
    return m_settings;
  }

  void
  setSettings(const TestEditSettings& settings)
  {
    m_settings = settings;
  }

  NODISCARD const chEngineSDK::UUID&
  getTexture() const
  {
    return m_texture;
  }

  void
  setTexture(const chEngineSDK::UUID& texture)
  {
    m_texture = texture;
  }

  NODISCARD const String&
  getLabel() const
  {
    return m_label;
  }

  void
  setLabel(const String& label)
  {
    m_label = label;
  }

  void
  press()
  {
    ++m_presses;
  }

  float m_speed = 1.0f;
  uint8 m_count = 3;
  TestEditMode m_mode = TestEditMode::First;
  TestEditSettings m_settings;
  chEngineSDK::UUID m_texture = chEngineSDK::UUID::null();
  String m_label;
  uint32 m_presses = 0;
};
} // namespace

DECLARE_TYPE_TRAITS(TestEditableComponent)

TEST_CASE("chCore - Component editor descriptions", "[Components]")
{
  using T = TestEditableComponent;
  ComponentEditor<T>("Test Editable", "Tests")
      .header("Values")
      .property<&T::getSpeed, &T::setSpeed>("Speed", {.minimum = 0.0, .maximum = 10.0})
      .property<&T::getCount, &T::setCount>("Count")
      .property<&T::getMode, &T::setMode>(
          "Mode", {.widget = ComponentPropertyWidget::Enum, .names = {"First", "Second", "Third"}})
      .memberProperty<&T::getSettings, &T::setSettings, &TestEditSettings::offset>("Offset")
      .memberProperty<&T::getSettings, &T::setSettings, &TestEditSettings::color>("Color")
      .property<&T::getTexture, &T::setTexture>(
          "Texture", {.objectType = ScriptPropertyType::Texture})
      .property<&T::getLabel, &T::setLabel>("Label")
      .action<&T::press>("Press", "Counts presses");

  const ComponentEditorDesc* desc =
      ComponentEditorRegister::find(TypeTraits<T>::getTypeId());
  REQUIRE(desc);
  REQUIRE(desc->displayName == "Test Editable");
  REQUIRE(desc->category == "Tests");
  REQUIRE(desc->entries.size() == 9);
  REQUIRE(desc->entries[0].kind == ComponentEditorEntryKind::Header);
  REQUIRE(desc->entries[1].type == ScriptPropertyType::Number);
  REQUIRE(desc->entries[1].maximum == 10.0);
  REQUIRE(desc->entries[2].type == ScriptPropertyType::Integer);
  REQUIRE(desc->entries[3].widget == ComponentPropertyWidget::Enum);
  REQUIRE(desc->entries[3].names.size() == 3);
  REQUIRE(desc->entries[4].type == ScriptPropertyType::Vector2);
  REQUIRE(desc->entries[5].type == ScriptPropertyType::Color);
  REQUIRE(desc->entries[6].type == ScriptPropertyType::Texture);
  REQUIRE(desc->entries[7].type == ScriptPropertyType::String);
  REQUIRE(desc->entries[8].kind == ComponentEditorEntryKind::Action);

  T component;
  const auto read = [&](const SIZE_T index) {
    ScriptPropertyValue value;
    value.type = desc->entries[index].type;
    desc->entries[index].getter(component, value);
    return value;
  };
  const auto write = [&](const SIZE_T index, const ScriptPropertyValue& value) {
    desc->entries[index].setter(component, value);
  };

  SECTION("Values go through the getters and setters")
  {
    REQUIRE(read(1).number == 1.0);
    ScriptPropertyValue value = read(1);
    value.number = 2.5;
    write(1, value);
    REQUIRE(component.m_speed == 2.5f);

    value = read(3);
    value.integer = 2;
    write(3, value);
    REQUIRE(component.m_mode == TestEditMode::Third);

    value = read(7);
    value.text = "Hello";
    write(7, value);
    REQUIRE(component.m_label == "Hello");

    const chEngineSDK::UUID texture = chEngineSDK::UUID::createRandom();
    value = read(6);
    value.object = texture;
    write(6, value);
    REQUIRE(component.m_texture == texture);
  }

  SECTION("Integers stay inside their type")
  {
    ScriptPropertyValue value = read(2);
    REQUIRE(value.integer == 3);
    value.integer = 300;
    write(2, value);
    REQUIRE(component.m_count == 255);
    value.integer = -5;
    write(2, value);
    REQUIRE(component.m_count == 0);
  }

  SECTION("A member property changes only its member of the struct")
  {
    component.m_settings.color = LinearColor(0.5f, 0.25f, 0.0f, 1.0f);
    ScriptPropertyValue value = read(4);
    value.components[0] = 3.0f;
    value.components[1] = -1.0f;
    write(4, value);
    REQUIRE(component.m_settings.offset == Vector2(3.0f, -1.0f));
    REQUIRE(component.m_settings.color == LinearColor(0.5f, 0.25f, 0.0f, 1.0f));
    REQUIRE(read(5).components[1] == 0.25f);
  }

  SECTION("Actions call the component")
  {
    desc->entries[8].action(component);
    desc->entries[8].action(component);
    REQUIRE(component.m_presses == 2);
  }

  SECTION("A second description replaces the first, and remove forgets it")
  {
    const SIZE_T count = ComponentEditorRegister::getAll().size();
    ComponentEditor<T>("Renamed", "Tests").property<&T::getSpeed, &T::setSpeed>("Speed");
    REQUIRE(ComponentEditorRegister::getAll().size() == count);
    REQUIRE(ComponentEditorRegister::find(TypeTraits<T>::getTypeId())->displayName ==
            "Renamed");
    ComponentEditorRegister::remove(TypeTraits<T>::getTypeId());
    REQUIRE_FALSE(ComponentEditorRegister::find(TypeTraits<T>::getTypeId()));
    REQUIRE(ComponentEditorRegister::getAll().size() == count - 1);
  }

  SECTION("Objects add components of registered types by id")
  {
    ComponentRegister::registerType<T>();
    Scene scene("Editable", chEngineSDK::UUID::createRandom());
    GameObject& object = *scene.createGameObject("Object", nullptr);
    Component* added = object.addComponentByType(TypeTraits<T>::getTypeId());
    REQUIRE(added);
    REQUIRE(added->isRegistered());
    REQUIRE(object.getComponent<T>() == added);
    REQUIRE_FALSE(object.addComponentByType(chEngineSDK::UUID::createRandom()));
  }
}

#endif
#if USING(CH_EDITOR)
namespace {
/*
 * A component with a gradient, a curve and a list, as the particles have.
 */
class TestKeyedComponent : public Component
{
 public:
  NODISCARD Span<const GradientKey>
  getGradient() const
  {
    return m_gradient;
  }

  void
  setGradient(Span<const GradientKey> keys)
  {
    m_gradient.assign(keys.begin(), keys.end());
  }

  NODISCARD Span<const CurveKey>
  getCurve() const
  {
    return m_curve;
  }

  void
  setCurve(Span<const CurveKey> keys)
  {
    m_curve.assign(keys.begin(), keys.end());
  }

  NODISCARD Span<const TestEditSettings>
  getItems() const
  {
    return m_items;
  }

  void
  setItems(Span<const TestEditSettings> items)
  {
    m_items.assign(items.begin(), items.end());
  }

  Vector<GradientKey> m_gradient;
  Vector<CurveKey> m_curve;
  Vector<TestEditSettings> m_items;
};
} // namespace

DECLARE_TYPE_TRAITS(TestKeyedComponent)

TEST_CASE("chCore - Component editor keys and lists", "[Components]")
{
  using T = TestKeyedComponent;
  ComponentEditor<T>("Test Keyed", "Tests")
      .gradient<&T::getGradient, &T::setGradient>("Color", {.maxKeys = 4})
      .curve<&T::getCurve, &T::setCurve>("Size", {.minimum = -1.0,
                                                  .maximum = 2.0,
                                                  .emptyValue = 1.0})
      .list<&T::getItems, &T::setItems>("Items", {.itemName = "Item", .maxCount = 3})
      .listField<&T::getItems, &T::setItems, &TestEditSettings::offset>("Offset")
      .listField<&T::getItems, &T::setItems, &TestEditSettings::color>("Color");

  const ComponentEditorDesc* desc = ComponentEditorRegister::find(TypeTraits<T>::getTypeId());
  REQUIRE(desc);
  REQUIRE(desc->entries.size() == 3);
  const ComponentEditorEntry& gradient = desc->entries[0];
  const ComponentEditorEntry& curve = desc->entries[1];
  const ComponentEditorEntry& list = desc->entries[2];
  REQUIRE(gradient.kind == ComponentEditorEntryKind::Gradient);
  REQUIRE(gradient.maxCount == 4);
  REQUIRE(curve.kind == ComponentEditorEntryKind::Curve);
  REQUIRE(curve.minimum == -1.0);
  REQUIRE(curve.emptyValue == 1.0);
  REQUIRE(list.kind == ComponentEditorEntryKind::List);
  REQUIRE(list.itemName == "Item");
  REQUIRE(list.fields.size() == 2);
  REQUIRE(list.fields[0].type == ScriptPropertyType::Vector2);
  REQUIRE(list.fields[1].type == ScriptPropertyType::Color);

  T component;
  const GradientKey colors[] = {{.time = 0.0f, .color = LinearColor::BLACK},
                                {.time = 1.0f, .color = LinearColor::WHITE}};
  gradient.gradientSetter(component, colors);
  REQUIRE(gradient.gradientGetter(component).size() == 2);
  REQUIRE(KeyCurve::evaluate(gradient.gradientGetter(component), 0.5f).r == Approx(0.5f));

  const CurveKey sizes[] = {{.time = 0.25f, .value = 2.0f}, {.time = 0.75f, .value = 0.0f}};
  curve.curveSetter(component, sizes);
  const Span<const CurveKey> read = curve.curveGetter(component);
  REQUIRE(KeyCurve::evaluate(read, 0.0f, 1.0f) == 2.0f);
  REQUIRE(KeyCurve::evaluate(read, 0.5f, 1.0f) == Approx(1.0f));
  REQUIRE(KeyCurve::evaluate(read, 1.0f, 1.0f) == 0.0f);
  REQUIRE(KeyCurve::evaluate(Span<const CurveKey>(), 0.5f, 1.0f) == 1.0f);

  REQUIRE(list.listCount(component) == 0);
  list.listAdd(component);
  list.listAdd(component);
  REQUIRE(list.listCount(component) == 2);
  ScriptPropertyValue value;
  value.type = ScriptPropertyType::Vector2;
  list.fields[0].getter(component, 1, value);
  value.components[0] = 4.0f;
  list.fields[0].setter(component, 1, value);
  REQUIRE(component.m_items[1].offset == Vector2(4.0f, 0.0f));
  REQUIRE(component.m_items[0].offset == Vector2::ZERO);
  list.listRemove(component, 0);
  REQUIRE(list.listCount(component) == 1);
  REQUIRE(component.m_items[0].offset == Vector2(4.0f, 0.0f));
  // A field or removal past the end changes nothing.
  list.fields[0].setter(component, 5, value);
  list.listRemove(component, 5);
  REQUIRE(list.listCount(component) == 1);
}
#endif // USING(CH_EDITOR)
