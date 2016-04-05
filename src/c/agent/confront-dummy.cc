//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Console api implementation specific to the windows platform.

#include "confront.hh"

BEGIN_C_INCLUDES
#include "utils/string-inl.h"
END_C_INCLUDES

using namespace conprx;
using namespace tclib;

// The fallback console is a console implementation that works on all platforms.
// The implementation is just placeholders but they should be functional enough
// that they can be used for testing.
class DummyConsoleFrontend : public ConsoleFrontend {
public:
  DummyConsoleFrontend();
  ~DummyConsoleFrontend();
  virtual void default_destroy() { tclib::default_delete_concrete(this); }

#define __DECLARE_CONAPI_METHOD__(Name, name, FLAGS, SIG, PSIG)                \
  virtual SIG(GET_SIG_RET) name SIG(GET_SIG_PARAMS);
  FOR_EACH_CONAPI_FUNCTION(__DECLARE_CONAPI_METHOD__)
#undef __DECLARE_CONAPI_METHOD__

  virtual NtStatus get_last_error();

private:
  utf8_t title_;
};

DummyConsoleFrontend::DummyConsoleFrontend()
  : title_(string_empty()) {
  title_ = string_default_dup(new_c_string("(fallback console default title)"));
}

DummyConsoleFrontend::~DummyConsoleFrontend() {
  string_default_delete(title_);
}

handle_t DummyConsoleFrontend::get_std_handle(dword_t n) {
  int32_t sn = static_cast<int32_t>(n);
  ssize_t result = (-12 <= sn && sn <= -10)
      ? sn + 18
      : IF_32_BIT(-1, -1LL);
  return reinterpret_cast<handle_t>(result);
}

bool_t DummyConsoleFrontend::write_console_a(handle_t output, const void *buffer,
    dword_t chars_to_write, dword_t *chars_written, void *reserved) {
  *chars_written = 0;
  return false;
}

bool_t DummyConsoleFrontend::write_console_w(handle_t output, const void *buffer,
    dword_t chars_to_write, dword_t *chars_written, void *reserved) {
  *chars_written = 0;
  return false;
}

static size_t min_size(size_t a, size_t b) {
  return (a < b) ? a : b;
}

dword_t DummyConsoleFrontend::get_console_title_a(ansi_str_t str, dword_t n) {
  size_t count = min_size(title_.size + 1, n);
  utf8_t title = string_substring(title_, 0, count);
  string_copy_to(title, str, count);
  return static_cast<dword_t>(count);
}

bool_t DummyConsoleFrontend::set_console_title_a(ansi_cstr_t str) {
  string_default_delete(title_);
  title_ = string_default_dup(new_c_string(str));
  return true;
}

dword_t DummyConsoleFrontend::get_console_title_w(wide_str_t str, dword_t n) {
  return 0;
}

bool_t DummyConsoleFrontend::set_console_title_w(wide_cstr_t str) {
  return false;
}

uint32_t DummyConsoleFrontend::get_console_cp() {
  return 0xB00B00;
}

bool_t DummyConsoleFrontend::set_console_cp(uint32_t value) {
  return false;
}

uint32_t DummyConsoleFrontend::get_console_output_cp() {
  return 0xF00F00;
}

bool_t DummyConsoleFrontend::set_console_output_cp(uint32_t value) {
  return false;
}

bool_t DummyConsoleFrontend::get_console_mode(handle_t handle, dword_t *mode_out) {
  return false;
}

bool_t DummyConsoleFrontend::set_console_mode(handle_t handle, dword_t mode) {
  return false;
}

bool_t DummyConsoleFrontend::get_console_screen_buffer_info(handle_t handle,
    console_screen_buffer_info_t *info_out) {
  return false;
}

bool_t DummyConsoleFrontend::get_console_screen_buffer_info_ex(handle_t handle,
    console_screen_buffer_infoex_t *info_out) {
  return false;
}

NtStatus DummyConsoleFrontend::get_last_error() {
  return NtStatus::success();
}

pass_def_ref_t<ConsoleFrontend> ConsoleFrontend::new_dummy() {
  return new (kDefaultAlloc) DummyConsoleFrontend();
}
