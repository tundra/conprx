//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Windows-specific implementation of binary patching.

#include "conapi.hh"

class WindowsMemoryManager : public MemoryManager {
public:
  virtual bool try_open_page_for_writing(address_t addr, dword_t *old_perms);
  virtual bool try_close_page_for_writing(address_t addr, dword_t old_perms);
  virtual Vector<byte_t> alloc_executable(address_t addr, size_t size);
};

bool WindowsMemoryManager::try_open_page_for_writing(address_t addr, dword_t *old_perms) {
  return VirtualProtect(addr, kPatchSizeBytes, PAGE_EXECUTE_READWRITE, old_perms);
}

bool WindowsMemoryManager::try_close_page_for_writing(address_t addr, dword_t old_perms) {
  dword_t dummy_perms = 0;
  return VirtualProtect(addr, kPatchSizeBytes, old_perms, &dummy_perms);
}

Vector<byte_t> WindowsMemoryManager::alloc_executable(address_t addr, size_t size) {
  return Vector<byte_t>();
}

MemoryManager &MemoryManager::get() {
  static WindowsMemoryManager *instance = NULL;
  if (instance == NULL)
    instance = new WindowsMemoryManager();
  return *instance;
}
