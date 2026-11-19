/************************************************************************/
/**
 * @file chCoreComponentEditors.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Inspector descriptions of the engine components that have no section of their own.
 */
/************************************************************************/
#include "chCoreComponentEditors.h"

#include "chAudioSourceComponent.h"
#include "chComponentEditor.h"
#include "chComponentEditorUI.h"
#include "chPostProcessComponent.h"
#include "chPostProcessEditorUI.h"
#include "chTextComponent.h"

namespace chEngineSDK {
namespace {
// A plain stop: the member's fade argument has a default, which a function pointer drops.
void
stopAudioSource(AudioSourceComponent& source)
{
  source.stop();
}

void
registerAudioSource()
{
  using Source = AudioSourceComponent;
  ComponentEditor<Source>("Audio Source", "Audio")
      .action<&Source::play>("Play", "Plays the clip now, also while editing")
      .action<&stopAudioSource>("Stop")
      .property<&Source::getClipId, &Source::setClipId>(
          "Clip", {.tooltip = "The sound it plays; it loads with the scene and goes into "
                              "packages",
                   .objectType = ScriptPropertyType::AudioClip})
      .property<&Source::isPlayOnStart, &Source::setPlayOnStart>(
          "Play On Start", {.tooltip = "Plays when the scene starts playing"})
      .property<&Source::isLooping, &Source::setLooping>("Loop")
      .property<&Source::getVolume, &Source::setVolume>(
          "Volume", {.minimum = 0.0, .maximum = 2.0,
                     .tooltip = "Times the clip's own volume and its bus"})
      .property<&Source::getPitch, &Source::setPitch>(
          "Pitch", {.minimum = 0.1, .maximum = 4.0,
                    .tooltip = "1 plays at its speed; 2 an octave higher and twice as fast"})
      .property<&Source::getPan, &Source::setPan>(
          "Pan", {.minimum = -1.0, .maximum = 1.0,
                  .tooltip = "-1 left, 1 right; only when not spatial"})
      .header("3D")
      .property<&Source::isSpatial, &Source::setSpatial>(
          "Spatial", {.tooltip = "Heard from where the object is, from the camera of the "
                                 "view, with doppler from its speed. Applies at the next "
                                 "play."})
      .memberProperty<&Source::getSpatialSettings, &Source::setSpatialSettings,
                      &AudioSpatialSettings::attenuation>(
          "Attenuation", {.tooltip = "How it gets quieter with distance",
                          .widget = ComponentPropertyWidget::Enum,
                          .names = {"None", "Inverse", "Linear", "Exponential"}})
      .memberProperty<&Source::getSpatialSettings, &Source::setSpatialSettings,
                      &AudioSpatialSettings::minDistance>(
          "Min Distance", {.minimum = 0.0, .maximum = 1000.0,
                           .tooltip = "Full volume up to here, in world units"})
      .memberProperty<&Source::getSpatialSettings, &Source::setSpatialSettings,
                      &AudioSpatialSettings::maxDistance>(
          "Max Distance", {.minimum = 0.0, .maximum = 10000.0,
                           .tooltip = "No quieter past here"})
      .memberProperty<&Source::getSpatialSettings, &Source::setSpatialSettings,
                      &AudioSpatialSettings::rolloff>("Rolloff", {.minimum = 0.0,
                                                                  .maximum = 10.0})
      .memberProperty<&Source::getSpatialSettings, &Source::setSpatialSettings,
                      &AudioSpatialSettings::dopplerFactor>(
          "Doppler", {.minimum = 0.0, .maximum = 4.0,
                      .tooltip = "0 turns the pitch shift of moving sounds off"})
      .memberProperty<&Source::getSpatialSettings, &Source::setSpatialSettings,
                      &AudioSpatialSettings::coneInnerAngle>(
          "Cone Inner Angle", {.minimum = 0.0, .maximum = 360.0,
                               .tooltip = "Degrees around the object's forward axis heard at "
                                          "full volume; 360 hears it the same everywhere"})
      .memberProperty<&Source::getSpatialSettings, &Source::setSpatialSettings,
                      &AudioSpatialSettings::coneOuterAngle>("Cone Outer Angle",
                                                             {.minimum = 0.0, .maximum = 360.0})
      .memberProperty<&Source::getSpatialSettings, &Source::setSpatialSettings,
                      &AudioSpatialSettings::coneOuterGain>(
          "Cone Outer Gain", {.minimum = 0.0, .maximum = 1.0,
                              .tooltip = "Volume outside the outer cone"});
}
} // namespace

/*
 */
void
CoreComponentEditors::registerDescriptions()
{
  registerAudioSource();

  using PostProcess = PostProcessComponent;
  ComponentEditor<PostProcess>("Post Process", "Rendering")
      .property<&PostProcess::getPriority, &PostProcess::setPriority>(
          "Priority", {.tooltip = "Components of a scene run lower priority first; their "
                                  "passes in the order of their lists"});
  ComponentEditorUI::setCustomSection(TypeTraits<PostProcess>::getTypeId(),
                                      &PostProcessEditorUI::render);

  using Text = TextComponent;
  ComponentEditor<Text>("Text", "2D")
      .property<&Text::getText, &Text::setText>(
          "Text", {.tooltip = "Up to 16 characters on one line without color tags is one "
                              "instance; longer texts are drawn glyph by glyph. "
                              "<color=#rrggbb>...</color> colors a part.",
                   .widget = ComponentPropertyWidget::Multiline})
      .property<&Text::getFontId, &Text::setFontId>(
          "Font", {.tooltip = "Without one the engine's font is used",
                   .objectType = ScriptPropertyType::Font})
      .property<&Text::getSize, &Text::setSize>(
          "Size", {.tooltip = "Height of the font in world units, times the object's scale"})
      .property<&Text::getColor, &Text::setColor>("Color")
      .header("Layout")
      .property<&Text::getAlign, &Text::setAlign>(
          "Align", {.widget = ComponentPropertyWidget::Enum,
                    .names = {"Left", "Center", "Right"}})
      .property<&Text::getPivot, &Text::setPivot>(
          "Pivot", {.minimum = 0.0, .maximum = 1.0,
                    .tooltip = "Point of the text at the object: (0, 0) bottom left"})
      .property<&Text::getWrapWidth, &Text::setWrapWidth>(
          "Wrap Width", {.tooltip = "Lines break at this width in world units; 0 never breaks"})
      .property<&Text::getLineSpacing, &Text::setLineSpacing>(
          "Line Spacing", {.tooltip = "Times the font's distance between lines"})
      .property<&Text::getLetterSpacing, &Text::setLetterSpacing>(
          "Letter Spacing", {.tooltip = "World units added after every glyph"})
      .property<&Text::isMonospaceDigits, &Text::setMonospaceDigits>(
          "Fixed Digits", {.tooltip = "Every digit takes the width of the widest, so numbers "
                                      "that change do not move"})
      .property<&Text::isRichText, &Text::setRichText>(
          "Color Tags", {.tooltip = "Reads <color=#rrggbb> tags; off, '<' is plain text"})
      .header("Look")
      .property<&Text::getSortingLayer, &Text::setSortingLayer>("Sorting Layer")
      .property<&Text::getOrder, &Text::setOrder>(
          "Order", {.tooltip = "Order inside the sorting layer"})
      .property<&Text::getBlendMode, &Text::setBlendMode>(
          "Blend", {.widget = ComponentPropertyWidget::Enum, .names = {"Alpha", "Additive"}})
      .property<&Text::getOutlineWidth, &Text::setOutlineWidth>(
          "Outline Width", {.minimum = 0.0, .maximum = 32.0,
                            .tooltip = "Pixels of the font's raster size, up to its SDF "
                                       "spread; pixel fonts have no outline"})
      .property<&Text::getOutlineColor, &Text::setOutlineColor>("Outline Color")
      .property<&Text::getShadowColor, &Text::setShadowColor>(
          "Shadow Color", {.tooltip = "Drawn when its alpha is above zero"})
      .property<&Text::getShadowOffset, &Text::setShadowOffset>(
          "Shadow Offset", {.tooltip = "World units, not turned with the text"})
      .header("Numbers")
      .memberProperty<&Text::getNumberFormat, &Text::setNumberFormat,
                      &NumberFormatDesc::notation>(
          "Notation", {.tooltip = "How scripts' setText(number) writes it",
                       .widget = ComponentPropertyWidget::Enum,
                       .names = {"Plain", "Suffix", "Scientific", "Engineering"}})
      .memberProperty<&Text::getNumberFormat, &Text::setNumberFormat,
                      &NumberFormatDesc::decimals>("Decimals", {.minimum = 0.0, .maximum = 9.0})
      .memberProperty<&Text::getNumberFormat, &Text::setNumberFormat,
                      &NumberFormatDesc::bTrimZeros>(
          "Trim Zeros", {.tooltip = "1.50K is written 1.5K"})
      .memberProperty<&Text::getNumberFormat, &Text::setNumberFormat,
                      &NumberFormatDesc::plainBelow>(
          "Plain Below", {.tooltip = "Numbers smaller than this are written plainly"})
      .memberProperty<&Text::getNumberFormat, &Text::setNumberFormat,
                      &NumberFormatDesc::plainDecimals>(
          "Plain Decimals", {.minimum = 0.0, .maximum = 9.0});
}

} // namespace chEngineSDK
