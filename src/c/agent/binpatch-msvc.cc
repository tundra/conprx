//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Windows-specific implementation of binary patching.

#include "confront.hh"

using namespace conprx;

class WindowsMemoryManager : public MemoryManager {
public:
  virtual fat_bool_t open_for_writing(tclib::Blob region,
      uint32_t *old_perms);
  virtual fat_bool_t close_for_writing(tclib::Blob region,
      uint32_t old_perms);
  virtual fat_bool_t alloc_executable(address_t addr, size_t size, Blob *blob_out);
  virtual fat_bool_t free_block(tclib::Blob block);
};

fat_bool_t WindowsMemoryManager::open_for_writing(tclib::Blob region,
    uint32_t *old_perms) {
  dword_t temp_old_perms = 0;
  bool result = VirtualProtect(region.start(), region.size(), PAGE_EXECUTE_READWRITE,
      &temp_old_perms);
  if (!result) {
    LOG_ERROR("VirtualProtect(PAGE_EXECUTE_READWRITE) failed: %i", GetLastError());
    return F_FALSE;
  }
  *old_perms = temp_old_perms;
  return F_TRUE;
}

fat_bool_t WindowsMemoryManager::close_for_writing(tclib::Blob region,
    uint32_t old_perms) {
  dword_t dummy_perms = 0;
  bool result = VirtualProtect(region.start(), region.size(), old_perms, &dummy_perms);
  if (!result) {
    LOG_ERROR("VirtualProtect(%i) failed: %i", old_perms, GetLastError());
    return F_FALSE;
  }
  return F_TRUE;
}

fat_bool_t WindowsMemoryManager::alloc_executable(address_t addr, size_t size,
    Blob *blob_out) {
  void *memory = VirtualAlloc(addr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
  if (memory == NULL) {
    LOG_ERROR("VirtualAlloc failed: %i", GetLastError());
    return F_FALSE;
  }
  *blob_out = tclib::Blob(memory, size);
  return F_TRUE;
}

fat_bool_t WindowsMemoryManager::free_block(Blob block) {
  return F_BOOL(VirtualFree(block.start(), 0, MEM_RELEASE));
}

MemoryManager &MemoryManager::get() {
  static WindowsMemoryManager *instance = NULL;
  if (instance == NULL)
    instance = new WindowsMemoryManager();
  return *instance;
}
