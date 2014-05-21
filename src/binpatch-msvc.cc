//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Windows-specific implementation of binary patching.

#include "conapi.hh"

class WindowsPatchEngine : public PatchEngine {
public:
  virtual bool try_open_page_for_writing(address_t addr, dword_t *old_permissions);
  virtual bool try_close_page_for_writing(address_t addr, dword_t old_permissions);
};

bool WindowsPatchEngine::try_open_page_for_writing(address_t addr, dword_t *old_perms) {
  return VirtualProtect(addr, kPatchSizeBytes, PAGE_EXECUTE_READWRITE, old_perms);
}

bool WindowsPatchEngine::try_close_page_for_writing(address_t addr, dword_t old_perms) {
  dword_t dummy_perms = 0;
  return VirtualProtect(addr, kPatchSizeBytes, old_perms, &dummy_perms);
}

PatchEngine &PatchEngine::get() {
  static WindowsPatchEngine *instance = NULL;
  if (instance == NULL)
    instance = new WindowsPatchEngine();
  return *instance;
}
