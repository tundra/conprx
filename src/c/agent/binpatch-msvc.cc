//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Windows-specific implementation of binary patching.

#include "conapi.hh"

using namespace conprx;

class WindowsMemoryManager : public MemoryManager {
public:
  virtual bool open_for_writing(tclib::Blob region,
      standalone_dword_t *old_perms, MessageSink *messages);
  virtual bool close_for_writing(tclib::Blob region,
      standalone_dword_t old_perms, MessageSink *messages);
  virtual tclib::Blob alloc_executable(address_t addr, size_t size,
      MessageSink *messages);
  virtual bool free_block(tclib::Blob block);
};

bool WindowsMemoryManager::open_for_writing(tclib::Blob region,
    standalone_dword_t *old_perms, MessageSink *messages) {
  dword_t temp_old_perms = 0;
  bool result = VirtualProtect(region.start(), region.size(), PAGE_EXECUTE_READWRITE,
      &temp_old_perms);
  if (!result) {
    return REPORT_MESSAGE(messages, "VirtualProtect(PAGE_EXECUTE_READWRITE) failed: %i",
        static_cast<int>(GetLastError()));
  }
  *old_perms = temp_old_perms;
  return true;
}

bool WindowsMemoryManager::close_for_writing(tclib::Blob region,
    standalone_dword_t old_perms, MessageSink *messages) {
  dword_t dummy_perms = 0;
  bool result = VirtualProtect(region.start(), region.size(), old_perms, &dummy_perms);
  if (!result) {
    return REPORT_MESSAGE(messages, "VirtualProtect(%i) failed: %i", old_perms,
        static_cast<int>(GetLastError()));
  }
  return true;
}

tclib::Blob WindowsMemoryManager::alloc_executable(address_t addr, size_t size,
    MessageSink *messages) {
  void *memory = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
  if (memory == NULL) {
    REPORT_MESSAGE(messages, "VirtualAlloc failed: %i",
        static_cast<int>(GetLastError()));
    return tclib::Blob();
  }
  return tclib::Blob(memory, size);
}

bool WindowsMemoryManager::free_block(tclib::Blob block) {
  return VirtualFree(block.start(), 0, MEM_RELEASE);
}

MemoryManager &MemoryManager::get() {
  static WindowsMemoryManager *instance = NULL;
  if (instance == NULL)
    instance = new WindowsMemoryManager();
  return *instance;
}
