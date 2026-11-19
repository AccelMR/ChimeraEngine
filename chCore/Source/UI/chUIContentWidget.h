/************************************************************************/
/**
 * @file chUIContentWidget.h
 * @author Swampertor
 * @date 2026/11/19
 * @brief Widget with exactly one child, as UMG's UContentWidget.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chUIPanelSlot.h"
#include "chUIWidget.h"

namespace chEngineSDK {

/**
 * Base of the widgets that wrap one piece of content: a button, a border, a size box. The
 * content is owned here and placed by one slot; the base slot fills the widget.
 */
class CH_CORE_EXPORT UIContentWidget : public UIWidget
{
 public:
  UIContentWidget();
  ~UIContentWidget() override;

  NODISCARD const ANSICHAR*
  getTypeName() const noexcept override
  {
    return "UIContentWidget";
  }

  NODISCARD SIZE_T
  getChildCount() const noexcept override
  {
    return m_content ? 1 : 0;
  }

  NODISCARD UIWidget*
  getChildAt(SIZE_T index) const noexcept override;

  /**
   * Replaces the content with a default slot that fills this widget. Returns the new
   * content, or nullptr when it is null or still owned by another parent.
   */
  UIWidget*
  setContent(UniquePtr<UIWidget> content);

  /**
   * Replaces the content with the slot that should place it. This widget takes both.
   */
  UIWidget*
  setContent(UniquePtr<UIWidget> content, UniquePtr<UIPanelSlot> slot);

  /**
   * Destroys the content and its slot.
   */
  void
  clearContent();

  NODISCARD FORCEINLINE UIWidget*
  getContent() const noexcept
  {
    return m_content.get();
  }

  NODISCARD FORCEINLINE UIPanelSlot*
  getContentSlot() const noexcept
  {
    return m_contentSlot.get();
  }

 protected:
  void
  arrangeChildren() override;

 private:
  UniquePtr<UIWidget> m_content;
  UniquePtr<UIPanelSlot> m_contentSlot;
};

} // namespace chEngineSDK
