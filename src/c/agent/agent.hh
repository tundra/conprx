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

namespace conprx {

// Controls the injection of the console agent.
class ConsoleAgent {
public:
  // Install the given console instead of the built-in one. Returns true on
  // success. If this succeeds the output parameter will hold another console
  // which can be called to get the original console behavior.
  static bool install(Console &console, Console **original_out);

private:
  // Returns the address of the console function with the given name.
  static address_t get_console_function_address(c_str_t name);

  // Returns the address of the delegator that turns static function calls into
  // method calls on the delegate object.
  static address_t get_delegate_bridge(int key);

#define __EMIT_DELEGATE_BRIDGE__(Name, name, RET, PARAMS, ARGS)                \
  static RET WINAPI name##_bridge PARAMS;
  FOR_EACH_CONAPI_FUNCTION(__EMIT_DELEGATE_BRIDGE__)
#undef __EMIT_DELEGATE_BRIDGE__

  // The console object currently being delegated to.
  static Console *delegate_;
  static Console &delegate() { return *delegate_; }
};

// Expands the given macro for each boolean option. The arguments are:
// <field>              <upper camel>           <all caps>
#define FOR_EACH_BOOL_OPTION(F)                                                \
  F(is_enabled,      true,      "Enabled",              "ENABLED")             \
  F(verbose_logging, true,      "VerboseLogging",       "VERBOSE_LOGGING")

// The set of options that control how the agent behaves. Where these are read
// from depends on the platform; on windows from the registry.
class Options {
public:
  Options();

#define __EMIT_BOOL_GETTER__(name, defawlt, Name, NAME) bool name() { return name##_; }
  FOR_EACH_BOOL_OPTION(__EMIT_BOOL_GETTER__)
#undef __EMIT_BOOL_GETTER__

  // Returns the singleton options instance.
  static Options &get();

protected:
#define __EMIT_BOOL_FIELD__(name, defawlt, Name, NAME) bool name##_;
  FOR_EACH_BOOL_OPTION(__EMIT_BOOL_FIELD__)
#undef __EMIT_BOOL_FIELD__

  // Dummy unused field.
  bool dummy_;
};

} // namespace conprx
