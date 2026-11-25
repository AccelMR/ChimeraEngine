/************************************************************************/
/**
 * @file chUIBorder.h
 * @author Swampertor
 * @date 2026/11/24
 * @brief Widget that draws a colored backdrop around one child, as UMG's UBorder.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chLinearColor.h"
#include "chUIContentWidget.h"
#include "chUIRect.h"

namespace chEngineSDK {

/**
 * Paints its rectangle with a brush color and places its content inside the padding.
 * A border with a transparent brush is a pure layout wrapper; one with a color is the
 * common panel or backdrop of a menu.
 */
class CH_CORE_EXPORT UIBorder : public UIContentWidget
{
 public:
  UIBorder();
  ~UIBorder() override;

  NODISCARD const ANSICHAR*
  getTypeName() const noexcept override
  {
    return "UIBorder";
  }

  // -- Brush ------------------------------------------------------------------

  NODISCARD FORCEINLINE const LinearColor&
  getBrushColor() const noexcept
  {
    return m_brushColor;
  }

  FORCEINLINE void
  setBrushColor(const LinearColor& color) noexcept
  {
    m_brushColor = color;
  }

  // -- Padding ----------------------------------------------------------------

  NODISCARD FORCEINLINE const UIMargin&
  getPadding() const noexcept
  {
    return m_padding;
  }

  FORCEINLINE void
  setPadding(const UIMargin& padding) noexcept
  {
    m_padding = padding;
  }

  // -- Widget -----------------------------------------------------------------

  NODISCARD Vector2
  getDesiredSize() const override;

  void
  paint(UIDrawList& drawList) const override;

 protected:
  void
  arrangeChildren() override;

 private:
  LinearColor m_brushColor{0.08f, 0.09f, 0.11f, 0.92f};
  UIMargin m_padding{4.0f, 4.0f, 4.0f, 4.0f};
};

} // namespace chEngineSDK
