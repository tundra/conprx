//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Console api implementation specific to the windows platform.

#include "conapi.hh"

BEGIN_C_INCLUDES
#include "utils/string-inl.h"
END_C_INCLUDES

using namespace conprx;
using namespace tclib;

// The fallback console is a console implementation that works on all platforms.
// The implementation is just placeholders but they should be functional enough
// that they can be used for testing.
class FallbackConsole : public Console {
public:
  FallbackConsole();
  ~FallbackConsole();
  virtual void default_destroy() { tclib::default_delete_concrete(this); }

#define __DECLARE_CONAPI_METHOD__(Name, name, FLAGS, SIG, PSIG)                \
  virtual SIG(GET_SIG_RET) name SIG(GET_SIG_PARAMS);
  FOR_EACH_CONAPI_FUNCTION(__DECLARE_CONAPI_METHOD__)
#undef __DECLARE_CONAPI_METHOD__

private:
  utf8_t title_;
};

FallbackConsole::FallbackConsole()
  : title_(string_empty()) {
  title_ = string_default_dup(new_c_string("(fallback console default title)"));
}

FallbackConsole::~FallbackConsole() {
  string_default_delete(title_);
}

handle_t FallbackConsole::get_std_handle(dword_t n_std_handle) {
  dword_t result = (-12UL <= n_std_handle && n_std_handle <= -10UL)
      ? n_std_handle + 18
      : -1;
  return reinterpret_cast<handle_t>(result);
}

bool FallbackConsole::write_console_a(handle_t console_output, const void *buffer,
    dword_t chars_to_write, dword_t *chars_written, void *reserved) {
  *chars_written = chars_to_write;
  return true;
}

static size_t min_size(size_t a, size_t b) {
  return (a < b) ? a : b;
}

dword_t FallbackConsole::get_console_title_a(str_t str, dword_t n) {
  size_t count = min_size(title_.size + 1, n);
  utf8_t title = string_substring(title_, 0, count);
  string_copy_to(title, str, count);
  return static_cast<dword_t>(count);
}

bool FallbackConsole::set_console_title_a(ansi_cstr_t str) {
  string_default_delete(title_);
  title_ = string_default_dup(new_c_string(str));
  return true;
}

dword_t Console::get_last_error() {
  return 0xFABACAEA;
}

pass_def_ref_t<Console> Console::new_native() {
  return pass_def_ref_t<Console>(new (kDefaultAlloc) FallbackConsole());
}
