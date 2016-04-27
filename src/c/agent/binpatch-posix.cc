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
  virtual fat_bool_t ensure_initialized();
  virtual fat_bool_t open_for_writing(tclib::Blob region, uint32_t *old_perms);
  virtual fat_bool_t close_for_writing(tclib::Blob region, uint32_t old_perms);
  virtual fat_bool_t alloc_executable(address_t addr, size_t size, Blob *blob_out);
  virtual fat_bool_t free_block(Blob block);
private:
  bool is_initialized_;
  size_t page_size_;

  // Attempts to set the permissions of the page containing the given address
  // to the given value.
  fat_bool_t set_permissions(tclib::Blob region, int prot);
};

PosixMemoryManager::PosixMemoryManager()
  : is_initialized_(false)
  , page_size_(0) { }

fat_bool_t PosixMemoryManager::ensure_initialized() {
  if (is_initialized_)
    return F_TRUE;
  int page_size = static_cast<int>(sysconf(_SC_PAGE_SIZE));
  if (page_size == -1) {
    // Failed to get the page size; this is definitely not going to work.
    LOG_ERROR("Failed sysconf(%i)", _SC_PAGE_SIZE);
    return F_FALSE;
  }
  page_size_ = page_size;
  is_initialized_ = true;
  return F_TRUE;
}

fat_bool_t PosixMemoryManager::open_for_writing(tclib::Blob region,
    uint32_t *old_perms) {
  // TODO: store the old permissions rather than assume they're read+exec. As
  //   far as I've been able to determine you're meant to get this info by
  //   parsing stuff from /proc which seems ridiculous.
  *old_perms = PROT_READ | PROT_EXEC;
  return set_permissions(region, PROT_READ | PROT_WRITE | PROT_EXEC);
}

fat_bool_t PosixMemoryManager::close_for_writing(tclib::Blob region,
    uint32_t old_perms) {
  return set_permissions(region, old_perms);
}

fat_bool_t PosixMemoryManager::set_permissions(tclib::Blob region, int prot) {
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
      return F_FALSE;
    }
    page_addr += page_size_;
  }
  return F_TRUE;
}

fat_bool_t PosixMemoryManager::alloc_executable(address_t addr, size_t size,
    tclib::Blob *blob_out) {
  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
  int protect = PROT_READ | PROT_WRITE | PROT_EXEC;
  errno = 0;
  void *result = mmap(addr, size, protect, flags, 0, 0);
  if (result == MAP_FAILED) {
    LOG_ERROR("mmap(%p, %i, %i, %i, 0, 0): %i", addr, size, protect,
        flags, errno);
    return F_FALSE;
  }
  *blob_out = Blob(result, size);
  return F_TRUE;
}

fat_bool_t PosixMemoryManager::free_block(Blob block) {
  return F_BOOL(munmap(block.start(), block.size()) == 0);
}

MemoryManager &MemoryManager::get() {
  static PosixMemoryManager *instance = NULL;
  if (instance == NULL)
    instance = new PosixMemoryManager();
  return *instance;
}
