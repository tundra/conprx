//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Console api implementation specific to the windows platform.

#include "confront.hh"
#include "conback.hh"

BEGIN_C_INCLUDES
#include "utils/string-inl.h"
END_C_INCLUDES

using namespace conprx;
using namespace tclib;

// The fallback console is a console implementation that works on all platforms.
// The implementation is just placeholders but they should be functional enough
// that they can be used for testing.
class SimulatingConsoleFrontend : public ConsoleFrontend {
public:
  SimulatingConsoleFrontend(ConsoleBackend *backend);
  ~SimulatingConsoleFrontend();
  virtual void default_destroy() { tclib::default_delete_concrete(this); }

#define __DECLARE_CONAPI_METHOD__(Name, name, FLAGS, SIG, PSIG)                \
  virtual SIG(GET_SIG_RET) name SIG(GET_SIG_PARAMS);
  FOR_EACH_CONAPI_FUNCTION(__DECLARE_CONAPI_METHOD__)
#undef __DECLARE_CONAPI_METHOD__

  virtual dword_t get_last_error();

  virtual int64_t poke_backend(int64_t value);

private:
  ConsoleBackend *backend() { return backend_; }
  ConsoleBackend *backend_;

};

SimulatingConsoleFrontend::SimulatingConsoleFrontend(ConsoleBackend *backend)
  : backend_(backend) {
}

SimulatingConsoleFrontend::~SimulatingConsoleFrontend() {
}

int64_t SimulatingConsoleFrontend::poke_backend(int64_t value) {
  return backend()->poke(value);
}

handle_t SimulatingConsoleFrontend::get_std_handle(dword_t n_std_handle) {
  return NULL;
}

bool_t SimulatingConsoleFrontend::write_console_a(handle_t console_output, const void *buffer,
    dword_t chars_to_write, dword_t *chars_written, void *reserved) {
  return false;
}

dword_t SimulatingConsoleFrontend::get_console_title_a(str_t str, dword_t n) {
  return 0;
}

bool_t SimulatingConsoleFrontend::set_console_title_a(ansi_cstr_t str) {
  return false;
}

dword_t SimulatingConsoleFrontend::get_last_error() {
  return 0xFABACAEA;
}

pass_def_ref_t<ConsoleFrontend> ConsoleFrontend::new_simulating(ConsoleBackend *backend) {
  return pass_def_ref_t<ConsoleFrontend>(new (kDefaultAlloc) SimulatingConsoleFrontend(backend));
}
