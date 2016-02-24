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
#include "confront.hh"
#include "io/stream.hh"
#include "rpc.hh"
#include "utils/log.hh"
#include "utils/types.hh"

namespace conprx {

using plankton::rpc::StreamServiceConnector;

class Options;

class LogEntry {
public:
  LogEntry();
  LogEntry(log_entry_t *entry) : entry_(*entry) { }
  void init(utf8_t file, uint32_t line, utf8_t message);

  // The seed type for handles.
  static plankton::SeedType<LogEntry> *seed_type() { return &kSeedType; }

private:
  log_entry_t entry_;

  template <typename T> friend class plankton::DefaultSeedType;
  static LogEntry *new_instance(plankton::Variant header, plankton::Factory *factory);
  plankton::Variant to_seed(plankton::Factory *factory);
  void init(plankton::Seed payload, plankton::Factory *factory);
  static plankton::DefaultSeedType<LogEntry> kSeedType;
};

// Log that streams messages onto an output stream before passing log handling
// on to the enclosing log.
class StreamingLog : public tclib::Log {
public:
  StreamingLog() : out_(NULL) { }
  virtual bool record(log_entry_t *entry);
  void set_destination(StreamServiceConnector *out) { out_ = out; }

private:
  StreamServiceConnector *out_;
};

// The block of data passed through to the agent's dll connector.
typedef struct {
  int32_t magic;
  standalone_dword_t parent_process_id;
  tclib::naked_file_handle_t agent_in_handle;
  tclib::naked_file_handle_t agent_out_handle;
} connect_data_t;

// Controls the injection of the console agent.
class ConsoleAgent : public tclib::DefaultDestructable {
public:
  virtual ~ConsoleAgent() { }

  // Install the given console instead of the built-in one. Returns true on
  // success. If this succeeds the output parameter will hold another console
  // which can be called to get the original console behavior.
  static bool install(Options &options, ConsoleFrontend &console, ConsoleFrontend **original_out);

  static const int32_t kConnectDataMagic = 0xFABACAEA;

  static const int cInvalidConnectDataSize = 0x11110000;
  static const int cInvalidConnectDataMagic = 0x11120000;
  static const int cInstallationFailed = 0x11130000;
  static const int cFailedToOpenParentProcess = 0x11140000;
  static const int cFailedToDuplicateOwnerIn = 0x11150000;
  static const int cFailedToDuplicateOwnerOut = 0x11160000;
  static const int cSuccess = 0x0;

  // Install this agent.
  bool install_agent(tclib::InStream *owner_in, tclib::OutStream *owner_out);

  // Send a request back to the owner and wait for a response.
  bool send_request(plankton::rpc::OutgoingRequest *request,
      plankton::rpc::IncomingResponse *response_out);

protected:
  // Perform the platform-specific part of the agent installation.
  virtual bool install_agent_platform() = 0;

private:
  // Send the is-ready message to the owner.
  bool send_is_ready();

  // A connection to the owner of the agent.
  tclib::def_ref_t<StreamServiceConnector> owner_;
  StreamServiceConnector *owner() { return *owner_; }

  StreamingLog *log() { return &log_; }
  StreamingLog log_;

  // Returns the address of the console function with the given name.
  static address_t get_console_function_address(cstr_t name);

  // Returns the address of the delegator that turns static function calls into
  // method calls on the delegate object.
  static address_t get_delegate_bridge(int key);

#define __EMIT_DELEGATE_BRIDGE__(Name, name, FLAGS, SIG, PSIG)                 \
  static SIG(GET_SIG_RET) WINAPI name##_bridge SIG(GET_SIG_PARAMS);
  FOR_EACH_CONAPI_FUNCTION(__EMIT_DELEGATE_BRIDGE__)
#undef __EMIT_DELEGATE_BRIDGE__

  // The console object currently being delegated to.
  static ConsoleFrontend &delegate() { return *static_cast<ConsoleFrontend*>(NULL); }
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
