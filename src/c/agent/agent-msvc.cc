//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "utils/types.hh"

static bool dll_process_attach() {
  return ConsoleAgent::install(Console::logger());
}

bool APIENTRY DllMain(module_t module, dword_t reason, void *) {
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      return dll_process_attach();
  }
  return true;
}

address_t ConsoleAgent::get_console_function_address(c_str_t name) {
  module_t kernel32 = GetModuleHandle(TEXT("kernel32.dll"));
  return Code::upcast(GetProcAddress(kernel32, name));
}
