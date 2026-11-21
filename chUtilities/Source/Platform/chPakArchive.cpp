/************************************************************************/
/**
 * @file chPakArchive.cpp
 * @author AccelMR
 * @date 2026/11/20
 * @brief
 *  One file holding many files, read through a memory mapping.
 */
/************************************************************************/
#include "chPakArchive.h"

#include <cstring>

#include "chFileStream.h"
#include "chFileSystem.h"

namespace chEngineSDK {

namespace {
struct PakHeader {
  uint32 magic = PakArchive::MAGIC;
  uint32 version = PakArchive::VERSION;
  uint64 indexOffset = 0;
  uint64 indexSize = 0;
  uint32 entryCount = 0;
  uint32 padding = 0;
};
static_assert(sizeof(PakHeader) == 32, "PakHeader must have no padding");

// The smallest index entry: an empty path's length, offset, size and compression.
constexpr uint64 kMinIndexEntrySize = sizeof(uint32) + 2 * sizeof(uint64) + sizeof(uint32);

// A path inside an archive: relative, '/' separators, no empty, "." or ".." parts, so it can
// never name something outside the archive's folder when the archive is mounted.
bool
isValidEntryPath(StringView path) noexcept
{
  if (path.empty() || path.size() > PakArchive::MAX_PATH_LENGTH) {
    return false;
  }
  SIZE_T start = 0;
  while (start <= path.size()) {
    SIZE_T end = path.find('/', start);
    if (end == StringView::npos) {
      end = path.size();
    }
    const StringView part = path.substr(start, end - start);
    if (part.empty() || part == "." || part == ".." || part.find('\\') != StringView::npos) {
      return false;
    }
    start = end + 1;
  }
  return true;
}

// Reads from the mapping, moving the cursor; false when the bytes end first.
bool
readBytes(const uint8* data, uint64 end, uint64& cursor, void* out, uint64 size) noexcept
{
  if (size > end - cursor) {
    return false;
  }
  memcpy(out, data + cursor, static_cast<SIZE_T>(size));
  cursor += size;
  return true;
}

// A read-only view of one file of an archive that keeps the archive mapped while it lives.
class PakEntryStream : public MemoryDataStream
{
 public:
  PakEntryStream(SPtr<const PakArchive> archive, Span<const uint8> data)
   : MemoryDataStream(const_cast<uint8*>(data.data()), data.size(), false),
     m_archive(std::move(archive))
  {
  }

  // The mapping is read only.
  SIZE_T
  write(const void* buffer, SIZE_T size) override
  {
    CH_PARAMETER_UNUSED(buffer);
    CH_PARAMETER_UNUSED(size);
    return 0;
  }

 private:
  SPtr<const PakArchive> m_archive;
};
} // namespace

/*
 */
SPtr<PakArchive>
PakArchive::open(const Path& file, String& outError)
{
  SPtr<PakArchive> archive = chMakeShared<PakArchive>();
  archive->m_file = file;
  if (!archive->m_mapping.open(file)) {
    outError = "Could not open the archive " + file.toString() + ".";
    return nullptr;
  }

  const uint8* data = archive->m_mapping.getData();
  const uint64 fileSize = archive->m_mapping.getSize();
  PakHeader header;
  uint64 cursor = 0;
  if (!readBytes(data, fileSize, cursor, &header, sizeof(header)) || header.magic != MAGIC) {
    outError = file.toString() + " is not an archive.";
    return nullptr;
  }
  if (header.version != VERSION) {
    outError = file.toString() + " is an archive of another version; package the game again.";
    return nullptr;
  }
  const uint64 indexEnd = header.indexOffset + header.indexSize;
  if (header.indexOffset < sizeof(PakHeader) || indexEnd < header.indexOffset
      || indexEnd > fileSize || header.entryCount > MAX_ENTRIES
      || header.entryCount > header.indexSize / kMinIndexEntrySize) {
    outError = file.toString() + " is cut or broken.";
    return nullptr;
  }

  archive->m_entries.reserve(header.entryCount);
  cursor = header.indexOffset;
  for (uint32 i = 0; i < header.entryCount; ++i) {
    uint32 pathLength = 0;
    if (!readBytes(data, indexEnd, cursor, &pathLength, sizeof(pathLength))
        || pathLength > MAX_PATH_LENGTH || pathLength > indexEnd - cursor) {
      outError = file.toString() + " has a broken index.";
      return nullptr;
    }
    String path(reinterpret_cast<const ANSICHAR*>(data + cursor), pathLength);
    cursor += pathLength;
    PakEntry entry;
    if (!readBytes(data, indexEnd, cursor, &entry.offset, sizeof(entry.offset))
        || !readBytes(data, indexEnd, cursor, &entry.size, sizeof(entry.size))
        || !readBytes(data, indexEnd, cursor, &entry.compression, sizeof(entry.compression))
        || !isValidEntryPath(path) || entry.compression != 0
        || entry.offset < sizeof(PakHeader) || entry.offset > header.indexOffset
        || entry.size > header.indexOffset - entry.offset) {
      outError = file.toString() + " has a broken index.";
      return nullptr;
    }
    for (SIZE_T slash = path.find('/'); slash != String::npos;
         slash = path.find('/', slash + 1)) {
      archive->m_directories.emplace(path.substr(0, slash));
    }
    if (!archive->m_entries.emplace(std::move(path), entry).second) {
      outError = file.toString() + " lists a file twice.";
      return nullptr;
    }
  }
  return archive;
}

/*
 */
const PakEntry*
PakArchive::findEntry(StringView path) const noexcept
{
  const auto it = m_entries.find(String(path));
  return it != m_entries.end() ? &it->second : nullptr;
}

/*
 */
bool
PakArchive::isFile(StringView path) const noexcept
{
  return findEntry(path) != nullptr;
}

/*
 */
bool
PakArchive::isDirectory(StringView path) const noexcept
{
  return path.empty() || m_directories.contains(String(path));
}

/*
 */
Span<const uint8>
PakArchive::getData(StringView path) const noexcept
{
  const PakEntry* entry = findEntry(path);
  if (!entry || entry->size == 0) {
    return {};
  }
  return Span<const uint8>(m_mapping.getData() + entry->offset,
                           static_cast<SIZE_T>(entry->size));
}

/*
 */
SPtr<DataStream>
PakArchive::openEntry(StringView path) const
{
  if (!findEntry(path)) {
    return nullptr;
  }
  return chMakeShared<PakEntryStream>(shared_from_this(), getData(path));
}

/*
 */
void
PakArchive::forEachEntry(StringView directory, bool recursive,
                         const Function<void(StringView, bool)>& func) const
{
  UnorderedSet<String> folders;
  for (const auto& [path, entry] : m_entries) {
    StringView relative = path;
    if (!directory.empty()) {
      if (relative.size() <= directory.size() + 1
          || relative.substr(0, directory.size()) != directory
          || relative[directory.size()] != '/') {
        continue;
      }
      relative.remove_prefix(directory.size() + 1);
    }

    // Every folder between directory and the file, each once.
    for (SIZE_T slash = relative.find('/'); slash != StringView::npos;
         slash = relative.find('/', slash + 1)) {
      const StringView folder = relative.substr(0, slash);
      if (folders.emplace(folder).second) {
        func(folder, true);
      }
      if (!recursive) {
        break;
      }
    }
    if (recursive || relative.find('/') == StringView::npos) {
      func(relative, false);
    }
  }
}

/*
 */
PakWriter::PakWriter() = default;

/*
 */
PakWriter::~PakWriter()
{
  if (m_stream) {
    m_stream->close();
  }
}

/*
 */
bool
PakWriter::begin(const Path& file)
{
  m_index.clear();
  m_paths.clear();
  m_stream = FileSystem::createAndOpenFile(file);
  if (!m_stream) {
    return false;
  }
  const PakHeader header;
  m_offset = m_stream->write(&header, sizeof(header));
  return m_offset == sizeof(header);
}

/*
 */
bool
PakWriter::addEntry(StringView path, Span<const uint8> data)
{
  if (!m_stream || !isValidEntryPath(path) || !m_paths.emplace(path).second) {
    return false;
  }

  static constexpr uint8 kZeros[PakArchive::DATA_ALIGNMENT] = {};
  const uint64 padding = (PakArchive::DATA_ALIGNMENT - m_offset % PakArchive::DATA_ALIGNMENT)
                         % PakArchive::DATA_ALIGNMENT;
  if (padding > 0 && m_stream->write(kZeros, static_cast<SIZE_T>(padding)) != padding) {
    return false;
  }
  m_offset += padding;

  if (!data.empty() && m_stream->write(data.data(), data.size()) != data.size()) {
    return false;
  }
  m_index.push_back(
      {.path = String(path), .entry = {.offset = m_offset, .size = data.size()}});
  m_offset += data.size();
  return true;
}

/*
 */
bool
PakWriter::finish()
{
  if (!m_stream) {
    return false;
  }
  const uint64 indexOffset = m_offset;
  bool bWritten = true;
  for (const PendingEntry& pending : m_index) {
    const uint32 pathLength = static_cast<uint32>(pending.path.size());
    bWritten =
        bWritten && m_stream->write(&pathLength, sizeof(pathLength)) == sizeof(pathLength);
    bWritten = bWritten && m_stream->write(pending.path.data(), pathLength) == pathLength;
    bWritten =
        bWritten && m_stream->write(&pending.entry.offset, sizeof(uint64)) == sizeof(uint64);
    bWritten =
        bWritten && m_stream->write(&pending.entry.size, sizeof(uint64)) == sizeof(uint64);
    bWritten =
        bWritten
        && m_stream->write(&pending.entry.compression, sizeof(uint32)) == sizeof(uint32);
    m_offset += sizeof(uint32) + pathLength + 2 * sizeof(uint64) + sizeof(uint32);
  }

  const PakHeader header{.indexOffset = indexOffset,
                         .indexSize = m_offset - indexOffset,
                         .entryCount = static_cast<uint32>(m_index.size())};
  m_stream->seek(0);
  bWritten = bWritten && m_stream->write(&header, sizeof(header)) == sizeof(header);
  m_stream->close();
  m_stream.reset();
  return bWritten;
}

} // namespace chEngineSDK
