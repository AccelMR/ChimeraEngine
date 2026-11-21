/************************************************************************/
/**
 * @file chPakArchive.h
 * @author AccelMR
 * @date 2026/11/20
 * @brief
 *  One file holding many files, read through a memory mapping.
 */
/************************************************************************/
#pragma once

#include "chMappedFile.h"
#include "chPath.h"
#include "chPrerequisitesUtilities.h"

namespace chEngineSDK {

class DataStream;

/**
 * Where a file is inside an archive.
 */
struct PakEntry {
  uint64 offset = 0;
  uint64 size = 0;
  // 0: stored as it is. The only value today; kept so compression can come later.
  uint32 compression = 0;
};

/**
 * Exists so a packaged game ships its content as one file that FileSystem mounts like a
 * folder (FileSystem::mountArchive): the index is read once into a hash map and every file
 * inside is read straight from the mapping, without opening anything. Paths inside are
 * relative to the archive with '/' separators, compared as text.
 *
 * Layout: PakHeader, the files one after the other (each at DATA_ALIGNMENT), then the index
 * (per file: path, offset, size, compression), whose place the header gives.
 */
class CH_UTILITY_EXPORT PakArchive : public EnableSharedFromThis<PakArchive>
{
 public:
  /**
   * Null with outError set when the file is missing, not an archive, of another version or
   * broken (an index or an entry past its end).
   */
  NODISCARD static SPtr<PakArchive>
  open(const Path& file, String& outError);

  NODISCARD const PakEntry*
  findEntry(StringView path) const noexcept;

  NODISCARD bool
  isFile(StringView path) const noexcept;

  /**
   * The empty path is the archive's root.
   */
  NODISCARD bool
  isDirectory(StringView path) const noexcept;

  /**
   * The bytes of a file, valid while the archive lives; empty when there is none.
   */
  NODISCARD Span<const uint8>
  getData(StringView path) const noexcept;

  /**
   * A read-only stream over a file that keeps the archive alive; null when there is none.
   */
  NODISCARD SPtr<DataStream>
  openEntry(StringView path) const;

  /**
   * Calls func with the path, relative to directory, of every file and folder under it
   * (directly, or at any depth when recursive), each once. Walks the whole index.
   */
  void
  forEachEntry(StringView directory, bool recursive,
               const Function<void(StringView relativePath, bool bDirectory)>& func) const;

  NODISCARD FORCEINLINE const Path&
  getFile() const noexcept
  {
    return m_file;
  }

  NODISCARD FORCEINLINE SIZE_T
  getEntryCount() const noexcept
  {
    return m_entries.size();
  }

  // "CHPK" as the bytes appear in the file.
  static constexpr uint32 MAGIC = 0x4B504843;
  static constexpr uint32 VERSION = 1;
  // Every file starts on this many bytes, so data read in place keeps any alignment it needs.
  static constexpr uint64 DATA_ALIGNMENT = 64;
  static constexpr uint32 MAX_ENTRIES = 1u << 22;
  static constexpr uint32 MAX_PATH_LENGTH = 4096;

 private:
  Path m_file;
  MappedFile m_mapping;
  UnorderedMap<String, PakEntry> m_entries;
  // Every folder that holds an entry, at any depth, without the root.
  UnorderedSet<String> m_directories;
};

/**
 * Writes an archive PakArchive reads: files are added one by one (only the index stays in
 * memory) and finish writes the index.
 */
class CH_UTILITY_EXPORT PakWriter
{
 public:
  PakWriter();
  ~PakWriter();

  /**
   * Creates or empties the file. False when it cannot be written.
   */
  NODISCARD bool
  begin(const Path& file);

  /**
   * False when the path is empty, absolute, already added, or the write fails.
   */
  NODISCARD bool
  addEntry(StringView path, Span<const uint8> data);

  /**
   * Writes the index and closes the file.
   */
  NODISCARD bool
  finish();

 private:
  struct PendingEntry {
    String path;
    PakEntry entry;
  };

  SPtr<DataStream> m_stream;
  Vector<PendingEntry> m_index;
  UnorderedSet<String> m_paths;
  uint64 m_offset = 0;
};

} // namespace chEngineSDK
