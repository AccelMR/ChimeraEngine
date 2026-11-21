/************************************************************************/
/**
 * @file chMappedFileWin32.cpp
 * @author AccelMR
 * @date 2026/11/20
 * @brief
 *  MappedFile on Windows: CreateFileMapping and MapViewOfFile.
 */
/************************************************************************/
#include "chMappedFile.h"

#if USING(CH_PLATFORM_WIN32)

#include "chFileSystem.h"
#include "chLogger.h"
#include "chWindows.h"

namespace chEngineSDK {
CH_LOG_DECLARE_STATIC(MappedFileLog, All);

struct MappedFile::Impl {
  HANDLE file = INVALID_HANDLE_VALUE;
  HANDLE mapping = nullptr;
  void* view = nullptr;
};

/*
 */
MappedFile::MappedFile() noexcept = default;

/*
 */
MappedFile::~MappedFile() { close(); }

/*
 */
MappedFile::MappedFile(MappedFile&& other) noexcept
 : m_impl(std::move(other.m_impl)),
   m_data(other.m_data),
   m_size(other.m_size),
   m_bOpen(other.m_bOpen)
{
  other.m_data = nullptr;
  other.m_size = 0;
  other.m_bOpen = false;
}

/*
 */
MappedFile&
MappedFile::operator=(MappedFile&& other) noexcept
{
  if (this != &other) {
    close();
    m_impl = std::move(other.m_impl);
    m_data = other.m_data;
    m_size = other.m_size;
    m_bOpen = other.m_bOpen;
    other.m_data = nullptr;
    other.m_size = 0;
    other.m_bOpen = false;
  }
  return *this;
}

/*
 */
bool
MappedFile::open(const Path& file)
{
  close();
  const Path realFile = FileSystem::absolutePath(file);
  Impl impl;
  impl.file = CreateFileW(realFile.getPlatformString().c_str(), GENERIC_READ, FILE_SHARE_READ,
                          nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (impl.file == INVALID_HANDLE_VALUE) {
    CH_LOG_ERROR(MappedFileLog, "Could not open {0} (error {1}).", realFile,
                 static_cast<uint32>(GetLastError()));
    return false;
  }

  LARGE_INTEGER size{};
  if (!GetFileSizeEx(impl.file, &size)) {
    CH_LOG_ERROR(MappedFileLog, "Could not read the size of {0}.", realFile);
    CloseHandle(impl.file);
    return false;
  }

  // Windows cannot map an empty file; it opens with no data.
  if (size.QuadPart > 0) {
    impl.mapping = CreateFileMappingW(impl.file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    impl.view = impl.mapping ? MapViewOfFile(impl.mapping, FILE_MAP_READ, 0, 0, 0) : nullptr;
    if (!impl.view) {
      CH_LOG_ERROR(MappedFileLog, "Could not map {0} (error {1}).", realFile,
                   static_cast<uint32>(GetLastError()));
      if (impl.mapping) {
        CloseHandle(impl.mapping);
      }
      CloseHandle(impl.file);
      return false;
    }
  }

  m_impl = chMakeUnique<Impl>(impl);
  m_data = static_cast<const uint8*>(impl.view);
  m_size = static_cast<SIZE_T>(size.QuadPart);
  m_bOpen = true;
  return true;
}

/*
 */
void
MappedFile::close() noexcept
{
  if (m_impl) {
    if (m_impl->view) {
      UnmapViewOfFile(m_impl->view);
    }
    if (m_impl->mapping) {
      CloseHandle(m_impl->mapping);
    }
    if (m_impl->file != INVALID_HANDLE_VALUE) {
      CloseHandle(m_impl->file);
    }
    m_impl.reset();
  }
  m_data = nullptr;
  m_size = 0;
  m_bOpen = false;
}

} // namespace chEngineSDK

#endif // USING(CH_PLATFORM_WIN32)
