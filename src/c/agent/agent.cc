//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent.hh"
#include "binpatch.hh"
#include "confront.hh"
#include "utils/log.hh"

using namespace conprx;
using namespace plankton;
using namespace tclib;

class OriginalConsole : public ConsoleFrontend {
public:
  OriginalConsole(Vector<PatchRequest*> patches) : patches_(patches) { }
  virtual void default_destroy() { tclib::default_delete_concrete(this); }
#define __EMIT_TRAMPOLINE_DECL__(Name, name, FLAGS, SIG, PSIG)                 \
  virtual SIG(GET_SIG_RET) name SIG(GET_SIG_PARAMS);
  FOR_EACH_CONAPI_FUNCTION(__EMIT_TRAMPOLINE_DECL__)
#undef __EMIT_TRAMPOLINE_DECL__
private:
  Vector<PatchRequest*> patches_;
};

#define __EMIT_TRAMPOLINE_IMPL__(Name, name, FLAGS, SIG, PSIG)                 \
  SIG(GET_SIG_RET) OriginalConsole::name SIG(GET_SIG_PARAMS) {                 \
    PatchRequest *patch = patches_[ConsoleFrontend::name##_key];               \
    ConsoleFrontend::name##_t imposter = patch->get_imposter<ConsoleFrontend::name##_t>(); \
    return imposter SIG(GET_SIG_ARGS);                                         \
  }
FOR_EACH_CONAPI_FUNCTION(__EMIT_TRAMPOLINE_IMPL__)
#undef __EMIT_TRAMPOLINE_IMPL__

bool StreamingLog::record(log_entry_t *entry) {
  plankton::Arena arena;
  plankton::Seed seed = arena.new_seed();
  seed.set_header("conprx.LogEntry");
  seed.set_field("file", entry->file);
  seed.set_field("line", entry->line);
  seed.set_field("message", entry->message.chars);
  out_->send_value(seed);
  return propagate(entry);
}

bool ConsoleAgent::install_agent_shared(tclib::InStream *agent_in,
    tclib::OutStream *agent_out) {
  owner_ = new (kDefaultAlloc) rpc::StreamServiceConnector(agent_in, agent_out);
  if (!owner()->init(empty_callback()))
    return false;

  log()->set_destination(owner()->output());
  log()->ensure_installed();
  if (!install_agent())
    return false;

  return true;
}

bool ConsoleAgent::install(Options &options, ConsoleFrontend &delegate, ConsoleFrontend **original_out) {
  LOG_DEBUG("Installing agent");
  // Get and initialize the platform.
  LOG_DEBUG("Initializing platform");
  Platform &platform = Platform::get();
  if (!platform.ensure_initialized())
    return false;

  LOG_DEBUG("Creating patch requests");
  // Identify the set of functions we're going to patch. Some of them may not
  // be available in which case we'll just not try to patch them.
  Vector<ConsoleFrontend::FunctionInfo> functions = ConsoleFrontend::functions();
  // An array of all the requests that could be constructed, in a contiguous
  // range.
  PatchRequest *packed_requests = new PatchRequest[functions.length()];
  // An array where the key'th entry holds a pointer to patch the function
  // with the given key. Some of these may be NULL, the rest will point into
  // the packed request array.
  PatchRequest **key_to_request = new PatchRequest*[ConsoleFrontend::kFunctionCount];
  for (size_t i = 0; i < ConsoleFrontend::kFunctionCount; i++)
    key_to_request[i] = NULL;
  // The current offset within the packed requests.
  size_t packed_offset = 0;
  for (size_t i = 0; i < functions.length(); i++) {
    ConsoleFrontend::FunctionInfo &info = functions[i];
    address_t original = get_console_function_address(info.name);
    address_t bridge = get_delegate_bridge(info.key);
    if (original == NULL || bridge == NULL) {
      LOG_DEBUG("Console function %s not found", info.name);
    } else {
      PatchRequest *entry = &packed_requests[packed_offset++];
      *entry = PatchRequest(original, bridge, info.name);
      key_to_request[info.key] = entry;
    }
  }
  Vector<PatchRequest> requests = Vector<PatchRequest>(packed_requests, packed_offset);
  LOG_DEBUG("Created %i patch requests", static_cast<int>(requests.length()));

  // Create a patch set and apply it.
  PatchSet patches(platform, requests);
  if (!patches.apply())
    return false;

  *original_out = new OriginalConsole(Vector<PatchRequest*>(key_to_request,
      ConsoleFrontend::kFunctionCount));

  LOG_DEBUG("Successfully installed agent");
  return true;
}

// Expands to a bridge for each api function that calls the object stored in
// the agent's delegate field.
#define __EMIT_BRIDGE__(Name, name, FLAGS, SIG, PSIG)                          \
  SIG(GET_SIG_RET) WINAPI ConsoleAgent::name##_bridge SIG(GET_SIG_PARAMS) {    \
    return ConsoleAgent::delegate().name SIG(GET_SIG_ARGS);                    \
  }
FOR_EACH_CONAPI_FUNCTION(__EMIT_BRIDGE__)
#undef __EMIT_BRIDGE__

address_t ConsoleAgent::get_delegate_bridge(int key) {
  switch (key) {
#define __EMIT_CASE__(Name, name, ...)                                         \
    case ConsoleFrontend::name##_key: {                                        \
      ConsoleFrontend::name##_t result = ConsoleAgent::name##_bridge;          \
      return Code::upcast(result);                                             \
    }
  FOR_EACH_CONAPI_FUNCTION(__EMIT_CASE__)
#undef __EMIT_CASE__
    default:
      return NULL;
  }
}

// The default options construction.
Options::Options() :
#define __EMIT_INIT__(name, defawlt, Name, NAME) name##_(defawlt) ,
    FOR_EACH_BOOL_OPTION(__EMIT_INIT__)
#undef __EMIT_INIT__
    dummy_(false) { }

#ifdef IS_MSVC
#include "agent-msvc.cc"
#else
#include "agent-posix.cc"
#endif
