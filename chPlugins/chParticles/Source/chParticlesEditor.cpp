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
#include "chParticleSystemComponent.h"

namespace chEngineSDK {

/*
 */
void
ParticlesEditor::registerDescriptions()
{
  using System = FloatingNumberSystemComponent;
  using Motion = AgeMotion;
  using Tier = FloatingNumberTier;
  ComponentEditor<System>("Floating Numbers", "2D")
      .action<&System::testSpawn>("Test Spawn",
                                  "Numbers around the object that reach every tier")
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
                      &NumberFormatDesc::plainBelow>("Plain Below")
      .list<&System::getTiers, &System::setTiers>(
          "Tiers", {.tooltip = "A number takes the highest tier whose Min Value it reaches",
                    .itemName = "Tier",
                    .maxCount = GpuFloatingNumberMotion::MAX_TIERS})
      .listField<&System::getTiers, &System::setTiers, &Tier::minValue>("Min Value")
      .listField<&System::getTiers, &System::setTiers, &Tier::color>("Color")
      .listField<&System::getTiers, &System::setTiers, &Tier::outlineColor>("Outline Color")
      .listField<&System::getTiers, &System::setTiers, &Tier::outlineWidth>(
          "Outline", {.tooltip = "SDF fonts only: pixels of the font's raster size; 0 draws "
                                 "no outline"})
      .listField<&System::getTiers, &System::setTiers, &Tier::scale>("Scale")
      .listField<&System::getTiers, &System::setTiers, &Tier::popScale>(
          "Pop", {.tooltip = "Born this many times bigger, settling over Pop Duration"})
      .listField<&System::getTiers, &System::setTiers, &Tier::popDuration>("Pop Duration")
      .listField<&System::getTiers, &System::setTiers, &Tier::shakeAmplitude>(
          "Shake", {.tooltip = "World units, fading to none over Shake Duration"})
      .listField<&System::getTiers, &System::setTiers, &Tier::shakeFrequency>(
          "Shake Frequency")
      .listField<&System::getTiers, &System::setTiers, &Tier::shakeDuration>("Shake Duration")
      .listField<&System::getTiers, &System::setTiers, &Tier::flashColor>("Flash Color")
      .listField<&System::getTiers, &System::setTiers, &Tier::flashDuration>(
          "Flash", {.tooltip = "Seconds of blinking to Flash Color"})
      .listField<&System::getTiers, &System::setTiers, &Tier::flashRate>(
          "Flash Rate", {.tooltip = "Blinks per second"});

  using Particles = ParticleSystemComponent;
  using Emission = ParticleEmission;
  using Look = ParticleLook;
  using Flipbook = ParticleFlipbook;
  using Burst = ParticleBurst;
  ComponentEditor<Particles>("Particle System", "2D")
      .action<&Particles::testEmit>("Test Emit", "A handful of particles on the shape")
      .action<&Particles::play>("Play", "Starts the emission while editing")
      .action<&Particles::stopAndClear>("Stop", "Ends the emission and clears the particles")
      .property<&Particles::getCapacity, &Particles::setCapacity>(
          "Capacity", {.tooltip = "Particles in the ring; the oldest are written over when it "
                                  "is full"})
      .header("Emission")
      .memberProperty<&Particles::getEmission, &Particles::setEmission, &Emission::rate>(
          "Rate", {.tooltip = "Particles per second while it plays"})
      .memberProperty<&Particles::getEmission, &Particles::setEmission, &Emission::duration>(
          "Duration", {.tooltip = "Seconds of a cycle; bursts happen within it"})
      .memberProperty<&Particles::getEmission, &Particles::setEmission, &Emission::bLoop>(
          "Loop")
      .memberProperty<&Particles::getEmission, &Particles::setEmission,
                      &Emission::bPlayOnStart>("Play On Start")
      .memberProperty<&Particles::getEmission, &Particles::setEmission,
                      &Emission::bLocalSpace>(
          "Local Space", {.tooltip = "Particles follow the object after they are born"})
      .memberProperty<&Particles::getEmission, &Particles::setEmission, &Emission::shape>(
          "Shape", {.widget = ComponentPropertyWidget::Enum,
                    .names = {"Point", "Circle", "Circle Edge", "Rectangle", "Line"}})
      .memberProperty<&Particles::getEmission, &Particles::setEmission, &Emission::shapeSize>(
          "Shape Size", {.tooltip = "Circle radius and line half length in X, rectangle half "
                                    "size"})
      .list<&Particles::getBursts, &Particles::setBursts>(
          "Bursts", {.tooltip = "Particles made at once, at a time of each cycle",
                     .itemName = "Burst",
                     .maxCount = Particles::MAX_BURSTS})
      .listField<&Particles::getBursts, &Particles::setBursts, &Burst::time>(
          "Time", {.tooltip = "Seconds into the cycle"})
      .listField<&Particles::getBursts, &Particles::setBursts, &Burst::count>("Count")
      .listField<&Particles::getBursts, &Particles::setBursts, &Burst::cycles>(
          "Cycles", {.tooltip = "How many times it fires in each cycle"})
      .listField<&Particles::getBursts, &Particles::setBursts, &Burst::interval>(
          "Interval", {.tooltip = "Seconds between its cycles"})
      .header("Look")
      .property<&Particles::getTextureId, &Particles::setTextureId>(
          "Texture", {.tooltip = "Without one each particle is a square of its color",
                      .objectType = ScriptPropertyType::Texture})
      .memberProperty<&Particles::getLook, &Particles::setLook, &ParticleLook::material>(
          "Material", {.tooltip = "A sprite material whose pixel shader shades the particles",
                       .objectType = ScriptPropertyType::SpriteMaterial})
      .memberProperty<&Particles::getLook, &Particles::setLook, &Look::size>("Size")
      .curve<&Particles::getSizeCurve, &Particles::setSizeCurve>(
          "Size Over Life", {.minimum = 0.0,
                             .maximum = 1.0,
                             .emptyValue = 1.0,
                             .tooltip = "Times the size, from birth (left) to death (right)",
                             .maxKeys = GpuParticleLook::MAX_KEYS})
      .memberProperty<&Particles::getLook, &Particles::setLook, &Look::minScale>(
          "Min Scale", {.tooltip = "Each particle picks a scale between Min and Max"})
      .memberProperty<&Particles::getLook, &Particles::setLook, &Look::maxScale>("Max Scale")
      .memberProperty<&Particles::getLook, &Particles::setLook, &Look::color>(
          "Color", {.tooltip = "Each particle picks a color between Color and Color 2"})
      .memberProperty<&Particles::getLook, &Particles::setLook, &Look::color2>("Color 2")
      .gradient<&Particles::getColorGradient, &Particles::setColorGradient>(
          "Color Over Life", {.tooltip = "Times the color, from birth (left) to death (right)",
                              .maxKeys = GpuParticleLook::MAX_KEYS})
      .memberProperty<&Particles::getLook, &Particles::setLook, &Look::rotationMode>(
          "Rotation", {.widget = ComponentPropertyWidget::Enum,
                       .names = {"Random", "Face Velocity"}})
      .memberProperty<&Particles::getLook, &Particles::setLook, &Look::rotation>(
          "Start Angle", {.tooltip = "Degrees"})
      .memberProperty<&Particles::getLook, &Particles::setLook, &Look::rotationVariance>(
          "Angle Variance")
      .memberProperty<&Particles::getLook, &Particles::setLook, &Look::spin>(
          "Spin", {.tooltip = "Degrees per second"})
      .memberProperty<&Particles::getLook, &Particles::setLook, &Look::spinVariance>(
          "Spin Variance")
      .memberProperty<&Particles::getLook, &Particles::setLook, &Look::depth>("Depth")
      .memberProperty<&Particles::getLook, &Particles::setLook, &Look::sortingLayer>(
          "Sorting Layer")
      .memberProperty<&Particles::getLook, &Particles::setLook, &Look::order>("Order")
      .memberProperty<&Particles::getLook, &Particles::setLook, &Look::blendMode>(
          "Blend", {.widget = ComponentPropertyWidget::Enum, .names = {"Alpha", "Additive"}})
      .memberProperty<&Particles::getLook, &Particles::setLook, &Look::filter>(
          "Filter", {.widget = ComponentPropertyWidget::Enum, .names = {"Point", "Linear"}})
      .header("Flipbook")
      .memberProperty<&Particles::getFlipbook, &Particles::setFlipbook, &Flipbook::atlas>(
          "Atlas", {.tooltip = "Frames of the atlas played over each particle's life; its "
                               "texture replaces the look's",
                    .objectType = ScriptPropertyType::SpriteAtlas})
      .memberProperty<&Particles::getFlipbook, &Particles::setFlipbook, &Flipbook::firstFrame>(
          "First Frame", {.tooltip = "From 0"})
      .memberProperty<&Particles::getFlipbook, &Particles::setFlipbook, &Flipbook::frameCount>(
          "Frame Count", {.tooltip = "0 takes every frame from the first"})
      .memberProperty<&Particles::getFlipbook, &Particles::setFlipbook,
                      &Flipbook::framesPerSecond>(
          "Frames Per Second", {.tooltip = "0 spreads the frames over the lifetime"})
      .header("Motion")
      .memberProperty<&Particles::getMotion, &Particles::setMotion, &Motion::angle>(
          "Angle", {.tooltip = "Degrees counterclockwise from +Y: 90 flies up"})
      .memberProperty<&Particles::getMotion, &Particles::setMotion, &Motion::spread>(
          "Spread", {.tooltip = "Degrees to each side of the angle"})
      .memberProperty<&Particles::getMotion, &Particles::setMotion, &Motion::speed>("Speed")
      .memberProperty<&Particles::getMotion, &Particles::setMotion, &Motion::speedVariance>(
          "Speed Variance")
      .memberProperty<&Particles::getMotion, &Particles::setMotion, &Motion::gravity>(
          "Gravity")
      .memberProperty<&Particles::getMotion, &Particles::setMotion, &Motion::drag>("Drag")
      .memberProperty<&Particles::getMotion, &Particles::setMotion, &Motion::lifetime>(
          "Lifetime")
      .memberProperty<&Particles::getMotion, &Particles::setMotion,
                      &Motion::lifetimeVariance>("Lifetime Variance")
      .memberProperty<&Particles::getMotion, &Particles::setMotion, &Motion::fadeIn>(
          "Fade In")
      .memberProperty<&Particles::getMotion, &Particles::setMotion, &Motion::fadeOut>(
          "Fade Out");
}

} // namespace chEngineSDK

#endif // USING(CH_EDITOR)
