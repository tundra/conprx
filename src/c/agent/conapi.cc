//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Shared console api implementation.

#include "conapi.hh"
#include "utils/log.hh"

Console::~Console() { }

// Console implementation that just logs everything.
class LoggingConsole : public Console {
public:
  virtual bool write_console_a(handle_t console_output, const void *buffer,
    dword_t number_of_chars_to_write, dword_t *number_of_chars_written,
    void *reserved);
};

bool LoggingConsole::write_console_a(handle_t console_output, const void *buffer,
  dword_t number_of_chars_to_write, dword_t *number_of_chars_written,
  void *reserved) {
  LOG_INFO("WriteConsoleA(\"%s\", %i)", buffer, number_of_chars_to_write);
  *number_of_chars_written = number_of_chars_to_write;
  return true;
}

Console &Console::logger() {
  static LoggingConsole *instance = NULL;
  if (instance == NULL)
    instance = new LoggingConsole();
  return *instance;
}

#ifdef IS_MSVC
#include "conapi-msvc.cc"
#endif
