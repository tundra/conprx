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

#define LOG(...) LOG_INFO(__VA_ARGS__)

bool_t LoggingConsole::alloc_console() {
  LOG("AllocConsole()");
  return delegate().alloc_console();
}

dword_t LoggingConsole::get_console_title_a(ansi_str_t console_title,
    dword_t size) {
  dword_t result = delegate().get_console_title_a(console_title, size);
  LOG("GetConsoleTitleA(%s) = %i", console_title, result);
  return result;
}

dword_t LoggingConsole::get_console_title_w(wide_str_t console_title,
    dword_t size) {
  dword_t result = delegate().get_console_title_w(console_title, size);
  LOG("GetConsoleTitleW(%S) = %i", console_title, result);
  return result;
}

handle_t LoggingConsole::get_std_handle(dword_t std_handle) {
  handle_t result = delegate().get_std_handle(std_handle);
  LOG("GetStdHandle(%i) = %p", std_handle, result);
  return result;
}

bool_t LoggingConsole::set_console_title_a(ansi_cstr_t console_title) {
  LOG("SetConsoleTitleA(%s)", console_title);
  return delegate().set_console_title_a(console_title);
}

bool_t LoggingConsole::set_console_title_w(wide_cstr_t console_title) {
  LOG("SetConsoleTitleW(%S)", console_title);
  return delegate().set_console_title_w(console_title);
}

bool_t LoggingConsole::write_console_a(handle_t console_output, const void *buffer,                            \
       dword_t number_of_chars_to_write, dword_t *number_of_chars_written,
       void *reserved) {
  LOG("WriteConsoleA(%i)", number_of_chars_to_write);
  return delegate().write_console_a(console_output, buffer, number_of_chars_to_write,
      number_of_chars_written, reserved);
}

bool_t LoggingConsole::write_console_w(handle_t console_output, const void *buffer,                            \
       dword_t number_of_chars_to_write, dword_t *number_of_chars_written,
       void *reserved) {
  LOG("WriteConsoleW(%i)", number_of_chars_to_write);
  return delegate().write_console_w(console_output, buffer, number_of_chars_to_write,
      number_of_chars_written, reserved);
}

#ifdef IS_MSVC
#include "conapi-msvc.cc"
#endif
