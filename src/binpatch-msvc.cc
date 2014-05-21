//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Windows-specific implementation of binary patching.

#include "conapi.hh"

class WindowsPatchEngine : public PatchEngine {
public:
  virtual bool try_open_page_for_writing(address_t addr, dword_t *old_permissions);
  virtual bool try_close_page_for_writing(address_t addr, dword_t old_permissions);
};

bool WindowsPatchEngine::try_open_page_for_writing(address_t addr, dword_t *old_permissions) {
  return VirtualProtect(addr, 5, PAGE_EXECUTE_READWRITE, old_permissions);
}

bool WindowsPatchEngine::try_close_page_for_writing(address_t addr, dword_t old_permissions) {
  dword_t dummy = 0;
  return VirtualProtect(addr, 5, old_permissions, &dummy);
}

PatchEngine &PatchEngine::get() {
  static WindowsPatchEngine *instance = NULL;
  if (instance == NULL)
    instance = new WindowsPatchEngine();
  return *instance;
}
