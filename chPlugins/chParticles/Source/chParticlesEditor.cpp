/************************************************************************/
/**
 * @file chParticlesEditor.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  What the Inspector shows of the particles plugin's components.
 */
/************************************************************************/
#include "chParticlesEditor.h"

#if USING(CH_EDITOR)

#include "chComponentEditor.h"
#include "chFloatingNumberSystemComponent.h"

namespace chEngineSDK {

/*
 */
void
ParticlesEditor::registerDescriptions()
{
  using System = FloatingNumberSystemComponent;
  using Motion = AgeMotion;
  ComponentEditor<System>("Floating Numbers", "2D")
      .action<&System::testSpawn>(
          "Test Spawn", "Numbers around the object that reach every tier; tiers are set from "
                        "scripts with setTiers")
      .property<&System::getFontId, &System::setFontId>(
          "Font", {.tooltip = "Without one the engine's font is used",
                   .objectType = ScriptPropertyType::Font})
      .property<&System::getSize, &System::setSize>(
          "Size", {.tooltip = "Height of the font in world units, times each tier's scale"})
      .property<&System::getCapacity, &System::setCapacity>(
          "Capacity", {.tooltip = "Numbers in the ring (an outlined one takes two); the oldest "
                                  "are written over when it is full"})
      .property<&System::isMonospaceDigits, &System::setMonospaceDigits>("Fixed Digits")
      .property<&System::getSortingLayer, &System::setSortingLayer>("Sorting Layer")
      .property<&System::getOrder, &System::setOrder>("Order")
      .property<&System::getBlendMode, &System::setBlendMode>(
          "Blend", {.widget = ComponentPropertyWidget::Enum, .names = {"Alpha", "Additive"}})
      .header("Motion")
      .memberProperty<&System::getMotion, &System::setMotion, &Motion::angle>(
          "Angle", {.tooltip = "Degrees counterclockwise from +Y: 90 flies up"})
      .memberProperty<&System::getMotion, &System::setMotion, &Motion::spread>(
          "Spread", {.tooltip = "Degrees to each side of the angle"})
      .memberProperty<&System::getMotion, &System::setMotion, &Motion::speed>("Speed")
      .memberProperty<&System::getMotion, &System::setMotion, &Motion::speedVariance>(
          "Speed Variance")
      .memberProperty<&System::getMotion, &System::setMotion, &Motion::gravity>(
          "Gravity", {.tooltip = "World units per second squared; it bends the flight into an "
                                 "arc"})
      .memberProperty<&System::getMotion, &System::setMotion, &Motion::drag>("Drag")
      .memberProperty<&System::getMotion, &System::setMotion, &Motion::lifetime>("Lifetime")
      .memberProperty<&System::getMotion, &System::setMotion, &Motion::lifetimeVariance>(
          "Lifetime Variance")
      .memberProperty<&System::getMotion, &System::setMotion, &Motion::fadeIn>("Fade In")
      .memberProperty<&System::getMotion, &System::setMotion, &Motion::fadeOut>("Fade Out")
      .header("Numbers")
      .memberProperty<&System::getNumberFormat, &System::setNumberFormat,
                      &NumberFormatDesc::notation>(
          "Notation", {.widget = ComponentPropertyWidget::Enum,
                       .names = {"Plain", "Suffix", "Scientific", "Engineering"}})
      .memberProperty<&System::getNumberFormat, &System::setNumberFormat,
                      &NumberFormatDesc::decimals>("Decimals", {.minimum = 0.0, .maximum = 9.0})
      .memberProperty<&System::getNumberFormat, &System::setNumberFormat,
                      &NumberFormatDesc::plainBelow>("Plain Below");
}

} // namespace chEngineSDK

#endif // USING(CH_EDITOR)
