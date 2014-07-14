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
///
/// ## Options
///
/// The agent understands a number of environment options, set either through
/// the registry or as environment variables. On startup options are read in
/// sequence from
///
///    1. `HKEY_LOCAL_MACHINE\Software\Tundra\Console Agent\...`
///    2. `HKEY_CURRENT_USER\Software\Tundra\Console Agent\...`
///    3. Environment variables.
///
/// Later values override earlier ones, so the local machine settings can
/// override the defaults, the user settings can override machine settings, and
/// environment settings can override user settings.
///
/// The naming convention for options are upper camel case for registry entries,
/// all caps for environment variables. The options are,
///
///    * `Enabled`/`CONSOLE_AGENT_ENABLED`: whether to patch code or just bail
///      out immediately. The default is true.
///    * `VerboseLogging`/`CONSOLE_AGENT_VERBOSE_LOGGING`: log what the agent
///      does, both successfully and on failures. The default is to log only
///      on failures.
///
/// Setting a registry option to integer `0` disables the option, `1` enables
/// it. Setting an environment variable to the string `"0"` disables an option,
/// `"1"` enables it.
///
/// ## Blacklist
///
/// The agent has a blacklist of processes it refuses to patch. In some cases
/// they're core services which we shouldn't tamper with, in other cases they're
/// fallbacks that we want to be sure work even if the agent misbehaves. For
/// instance, the registry editor is blacklisted because if the agent misbehaves
/// we may need to disable it by setting registry values.

#include "binpatch.hh"
#include "conapi.hh"
#include "utils/types.hh"

// Enable this to install the unpatched monitor which logs when functions are
// called that haven't been patched.
#define USE_UNPATCHED_MONITOR 1

namespace conprx {

class Options;

// Controls the injection of the console agent.
class ConsoleAgent {
public:
  // Install the given console instead of the built-in one. Returns true on
  // success. If this succeeds the output parameter will hold another console
  // which can be called to get the original console behavior.
  static bool install(Options &options, Console &console, Console **original_out);

private:
  // Returns the address of the console function with the given name.
  static address_t get_console_function_address(cstr_t name);

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

// Enumerate each lpc function.
#define FOR_EACH_LPC_FUNCTION(F)                                               \
  F(NtRequestWaitReplyPort, nt_request_wait_reply_port, ntstatus_t,            \
      (handle_t port_handle, lpc_message_t *request,                           \
       lpc_message_t *incoming_reply),                                         \
      (port_handle, request, incoming_reply))

// Convenience that allows the define above to be used within an expression.
#if USE_UNPATCHED_MONITOR
#define IF_USE_UNPATCHED_MONITOR(T, F) T
#else
#define IF_USE_UNPATCHED_MONITOR(T, F) F
#endif

// A monitor that attempts to log calls to console api functions that haven't
// been patched by the agent. This is a relatively epic hack so don't, under any
// circumstances, keep it enabled in a release.
//
// The built-in console works by sending messages from the client to a console
// server process, which it does using the built-in lpc system. The monitor
// works by intercepting these calls and printing the stack. If a call to the
// console api has been patched we explicitly disable the monitor before and
// after so we still intercept the call but suppress the output. Consequently
// only unpatched calls should make their way through.
class UnpatchedMonitor {
public:
  UnpatchedMonitor();

  // Stack-allocate one of these to disable the given unpatched monitor
  // temporarily, reverting to the previous state on scope exit.
  class Disable {
  public:
    // Disable the given unpatched monitor.
    Disable(UnpatchedMonitor *monitor);

    // Disable the default unpatched monitor.
    Disable();

    // Revert enablement to the previous state.
    ~Disable();
  private:
    UnpatchedMonitor *monitor_;
    bool prev_is_active_;
  };

  // Dumps the current stack trace to stderr.
  void print_stack_trace(FILE *out);

  // Keys that identify which functions are stored in which entry in the
  // patches_ array.
  enum lpc_function_key_t {
#define DECLARE_ENUM(Name, name, RET, PARAMS, ARGS)                            \
    name##_k ,
    FOR_EACH_LPC_FUNCTION(DECLARE_ENUM)
#undef DECLARE_ENUM
    kLpcFunctionCount
  };

  // Static bridges.
#define DECLARE_BRIDGE(Name, name, RET, PARAMS, ARGS)                          \
  static RET NTAPI name##_bridge PARAMS;
  FOR_EACH_LPC_FUNCTION(DECLARE_BRIDGE)
#undef DECLARE_BRIDGE

  // Statful implementations.
#define DECLARE_HANDLER(Name, name, RET, PARAMS, ARGS)                         \
  RET NTAPI name PARAMS;
  FOR_EACH_LPC_FUNCTION(DECLARE_HANDLER)
#undef DECLARE_HANDLER

  // Install the unpatched monitor, returning true iff successful.
  bool install();

private:
  // Initializes the patch with the given key such that it is ready to be
  // applied. Returns true iff successful.
  template <typename T>
  bool init_patch(lpc_function_key_t key, module_t ntdll, const char *name, T *repl);

  // Currently installed unpatched monitor.
  static UnpatchedMonitor *installed_;

  // Array of api indices we're definitely not interested in.
  static const size_t kApiIndexBlacklistSize = 2;
  static ushort_t kApiIndexBlacklist[kApiIndexBlacklistSize];

  // Is the api call with the given index blacklisted? The api index corresponds
  // to a function on the server side.
  static bool is_blacklisted(short_t dll_index, short_t api_index);

  // Patch requests used to inject the monitor.
  PatchRequest patches_[kLpcFunctionCount];

  // Are we currently intercepting calls or just passing them on?
  bool is_active_;
};

// Expands the given macro for each boolean option. The arguments are:
// <field>              <upper camel>           <all caps>
#define FOR_EACH_BOOL_OPTION(F)                                                \
  F(is_enabled,      true,      "Enabled",              "ENABLED")             \
  F(verbose_logging, false,     "VerboseLogging",       "VERBOSE_LOGGING")

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
