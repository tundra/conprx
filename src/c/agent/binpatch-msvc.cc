//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Windows-specific implementation of binary patching.

#include "conapi.hh"

class WindowsMemoryManager : public MemoryManager {
public:
  virtual bool open_for_writing(Vector<byte_t> region, standalone_dword_t *old_perms);
  virtual bool close_for_writing(Vector<byte_t> region, standalone_dword_t old_perms);
  virtual Vector<byte_t> alloc_executable(address_t addr, size_t size);
};

bool WindowsMemoryManager::open_for_writing(Vector<byte_t> region, standalone_dword_t *old_perms) {
  dword_t temp_old_perms = 0;
  bool result = VirtualProtect(region.start(), region.length(), PAGE_EXECUTE_READWRITE,
      &temp_old_perms);
  if (!result)
    WARN("VirtualProtect(PAGE_EXECUTE_READWRITE) failed: %i",
        static_cast<int>(GetLastError()));
  *old_perms = temp_old_perms;
  return result;
}

bool WindowsMemoryManager::close_for_writing(Vector<byte_t> region, standalone_dword_t old_perms) {
  dword_t dummy_perms = 0;
  bool result = VirtualProtect(region.start(), region.length(), old_perms, &dummy_perms);
  if (!result)
    WARN("VirtualProtect(%i) failed: %i", old_perms,
        static_cast<int>(GetLastError()));
  return result;
}

Vector<byte_t> WindowsMemoryManager::alloc_executable(address_t addr, size_t size) {
  void *memory = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
  if (memory == NULL) {
    WARN("VirtualAlloc failed: %i", static_cast<int>(GetLastError()));
    return Vector<byte_t>();
  }
  return Vector<byte_t>(static_cast<address_t>(memory), size);
}

MemoryManager &MemoryManager::get() {
  static WindowsMemoryManager *instance = NULL;
  if (instance == NULL)
    instance = new WindowsMemoryManager();
  return *instance;
}
