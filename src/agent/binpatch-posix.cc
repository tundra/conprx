//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Posix-specific implementation of binary patching.

#include <sys/mman.h>
#include <unistd.h>

#include "log.hh"


class PosixPatchEngine : public PatchEngine {
public:
  PosixPatchEngine();
  virtual bool ensure_initialized();
  virtual bool try_open_page_for_writing(address_t addr, dword_t *old_perms);
  virtual bool try_close_page_for_writing(address_t addr, dword_t old_perms);
  virtual address_t alloc_executable(address_t addr, size_t size);
private:
  bool is_initialized_;
  size_t page_size_;

  // Attempts to set the permissions of the page containing the given address
  // to the given value.
  bool try_set_page_permissions(address_t addr, int prot);
};

PosixPatchEngine::PosixPatchEngine()
  : is_initialized_(false)
  , page_size_(0) { }

bool PosixPatchEngine::ensure_initialized() {
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

bool PosixPatchEngine::try_open_page_for_writing(address_t addr, dword_t *old_perms) {
  // TODO: store the old permissions rather than assume they're read+exec. As
  //   far as I've been able to determine you're meant to get this info by
  //   parsing stuff from /proc which seems ridiculous.
  *old_perms = 0;
  bool result = try_set_page_permissions(addr, PROT_READ | PROT_WRITE | PROT_EXEC);
  if (!result)
    LOG_ERROR("Failed to open %p for writing", addr);
  return result;
}

bool PosixPatchEngine::try_close_page_for_writing(address_t addr, dword_t old_perms) {
  bool result = try_set_page_permissions(addr, PROT_READ | PROT_EXEC);
  if (!result)
    LOG_ERROR("Failed to close %p after writing", addr);
  return result;
}

bool PosixPatchEngine::try_set_page_permissions(address_t addr, int prot) {
  // TODO: deal with the case where the function is spread over multiple pages.
  address_arith_t start_addr = reinterpret_cast<address_arith_t>(addr);
  address_arith_t page_addr = start_addr & ~(this->page_size_ - 1);
  void *page_ptr = reinterpret_cast<void*>(page_addr);
  if (mprotect(page_ptr, this->page_size_, prot) == -1)
    return false;
  return true;
}

address_t PosixPatchEngine::alloc_executable(address_t addr, size_t size) {
  address_arith_t addr_val = reinterpret_cast<address_arith_t>(addr);
  if ((addr_val & 0xFFFFFFFF) != addr_val) {
    LOG_ERROR("Cannot allocate executable memory near %p (%ib)", addr, size);
    // All we can do to allocate near the given address is to rely on it being
    // in the bottom 2G and then ask mmap to give us memory there. If it's not
    // there there's nothing we can do.
    return NULL;
  }
  void *result = mmap(NULL, page_size_, PROT_READ | PROT_WRITE | PROT_EXEC,
      MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, 0, 0);
  return static_cast<address_t>(result);
}

PatchEngine &PatchEngine::get() {
  static PosixPatchEngine *instance = NULL;
  if (instance == NULL)
    instance = new PosixPatchEngine();
  return *instance;
}
