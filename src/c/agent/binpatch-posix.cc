//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Posix-specific implementation of binary patching.

#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

#include "agent/binpatch.hh"

using namespace conprx;

class PosixMemoryManager : public MemoryManager {
public:
  PosixMemoryManager();
  virtual bool ensure_initialized();
  virtual bool open_for_writing(tclib::Blob region, standalone_dword_t *old_perms);
  virtual bool close_for_writing(tclib::Blob region, standalone_dword_t old_perms);
  virtual tclib::Blob alloc_executable(address_t addr, size_t size);
  virtual bool free_block(tclib::Blob block);
private:
  bool is_initialized_;
  size_t page_size_;

  // Attempts to set the permissions of the page containing the given address
  // to the given value.
  bool set_permissions(tclib::Blob region, int prot);
};

PosixMemoryManager::PosixMemoryManager()
  : is_initialized_(false)
  , page_size_(0) { }

bool PosixMemoryManager::ensure_initialized() {
  if (is_initialized_)
    return true;
  int page_size = static_cast<int>(sysconf(_SC_PAGE_SIZE));
  if (page_size == -1) {
    // Failed to get the page size; this is definitely not going to work.
    LOG_ERROR("Failed sysconf(%i)", _SC_PAGE_SIZE);
    return false;
  }
  page_size_ = page_size;
  is_initialized_ = true;
  return true;
}

bool PosixMemoryManager::open_for_writing(tclib::Blob region,
    standalone_dword_t *old_perms) {
  // TODO: store the old permissions rather than assume they're read+exec. As
  //   far as I've been able to determine you're meant to get this info by
  //   parsing stuff from /proc which seems ridiculous.
  *old_perms = PROT_READ | PROT_EXEC;
  return set_permissions(region, PROT_READ | PROT_WRITE | PROT_EXEC);
}

bool PosixMemoryManager::close_for_writing(tclib::Blob region,
    standalone_dword_t old_perms) {
  return set_permissions(region, old_perms);
}

bool PosixMemoryManager::set_permissions(tclib::Blob region, int prot) {
  // Determine the range of addresses to set.
  address_arith_t start_addr = reinterpret_cast<address_arith_t>(region.start());
  address_arith_t end_addr = reinterpret_cast<address_arith_t>(region.end());
  // Align the start to a page boundary.
  address_arith_t page_addr = start_addr & ~(page_size_ - 1);
  // Scan through the pages one by one.
  while (page_addr <= end_addr) {
    errno = 0;
    int ret = mprotect(reinterpret_cast<void*>(page_addr), page_size_, prot);
    if (ret == -1) {
      LOG_ERROR("Failure: mprotect(%p, %i, %i): %i", page_addr, page_size_,
          prot, errno);
      return false;
    }
    page_addr += page_size_;
  }
  return true;
}

tclib::Blob PosixMemoryManager::alloc_executable(address_t addr, size_t size) {
  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
  int protect = PROT_READ | PROT_WRITE | PROT_EXEC;
  errno = 0;
  void *result = mmap(NULL, page_size_, protect, flags, 0, 0);
  if (result == MAP_FAILED) {
    LOG_ERROR("mmap(NULL, %i, %i, %i, 0, 0): %i", page_size_, protect, flags,
        errno);
    return tclib::Blob();
  }
  return tclib::Blob(result, size);
}

bool PosixMemoryManager::free_block(tclib::Blob block) {
  return munmap(block.start(), block.size()) == 0;
}

MemoryManager &MemoryManager::get() {
  static PosixMemoryManager *instance = NULL;
  if (instance == NULL)
    instance = new PosixMemoryManager();
  return *instance;
}
