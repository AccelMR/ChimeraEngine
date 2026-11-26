/************************************************************************/
/**
 * @file chUIDocumentAsset.cpp
 * @author Swampertor
 * @date 2026/11/26
 * @brief A UI document saved as an asset, with the assets its widgets use.
 */
/************************************************************************/
#include "chUIDocumentAsset.h"

#include <cstring>

#include "chAssetFile.h"
#include "chAssetManager.h"
#include "chFileStream.h"
#include "chLogger.h"

namespace chEngineSDK {

namespace {
// Above any real document; only a broken file asks for more.
constexpr uint32 kMaxContentSize = 1u << 30;

void
addUniqueReference(Vector<UUID>& references, const UUID& reference)
{
  if (reference.isNull()) {
    return;
  }
  for (const UUID& existing : references) {
    if (existing == reference) {
      return;
    }
  }
  references.push_back(reference);
}
} // namespace

/*
 */
bool
UIDocumentAsset::setContent(const UIDocument& document)
{
  MemoryDataStream stream;
  if (!document.serialize(stream)) {
    CH_LOG_ERROR(AssetSystem, "UI document {0} cannot be serialized", getName());
    return false;
  }

  m_content.resize(stream.size());
  if (!m_content.empty()) {
    std::memcpy(m_content.data(), stream.getStartPtr(), m_content.size());
  }

  m_contentReferences.clear();
  document.collectReferences(m_contentReferences);
  return true;
}

/*
 */
UniquePtr<UIDocument>
UIDocumentAsset::createDocument() const
{
  if (m_content.empty()) {
    return nullptr;
  }

  MemoryDataStream stream(m_content.size());
  if (stream.write(m_content.data(), m_content.size()) != m_content.size()) {
    CH_LOG_ERROR(AssetSystem, "UI document {0} cannot be read", getName());
    return nullptr;
  }
  stream.seek(0);

  UniquePtr<UIDocument> document = chMakeUnique<UIDocument>();
  if (!document->deserialize(stream)) {
    CH_LOG_ERROR(AssetSystem, "UI document {0} has data this engine cannot read", getName());
    return nullptr;
  }
  return document;
}

/*
 */
UniquePtr<UIUserWidget>
UIDocumentAsset::createUserWidget() const
{
  UniquePtr<UIDocument> document = createDocument();
  return document ? document->createUserWidget() : nullptr;
}

/*
 */
bool
UIDocumentAsset::serialize(SPtr<DataStream> stream)
{
  if (!stream) {
    return false;
  }
  const uint32 version = SERIALIZATION_VERSION;
  const uint32 size = static_cast<uint32>(m_content.size());
  stream->write(&version, sizeof(version));
  stream->write(&size, sizeof(size));
  stream->write(m_content.data(), m_content.size());
  return true;
}

/*
 */
bool
UIDocumentAsset::deserialize(SPtr<DataStream> stream)
{
  uint32 version = 0;
  uint32 size = 0;
  if (!stream || stream->read(&version, sizeof(version)) != sizeof(version) || version == 0
      || version > SERIALIZATION_VERSION || stream->read(&size, sizeof(size)) != sizeof(size)
      || size > kMaxContentSize) {
    CH_LOG_ERROR(AssetSystem, "UI document {0} has data this engine cannot read", getName());
    return false;
  }

  m_content.resize(size);
  if (stream->read(m_content.data(), size) != size) {
    CH_LOG_ERROR(AssetSystem, "UI document {0} is cut short", getName());
    m_content.clear();
    return false;
  }

  // IAsset::load read them from the start of the file.
  m_contentReferences = m_referencedAssets;
  return true;
}

/*
 */
void
UIDocumentAsset::clearAssetData()
{
  m_content.clear();
  m_contentReferences.clear();
}

/*
 */
void
UIDocumentAsset::collectReferences(Vector<UUID>& outReferences) const
{
  for (const UUID& reference : m_contentReferences) {
    addUniqueReference(outReferences, reference);
  }
}

} // namespace chEngineSDK
