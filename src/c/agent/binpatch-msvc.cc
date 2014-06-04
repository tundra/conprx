//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Windows-specific implementation of binary patching.

#include "conapi.hh"

class WindowsMemoryManager : public MemoryManager {
public:
  virtual bool open_for_writing(Vector<byte_t> region, dword_t *old_perms);
  virtual bool close_for_writing(Vector<byte_t> region, dword_t old_perms);
  virtual Vector<byte_t> alloc_executable(address_t addr, size_t size);
};

bool WindowsMemoryManager::open_for_writing(Vector<byte_t> region, dword_t *old_perms) {
  return VirtualProtect(region.start(), region.length(), PAGE_EXECUTE_READWRITE, old_perms);
}

bool WindowsMemoryManager::close_for_writing(Vector<byte_t> region, dword_t old_perms) {
  dword_t dummy_perms = 0;
  return VirtualProtect(region.start(), region.length(), old_perms, &dummy_perms);
}

Vector<byte_t> WindowsMemoryManager::alloc_executable(address_t addr, size_t size) {
  void *memory = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
  if (memory == NULL) {
    int error = GetLastError();
    LOG_WARNING("VirtualAlloc failed: %i", error);
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
