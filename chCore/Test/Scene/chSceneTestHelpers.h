/************************************************************************/
/**
 * @file chSceneTestHelpers.h
 * @author AccelMR
 * @date 2026/11/17
 * @brief Components that the scene and level tests share.
 */
/************************************************************************/
#pragma once

#include "chComponent.h"
#include "chTypeTraits.h"

namespace chEngineSDK {
/**
 * Exists so the tests can count which updates a scene runs.
 */
class TestCounterComponent : public Component
{
 public:
  explicit TestCounterComponent(bool bUpdatesWhileEditing = false)
  {
    setUpdateFlags(ComponentUpdate::Frame | ComponentUpdate::Fixed);
    setUpdatesWhileEditing(bUpdatesWhileEditing);
  }

  void
  fixedUpdate(float deltaTime) override
  {
    CH_PARAMETER_UNUSED(deltaTime);
    ++fixedUpdates;
  }

  void
  update(float deltaTime) override
  {
    CH_PARAMETER_UNUSED(deltaTime);
    ++updates;
  }

  uint32 fixedUpdates = 0;
  uint32 updates = 0;
};
DECLARE_TYPE_TRAITS(TestCounterComponent)
} // namespace chEngineSDK
