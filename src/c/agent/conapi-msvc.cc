//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Console api implementation specific to the windows platform.

using namespace conprx;
using namespace tclib;

class WindowsConsole : public Console {
public:
  virtual void default_destroy() { default_delete_concrete(this); }

#define __DECLARE_CONAPI_METHOD__(Name, name, FLAGS, SIG, PSIG)                \
  virtual SIG(GET_SIG_RET) name SIG(GET_SIG_PARAMS);
  FOR_EACH_CONAPI_FUNCTION(__DECLARE_CONAPI_METHOD__)
#undef __DECLARE_CONAPI_METHOD__
};

handle_t WindowsConsole::get_std_handle(dword_t handle) {
  return GetStdHandle(handle);
}

bool_t WindowsConsole::write_console_a(handle_t console_output, const void *buffer,
    dword_t chars_to_write, dword_t *chars_written, void *reserved) {
  return WriteConsoleA(console_output, buffer, chars_to_write, chars_written, reserved);
}

dword_t WindowsConsole::get_console_title_a(str_t str, dword_t n) {
  return GetConsoleTitleA(str, n);
}

bool_t WindowsConsole::set_console_title_a(ansi_cstr_t str) {
  return SetConsoleTitleA(str);
}

dword_t Console::get_last_error() {
  return GetLastError();
}

pass_def_ref_t<Console> Console::new_native() {
  return pass_def_ref_t<Console>(new (kDefaultAlloc) WindowsConsole());
}
