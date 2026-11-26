/************************************************************************/
/**
 * @file chUIDocument.h
 * @author Swampertor
 * @date 2026/11/26
 * @brief A widget tree saved as a document, with the registry that rebuilds it.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chUIPanelSlot.h"
#include "chUIUserWidget.h"
#include "chUIWidget.h"
#include "chUUID.h"

namespace chEngineSDK {
class DataStream;

/**
 * How one concrete widget type is written and rebuilt. The registry keeps one of these per
 * type name, so a document never needs a switch over the widget classes. writeProperties
 * and readProperties handle only what the class adds to UIWidget; the common fields and the
 * children are written by UIDocument.
 */
struct UIWidgetTypeDesc {
  using WriteFunc = Function<void(const UIWidget&, DataStream&)>;
  using ReadFunc = Function<bool(UIWidget&, DataStream&)>;
  using ReferencesFunc = Function<void(const UIWidget&, Vector<UUID>&)>;
  using AttachChildFunc =
      Function<UIWidget*(UIWidget&, UniquePtr<UIWidget>, UniquePtr<UIPanelSlot>)>;

  String typeName;
  Function<UniquePtr<UIWidget>()> create;
  WriteFunc writeProperties;
  ReadFunc readProperties;
  ReferencesFunc collectReferences;
  /**
   * Adds a child and its slot to a panel or content widget. Null for a widget that has no
   * children, which makes a document with children under it invalid.
   */
  AttachChildFunc attachChild;
  /**
   * Children the class accepts: 0 for a leaf, 1 for a content widget and the document
   * limit for a panel. A larger child count in a file is rejected.
   */
  uint32 maxChildren = 0;
};

/**
 * How one concrete slot type is written and rebuilt.
 */
struct UIPanelSlotTypeDesc {
  using WriteFunc = Function<void(const UIPanelSlot&, DataStream&)>;
  using ReadFunc = Function<bool(UIPanelSlot&, DataStream&)>;

  String typeName;
  Function<UniquePtr<UIPanelSlot>()> create;
  WriteFunc writeProperties;
  ReadFunc readProperties;
};

/**
 * The widget and slot types a UI document can hold, looked up by the name getTypeName()
 * returns. It is filled with the built-in widgets the first time it is used; a game can add
 * its own types with registerWidget and registerSlot before saving a document that uses
 * them.
 */
class CH_CORE_EXPORT UIDocumentRegistry
{
 public:
  NODISCARD static UIDocumentRegistry&
  instance();

  UIDocumentRegistry(const UIDocumentRegistry&) = delete;
  UIDocumentRegistry&
  operator=(const UIDocumentRegistry&) = delete;

  void
  registerWidget(UIWidgetTypeDesc desc);

  void
  registerSlot(UIPanelSlotTypeDesc desc);

  NODISCARD const UIWidgetTypeDesc*
  findWidget(StringView typeName) const;

  NODISCARD const UIPanelSlotTypeDesc*
  findSlot(StringView typeName) const;

  /**
   * Makes a widget of the named type, or null when no type has that name.
   */
  NODISCARD UniquePtr<UIWidget>
  createWidget(StringView typeName) const;

  /**
   * Makes a slot of the named type, or null when no type has that name.
   */
  NODISCARD UniquePtr<UIPanelSlot>
  createSlot(StringView typeName) const;

 private:
  UIDocumentRegistry();

  UnorderedMap<String, UIWidgetTypeDesc> m_widgets;
  UnorderedMap<String, UIPanelSlotTypeDesc> m_slots;
};

/**
 * One saved widget hierarchy, before it is put in a viewport. It owns the root and the root
 * owns the children, exactly as UIWidgetTree does at runtime; deserialize rebuilds it
 * through the registry, and createUserWidget hands it to a viewport.
 *
 * The file starts with a version, then the root widget and its children depth first. Each
 * widget stores its class name, the fields every widget has (name, enabled, visibility,
 * color, render transform and navigation), the fields of its class, and the class of the
 * slot its parent gave it. The root has no slot. A version this engine does not know, an
 * unknown class or a cut file fails without changing the document.
 */
class CH_CORE_EXPORT UIDocument
{
 public:
  static constexpr uint32 SERIALIZATION_VERSION = 1;

  UIDocument() = default;
  ~UIDocument() = default;

  UIDocument(const UIDocument&) = delete;
  UIDocument&
  operator=(const UIDocument&) = delete;

  /**
   * Replaces the tree. Returns the new root.
   */
  UIWidget*
  setRoot(UniquePtr<UIWidget> root);

  NODISCARD FORCEINLINE UIWidget*
  getRoot() const noexcept
  {
    return m_root.get();
  }

  /**
   * Moves the root out, leaving the document empty. Used to hand a deserialized tree to a
   * user widget.
   */
  NODISCARD UniquePtr<UIWidget>
  takeRoot() noexcept;

  void
  clear();

  NODISCARD bool
  isEmpty() const noexcept
  {
    return m_root == nullptr;
  }

  NODISCARD UIWidget*
  findWidget(StringView name) const;

  NODISCARD SIZE_T
  getWidgetCount() const;

  /**
   * Writes the tree in the format described above. False, with the stream holding what was
   * written so far, when a widget or slot class is not registered.
   */
  NODISCARD bool
  serialize(DataStream& stream) const;

  /**
   * Replaces the tree with the one in the stream. False, leaving the document unchanged,
   * when the version is unknown, the data is cut or a class is not registered.
   */
  NODISCARD bool
  deserialize(DataStream& stream);

  /**
   * Adds the UUIDs of the font and texture assets the widgets use, without duplicates.
   */
  void
  collectReferences(Vector<UUID>& outReferences) const;

  /**
   * Hands the root to a user widget that puts it in its tree when the viewport initializes
   * it. Null when the document is empty.
   */
  NODISCARD UniquePtr<UIUserWidget>
  createUserWidget();

 private:
  UniquePtr<UIWidget> m_root;
};

/**
 * The runtime document UIDocumentAsset makes: a UIUserWidget whose tree comes from the
 * saved document instead of a subclass building it in onInitialized.
 */
class CH_CORE_EXPORT UIDocumentUserWidget final : public UIUserWidget
{
 public:
  explicit UIDocumentUserWidget(UniquePtr<UIWidget> root);
  ~UIDocumentUserWidget() override = default;

  void
  onInitialized() override;

 private:
  UniquePtr<UIWidget> m_pendingRoot;
};

} // namespace chEngineSDK
