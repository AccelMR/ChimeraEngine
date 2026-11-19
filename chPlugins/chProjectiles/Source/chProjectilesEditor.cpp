/************************************************************************/
/**
 * @file chProjectilesEditor.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  What the editor's Inspector shows of the projectile components.
 */
/************************************************************************/
#include "chProjectilesEditor.h"

#if USING(CH_EDITOR)

#include "chComponentEditor.h"
#include "chProjectileSystemComponent.h"
#include "chProjectileTargetComponent.h"

namespace chEngineSDK {

namespace {
constexpr const ANSICHAR* kCategory = "Projectiles";
constexpr std::initializer_list<const ANSICHAR*> kTeamNames = {
    "Team 1", "Team 2",  "Team 3",  "Team 4",  "Team 5",  "Team 6",  "Team 7",  "Team 8",
    "Team 9", "Team 10", "Team 11", "Team 12", "Team 13", "Team 14", "Team 15", "Team 16"};
} // namespace

/*
 */
void
ProjectilesEditor::registerDescriptions()
{
  using System = ProjectileSystemComponent;
  ComponentEditor<System>("Projectile System", kCategory)
      .header("Look")
      .memberProperty<&System::getLook, &System::setLook, &ProjectileLook::texture>(
          "Texture", {.tooltip = "The whole image is shown; scripts can set a part of it "
                                 "with setLook(SpriteStyle)",
                      .objectType = ScriptPropertyType::Texture})
      .memberProperty<&System::getLook, &System::setLook, &ProjectileLook::material>(
          "Material", {.tooltip = "A sprite material that shades every projectile",
                       .objectType = ScriptPropertyType::SpriteMaterial})
      .memberProperty<&System::getLook, &System::setLook, &ProjectileLook::size>(
          "Size", {.tooltip = "World units, times each projectile's scale"})
      .memberProperty<&System::getLook, &System::setLook, &ProjectileLook::pivot>(
          "Pivot", {.minimum = 0.0, .maximum = 1.0, .tooltip = "(0, 0) is the bottom left"})
      .memberProperty<&System::getLook, &System::setLook, &ProjectileLook::color>("Color")
      .memberProperty<&System::getLook, &System::setLook, &ProjectileLook::depth>(
          "Depth", {.tooltip = "World X of every projectile"})
      .memberProperty<&System::getLook, &System::setLook, &ProjectileLook::sortingLayer>(
          "Sorting Layer")
      .memberProperty<&System::getLook, &System::setLook, &ProjectileLook::order>(
          "Order", {.tooltip = "Order inside the sorting layer"})
      .memberProperty<&System::getLook, &System::setLook, &ProjectileLook::blendMode>(
          "Blend", {.widget = ComponentPropertyWidget::Enum, .names = {"Alpha", "Additive"}})
      .memberProperty<&System::getLook, &System::setLook, &ProjectileLook::filter>(
          "Filter", {.widget = ComponentPropertyWidget::Enum, .names = {"Point", "Linear"}})
      .memberProperty<&System::getLook, &System::setLook, &ProjectileLook::bFaceVelocity>(
          "Face Velocity", {.tooltip = "The image's right side points where it flies"})
      .header("Shots")
      .memberProperty<&System::getDefaults, &System::setDefaults, &ProjectileDefaults::speed>(
          "Speed", {.tooltip = "Units per second of fire, fireRing and fireSpread"})
      .memberProperty<&System::getDefaults, &System::setDefaults,
                      &ProjectileDefaults::lifetime>("Lifetime",
                                                     {.minimum = 0.0, .maximum = 30.0})
      .memberProperty<&System::getDefaults, &System::setDefaults, &ProjectileDefaults::radius>(
          "Radius", {.tooltip = "Size of the circle that hits targets"})
      .memberProperty<&System::getDefaults, &System::setDefaults, &ProjectileDefaults::damage>(
          "Damage")
      .memberProperty<&System::getDefaults, &System::setDefaults,
                      &ProjectileDefaults::turnRate>(
          "Turn Rate", {.tooltip = "Degrees per second toward the homing target a script "
                                   "sets; 0 flies straight"})
      .memberProperty<&System::getDefaults, &System::setDefaults, &ProjectileDefaults::scale>(
          "Scale")
      .memberProperty<&System::getDefaults, &System::setDefaults,
                      &ProjectileDefaults::acceleration>("Acceleration")
      .memberProperty<&System::getDefaults, &System::setDefaults, &ProjectileDefaults::pierce>(
          "Pierce", {.tooltip = "Targets a projectile goes through before the next hit kills "
                                "it"})
      .header("System")
      .property<&System::getCapacity, &System::setCapacity>(
          "Capacity", {.tooltip = "Most projectiles alive at once; memory for all of them is "
                                  "kept"})
      .property<&System::getHitTeams, &System::setHitTeams>(
          "Hit Teams",
          {.tooltip = "Teams of the Projectile Targets it hits (scripts reach all 32)",
           .widget = ComponentPropertyWidget::Flags,
           .names = kTeamNames})
      .property<&System::getCellSize, &System::setCellSize>(
          "Cell Size", {.tooltip = "About twice the biggest radius of projectiles and "
                                   "targets"})
      .action<&System::testFire>("Test Fire",
                                 "A ring from the object, to see the look while editing "
                                 "(set Renderer.Path to Sprite2D to see sprites). Not saved.");

  using Target = ProjectileTargetComponent;
  ComponentEditor<Target>("Projectile Target", kCategory)
      .property<&Target::getRadius, &Target::setRadius>("Radius")
      .property<&Target::getTeam, &Target::setTeam>(
          "Team", {.minimum = 1.0,
                   .maximum = static_cast<double>(ProjectileTargetComponent::MAX_TEAMS),
                   .tooltip = "Projectile systems choose the teams they hit"});
}

} // namespace chEngineSDK

#endif // USING(CH_EDITOR)
