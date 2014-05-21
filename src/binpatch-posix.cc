//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Posix-specific implementation of binary patching.

#include <sys/mman.h>
#include <unistd.h>

class PosixPatchEngine : public PatchEngine {
public:
  PosixPatchEngine();
  virtual bool ensure_initialized();
  virtual bool try_open_page_for_writing(address_t addr, dword_t *old_perms);
  virtual bool try_close_page_for_writing(address_t addr, dword_t old_perms);
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
  if (page_size == -1)
    // Failed to get the page size; this is definitely not going to work.
    return false;
  this->page_size_ = page_size;
  this->is_initialized_ = true;
  return true;
}

bool PosixPatchEngine::try_open_page_for_writing(address_t addr, dword_t *old_perms) {
  // TODO: store the old permissions rather than assume they're read+exec. As
  //   far as I've been able to determine you're meant to get this info by
  //   parsing stuff from /proc which seems ridiculous.
  *old_perms = 0;
  return try_set_page_permissions(addr, PROT_READ | PROT_WRITE | PROT_EXEC);
}

bool PosixPatchEngine::try_close_page_for_writing(address_t addr, dword_t old_perms) {
  return try_set_page_permissions(addr, PROT_READ | PROT_EXEC);
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

PatchEngine &PatchEngine::get() {
  static PosixPatchEngine *instance = NULL;
  if (instance == NULL)
    instance = new PosixPatchEngine();
  return *instance;
}
