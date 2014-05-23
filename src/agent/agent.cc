//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent.hh"
#include "binpatch.hh"
#include "conapi.hh"
#include "utils/log.hh"

// Controls the injection of the console agent.
class ConsoleAgent {
public:
  // Install the console agent instead of the built-in console.
  static bool install(Console &console);

private:
  // Returns the address of the console function with the given name.
  static function_t get_console_function_address(c_str_t name);

  // The console object currently being delegated to.
  static Console *delegate;

  static bool WINAPI write_console_a_bridge(handle_t console_output,
      const void *buffer, dword_t number_of_chars_to_write,
      dword_t *number_of_chars_written, void *reserved) {
    return delegate->write_console_a(console_output, buffer, number_of_chars_to_write,
        number_of_chars_written, reserved);
  }

};

Console *ConsoleAgent::delegate = NULL;

bool ConsoleAgent::install(Console &installed_delegate) {
  if (delegate != NULL) {
    LOG_ERROR("Console agent has already been installed");
    return false;
  }
  delegate = &installed_delegate;
  function_t write_console_a = get_console_function_address(TEXT("WriteConsoleA"));
  BinaryPatch patch(write_console_a, FUNCAST(write_console_a_bridge), 0);
  PatchEngine &engine = PatchEngine::get();
  if (!engine.ensure_initialized())
    return false;
  if (!patch.is_tentatively_possible()) {
    LOG_ERROR("Patch is impossible");
    return false;
  }
  if (!patch.apply(engine)) {
    LOG_ERROR("Failed to patch");
    return false;
  }
  return true;
}

#ifdef IS_MSVC
#include "agent-msvc.cc"
#else
#include "agent-posix.cc"
#endif
