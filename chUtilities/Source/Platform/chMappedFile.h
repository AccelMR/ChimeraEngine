/************************************************************************/
/**
 * @file chMappedFile.h
 * @author AccelMR
 * @date 2026/11/20
 * @brief
 *  A whole file mapped into memory, read only.
 */
/************************************************************************/
#pragma once

#include "chPath.h"
#include "chPrerequisitesUtilities.h"

namespace chEngineSDK {

/**
 * Exists so big read-only files (pak archives) are read without copies or a system call per
 * read: the system loads the pages the program touches and drops them under memory pressure.
 * Each platform implements it in its own file, so no system header reaches shared code.
 */
class CH_UTILITY_EXPORT MappedFile
{
 public:
  MappedFile() noexcept;
  ~MappedFile();

  MappedFile(MappedFile&& other) noexcept;
  MappedFile&
  operator=(MappedFile&& other) noexcept;

  MappedFile(const MappedFile&) = delete;
  MappedFile&
  operator=(const MappedFile&) = delete;

  /**
   * Maps a real (not virtual) file, closing what was open. False and logged when it cannot;
   * an empty file opens with no data.
   */
  NODISCARD bool
  open(const Path& file);

  void
  close() noexcept;

  NODISCARD FORCEINLINE bool
  isOpen() const noexcept
  {
    return m_bOpen;
  }

  NODISCARD FORCEINLINE const uint8*
  getData() const noexcept
  {
    return m_data;
  }

  NODISCARD FORCEINLINE SIZE_T
  getSize() const noexcept
  {
    return m_size;
  }

 private:
  struct Impl;

  UniquePtr<Impl> m_impl;
  const uint8* m_data = nullptr;
  SIZE_T m_size = 0;
  bool m_bOpen = false;
};

} // namespace chEngineSDK
