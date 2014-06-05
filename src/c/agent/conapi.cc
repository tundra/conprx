//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Shared console api implementation.

#include "conapi.hh"
#include "utils/log.hh"

using namespace conprx;

Console::~Console() { }

static Console::FunctionInfo function_info[Console::kFunctionCount + 1] = {
#define __DECLARE_INFO__(Name, name, ...) {TEXT(#Name), Console::name##_key} ,
    FOR_EACH_CONAPI_FUNCTION(__DECLARE_INFO__)
#undef __DECLARE_INFO__
    {NULL, 0}
};

Vector<Console::FunctionInfo> Console::functions() {
  return Vector<FunctionInfo>(function_info, Console::kFunctionCount);
}

// --- L o g g i n g ---

bool_t LoggingConsole::alloc_console() {
  return delegate().alloc_console();
}

bool_t LoggingConsole::write_console_a(handle_t console_output, const void *buffer,                            \
       dword_t number_of_chars_to_write, dword_t *number_of_chars_written,
       void *reserved) {
  return delegate().write_console_a(console_output, buffer, number_of_chars_to_write,
      number_of_chars_written, reserved);
}

#ifdef IS_MSVC
#include "conapi-msvc.cc"
#endif
