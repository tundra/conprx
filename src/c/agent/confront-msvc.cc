//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Console api implementation specific to the windows platform.

#include "agent/confront.hh"

using namespace conprx;
using namespace tclib;

class WindowsConsoleFrontend : public ConsoleFrontend {
public:
  virtual void default_destroy() { default_delete_concrete(this); }

#define __DECLARE_CONAPI_METHOD__(Name, name, FLAGS, SIG, PSIG)                \
  virtual SIG(GET_SIG_RET) name SIG(GET_SIG_PARAMS);
  FOR_EACH_CONAPI_FUNCTION(__DECLARE_CONAPI_METHOD__)
#undef __DECLARE_CONAPI_METHOD__

  virtual dword_t get_last_error();
};

handle_t WindowsConsoleFrontend::get_std_handle(dword_t handle) {
  return GetStdHandle(handle);
}

bool_t WindowsConsoleFrontend::write_console_a(handle_t console_output, const void *buffer,
    dword_t chars_to_write, dword_t *chars_written, void *reserved) {
  return WriteConsoleA(console_output, buffer, chars_to_write, chars_written, reserved);
}

dword_t WindowsConsoleFrontend::get_console_title_a(str_t str, dword_t n) {
  return GetConsoleTitleA(str, n);
}

bool_t WindowsConsoleFrontend::set_console_title_a(ansi_cstr_t str) {
  return SetConsoleTitleA(str);
}

uint32_t WindowsConsoleFrontend::get_console_cp() {
  return GetConsoleCP();
}

bool_t WindowsConsoleFrontend::set_console_cp(uint32_t value) {
  return SetConsoleCP(value);
}

dword_t WindowsConsoleFrontend::get_last_error() {
  return GetLastError();
}

pass_def_ref_t<ConsoleFrontend> ConsoleFrontend::new_native() {
  return pass_def_ref_t<ConsoleFrontend>(new (kDefaultAlloc) WindowsConsoleFrontend());
}
