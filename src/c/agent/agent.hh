//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// # Agent
///
/// The console agent which is the part of the console proxy infrastructure that
/// translates an applications's use of the windows console api to messages with
/// the console handler.
///
/// The agent is injected into all processes that are covered by the console
/// proxy infrastructure, which it typically all of them, so it is constrained
/// by a number of factors:
///
///    * It must be secure. Any security issued introduced by the agent will
///      affect all processes would be a Bad Thing(TM).
///    * The injection process should be fast since it runs on every process
///      creation.
///    * It should be small since it will take up space in all processes.
///
/// The agent consists of a number of components spread among a number of files.
///
///    * The agent itself which is declared in this file is responsible for
///      initiating the injection process.
///    * The {{binpatch.hh}}(binary patching infrastructure) does the actual
///      platform-specific code patching.
///    * The {{conapi.hh}}(console api) is a framework for implementing
///      alternative consoles. The agent can have different functions
///      (delegating, tracing, etc.) depending on which replacement console api
///      is selected.
///
/// In addition to these core components there are components used for testing
/// and debugging,
///
///    * The {{host.hh}}(console host) is a command-line utility that runs a
///      command with the agent injected.

#include "binpatch.hh"
#include "conapi.hh"
#include "utils/types.hh"

// Controls the injection of the console agent.
class ConsoleAgent {
public:
  // Install the console agent instead of the built-in console.
  static bool install(Console &console);

private:
  // Returns the address of the console function with the given name.
  static address_t get_console_function_address(c_str_t name);

  // The console object currently being delegated to.
  static Console *delegate;

  static bool WINAPI write_console_a_bridge(handle_t console_output,
      const void *buffer, dword_t number_of_chars_to_write,
      dword_t *number_of_chars_written, void *reserved) {
    return delegate->write_console_a(console_output, buffer, number_of_chars_to_write,
        number_of_chars_written, reserved);
  }

};
