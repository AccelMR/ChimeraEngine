/************************************************************************/
/**
 * @file chUIDocumentAsset.h
 * @author Swampertor
 * @date 2026/11/26
 * @brief A UI document saved as an asset, with the assets its widgets use.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chIAsset.h"
#include "chTypeTraits.h"
#include "chUIDocument.h"

namespace chEngineSDK {

/**
 * Exists to save a widget tree once and open it at runtime. The file holds the bytes
 * UIDocument writes; the fonts and textures the widgets name are its references, so they
 * load before the document and packaging follows them. createUserWidget reads the bytes
 * again for every instance, so two screens made from one document never share widgets.
 */
class CH_CORE_EXPORT UIDocumentAsset : public IAsset
{
 public:
  UIDocumentAsset() = delete;
  UIDocumentAsset(const AssetMetadata& metadata) : IAsset(metadata) {}
  ~UIDocumentAsset() override = default;

  /**
   * Takes document as the new content; save() writes it to the file. False, with the
   * content unchanged, when the tree cannot be serialized.
   */
  NODISCARD bool
  setContent(const UIDocument& document);

  /**
   * The serialized document, empty while the asset is not loaded or has no content.
   */
  NODISCARD FORCEINLINE Span<const uint8>
  getContent() const noexcept
  {
    return m_content;
  }

  /**
   * A new document read from the content, or null when it is empty or broken.
   */
  NODISCARD UniquePtr<UIDocument>
  createDocument() const;

  /**
   * A new user widget with a fresh tree read from the content. Null when the asset has no
   * content or the document cannot be read.
   */
  NODISCARD UniquePtr<UIUserWidget>
  createUserWidget() const;

  static constexpr uint32 SERIALIZATION_VERSION = 1;

 protected:
  bool
  serialize(SPtr<DataStream> stream) override;

  bool
  deserialize(SPtr<DataStream> stream) override;

  void
  clearAssetData() override;

  void
  collectReferences(Vector<UUID>& outReferences) const override;

 private:
  Vector<uint8> m_content;
  // What the content uses, kept so saving again does not need to read it.
  Vector<UUID> m_contentReferences;
};
DECLARE_ASSET_TYPE(UIDocumentAsset);

} // namespace chEngineSDK
