//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Windows-specific implementation of binary patching.

#include "conapi.hh"

using namespace conprx;

class WindowsMemoryManager : public MemoryManager {
public:
  virtual bool open_for_writing(Vector<byte_t> region,
      standalone_dword_t *old_perms, MessageSink *messages);
  virtual bool close_for_writing(Vector<byte_t> region,
      standalone_dword_t old_perms, MessageSink *messages);
  virtual Vector<byte_t> alloc_executable(address_t addr, size_t size,
      MessageSink *messages);
  virtual bool free_block(Vector<byte_t> block);
};

bool WindowsMemoryManager::open_for_writing(Vector<byte_t> region,
    standalone_dword_t *old_perms, MessageSink *messages) {
  dword_t temp_old_perms = 0;
  bool result = VirtualProtect(region.start(), region.length(), PAGE_EXECUTE_READWRITE,
      &temp_old_perms);
  if (!result) {
    return REPORT_MESSAGE(messages, "VirtualProtect(PAGE_EXECUTE_READWRITE) failed: %i",
        static_cast<int>(GetLastError()));
  }
  *old_perms = temp_old_perms;
  return true;
}

bool WindowsMemoryManager::close_for_writing(Vector<byte_t> region,
    standalone_dword_t old_perms, MessageSink *messages) {
  dword_t dummy_perms = 0;
  bool result = VirtualProtect(region.start(), region.length(), old_perms, &dummy_perms);
  if (!result) {
    return REPORT_MESSAGE(messages, "VirtualProtect(%i) failed: %i", old_perms,
        static_cast<int>(GetLastError()));
  }
  return true;
}

Vector<byte_t> WindowsMemoryManager::alloc_executable(address_t addr, size_t size,
    MessageSink *messages) {
  void *memory = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
  if (memory == NULL) {
    REPORT_MESSAGE(messages, "VirtualAlloc failed: %i",
        static_cast<int>(GetLastError()));
    return Vector<byte_t>();
  }
  return Vector<byte_t>(static_cast<address_t>(memory), size);
}

bool WindowsMemoryManager::free_block(Vector<byte_t> block) {
  return VirtualFree(block.start(), 0, MEM_RELEASE);
}

MemoryManager &MemoryManager::get() {
  static WindowsMemoryManager *instance = NULL;
  if (instance == NULL)
    instance = new WindowsMemoryManager();
  return *instance;
}
