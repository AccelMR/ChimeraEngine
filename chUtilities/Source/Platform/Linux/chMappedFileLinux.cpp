/************************************************************************/
/**
 * @file chMappedFileLinux.cpp
 * @author AccelMR
 * @date 2026/11/20
 * @brief
 *  MappedFile on Linux: open and mmap.
 */
/************************************************************************/
#include "chMappedFile.h"

#if USING(CH_PLATFORM_LINUX)

#include "chFileSystem.h"
#include "chLogger.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace chEngineSDK {
CH_LOG_DECLARE_STATIC(MappedFileLog, All);

struct MappedFile::Impl {
  int32 descriptor = -1;
  void* view = nullptr;
  SIZE_T size = 0;
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
  impl.descriptor = ::open(realFile.toString().c_str(), O_RDONLY | O_CLOEXEC);
  if (impl.descriptor < 0) {
    CH_LOG_ERROR(MappedFileLog, "Could not open {0}: {1}", realFile,
                 String(std::strerror(errno)));
    return false;
  }

  struct stat status{};
  if (fstat(impl.descriptor, &status) != 0) {
    CH_LOG_ERROR(MappedFileLog, "Could not read the size of {0}.", realFile);
    ::close(impl.descriptor);
    return false;
  }
  impl.size = static_cast<SIZE_T>(status.st_size);

  // mmap refuses a length of zero; an empty file opens with no data.
  if (impl.size > 0) {
    impl.view = mmap(nullptr, impl.size, PROT_READ, MAP_PRIVATE, impl.descriptor, 0);
    if (impl.view == MAP_FAILED) {
      CH_LOG_ERROR(MappedFileLog, "Could not map {0}: {1}", realFile,
                   String(std::strerror(errno)));
      ::close(impl.descriptor);
      return false;
    }
  }

  m_impl = chMakeUnique<Impl>(impl);
  m_data = static_cast<const uint8*>(impl.view);
  m_size = impl.size;
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
      munmap(m_impl->view, m_impl->size);
    }
    if (m_impl->descriptor >= 0) {
      ::close(m_impl->descriptor);
    }
    m_impl.reset();
  }
  m_data = nullptr;
  m_size = 0;
  m_bOpen = false;
}

} // namespace chEngineSDK

#endif // USING(CH_PLATFORM_LINUX)
