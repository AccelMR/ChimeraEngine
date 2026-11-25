/************************************************************************/
/**
 * @file chUISizeBox.h
 * @author Swampertor
 * @date 2026/11/24
 * @brief Layout wrapper that keeps one child at a fixed size, as UMG's USizeBox.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chUIContentWidget.h"

namespace chEngineSDK {

/**
 * Asks for a fixed width and height instead of its content's. A zero override asks for the
 * content's size on that axis, so a size box can fix only one dimension.
 *
 * When its slot gives less room than an override, the content is clamped to the room it
 * has; the box never draws and takes no input of its own.
 */
class CH_CORE_EXPORT UISizeBox : public UIContentWidget
{
 public:
  UISizeBox();
  ~UISizeBox() override;

  NODISCARD const ANSICHAR*
  getTypeName() const noexcept override
  {
    return "UISizeBox";
  }

  NODISCARD FORCEINLINE float
  getWidthOverride() const noexcept
  {
    return m_widthOverride;
  }

  FORCEINLINE void
  setWidthOverride(float width) noexcept
  {
    m_widthOverride = width;
  }

  NODISCARD FORCEINLINE float
  getHeightOverride() const noexcept
  {
    return m_heightOverride;
  }

  FORCEINLINE void
  setHeightOverride(float height) noexcept
  {
    m_heightOverride = height;
  }

  NODISCARD Vector2
  getDesiredSize() const override;

 protected:
  void
  arrangeChildren() override;

 private:
  float m_widthOverride = 0.0f;
  float m_heightOverride = 0.0f;
};

} // namespace chEngineSDK
