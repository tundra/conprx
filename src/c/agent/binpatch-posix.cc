//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Posix-specific implementation of binary patching.

#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

#include "utils/log.hh"


class PosixMemoryManager : public MemoryManager {
public:
  PosixMemoryManager();
  virtual bool ensure_initialized();
  virtual bool open_for_writing(Vector<byte_t> region, dword_t *old_perms);
  virtual bool close_for_writing(Vector<byte_t> region, dword_t old_perms);
  virtual Vector<byte_t> alloc_executable(address_t addr, size_t size);
private:
  bool is_initialized_;
  size_t page_size_;

  // Attempts to set the permissions of the page containing the given address
  // to the given value.
  bool try_set_page_permissions(address_t addr, int prot);
  bool set_permissions(Vector<byte_t> region, int prot);
};

PosixMemoryManager::PosixMemoryManager()
  : is_initialized_(false)
  , page_size_(0) { }

bool PosixMemoryManager::ensure_initialized() {
  if (is_initialized_)
    return true;
  int page_size = sysconf(_SC_PAGE_SIZE);
  if (page_size == -1) {
    LOG_ERROR("Failed to determine page size");
    // Failed to get the page size; this is definitely not going to work.
    return false;
  }
  page_size_ = page_size;
  is_initialized_ = true;
  return true;
}

bool PosixMemoryManager::open_for_writing(Vector<byte_t> region, dword_t *old_perms) {
  // TODO: store the old permissions rather than assume they're read+exec. As
  //   far as I've been able to determine you're meant to get this info by
  //   parsing stuff from /proc which seems ridiculous.
  *old_perms = PROT_READ | PROT_EXEC;
  return set_permissions(region, PROT_READ | PROT_WRITE | PROT_EXEC);
}

bool PosixMemoryManager::close_for_writing(Vector<byte_t> region, dword_t old_perms) {
  return set_permissions(region, old_perms);
}

bool PosixMemoryManager::set_permissions(Vector<byte_t> region, int prot) {
  // Determine the range of addresses to set.
  address_arith_t start_addr = reinterpret_cast<address_arith_t>(region.start());
  address_arith_t end_addr = reinterpret_cast<address_arith_t>(region.end());
  // Align the start to a page boundary.
  address_arith_t page_addr = start_addr & ~(page_size_ - 1);
  // Scan through the pages one by one.
  while (page_addr <= end_addr) {
    int ret = mprotect(reinterpret_cast<void*>(page_addr), page_size_, prot);
    if (ret == -1)
      return false;
    page_addr += page_size_;
  }
  return true;
}

Vector<byte_t> PosixMemoryManager::alloc_executable(address_t addr, size_t size) {
  address_arith_t addr_val = reinterpret_cast<address_arith_t>(addr);
  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
  if ((addr_val >> 32) == 0)
    // If the address is in the first 32 bits we ask for memory there. If it's
    // not we're basically screwed but will try anyway, just in case.
    flags |= MAP_32BIT;
  void *result = mmap(NULL, page_size_, PROT_READ | PROT_WRITE | PROT_EXEC,
      flags, 0, 0);
  if (result == NULL) {
    int error = errno;
    LOG_WARNING("mmap failed: %i", error);
    return Vector<byte_t>();
  }
  return Vector<byte_t>(static_cast<byte_t*>(result), size);
}

MemoryManager &MemoryManager::get() {
  static PosixMemoryManager *instance = NULL;
  if (instance == NULL)
    instance = new PosixMemoryManager();
  return *instance;
}
