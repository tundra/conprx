//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent.hh"
#include "binpatch.hh"
#include "conapi.hh"
#include "utils/log.hh"

using namespace conprx;

class OriginalConsole : public Console {
public:
  OriginalConsole(Vector<PatchRequest*> patches) : patches_(patches) { }
#define __EMIT_TRAMPOLINE_DECL__(Name, name, RET, PARAMS, ARGS)                \
  virtual RET name PARAMS;
  FOR_EACH_CONAPI_FUNCTION(__EMIT_TRAMPOLINE_DECL__)
#undef __EMIT_TRAMPOLINE_DECL__
private:
  Vector<PatchRequest*> patches_;
};

#define __EMIT_TRAMPOLINE_IMPL__(Name, name, RET, PARAMS, ARGS)                \
  RET OriginalConsole::name PARAMS {                                           \
    PatchRequest *patch = patches_[Console::name##_key];                       \
    Console::name##_t trampoline = patch->get_trampoline<Console::name##_t>(); \
    return trampoline ARGS;                                                    \
  }
FOR_EACH_CONAPI_FUNCTION(__EMIT_TRAMPOLINE_IMPL__)
#undef __EMIT_TRAMPOLINE_IMPL__

Console *ConsoleAgent::delegate_ = NULL;

bool ConsoleAgent::install(Options &options, Console &delegate, Console **original_out) {
  LOG_DEBUG("Installing agent");
  delegate_ = &delegate;

  // Get and initialize the platform.
  LOG_DEBUG("Initializing platform");
  Platform &platform = Platform::get();
  if (!platform.ensure_initialized())
    return false;

  LOG_DEBUG("Creating patch requests");
  // Identify the set of functions we're going to patch. Some of them may not
  // be available in which case we'll just not try to patch them.
  Vector<Console::FunctionInfo> functions = Console::functions();
  // An array of all the requests that could be constructed, in a contiguous
  // range.
  PatchRequest *packed_requests = new PatchRequest[functions.length()];
  // An array where the key'th entry holds a pointer to patch the function
  // with the given key. Some of these may be NULL, the rest will point into
  // the packed request array.
  PatchRequest **key_to_request = new PatchRequest*[Console::kFunctionCount];
  for (size_t i = 0; i < Console::kFunctionCount; i++)
    key_to_request[i] = NULL;
  // The current offset within the packed requests.
  size_t packed_offset = 0;
  for (size_t i = 0; i < functions.length(); i++) {
    Console::FunctionInfo &info = functions[i];
    address_t original = get_console_function_address(info.name);
    address_t bridge = get_delegate_bridge(info.key);
    if (original == NULL || bridge == NULL) {
      LOG_DEBUG("Console function %s not found", info.name);
    } else {
      PatchRequest *entry = &packed_requests[packed_offset++];
      *entry = PatchRequest(original, bridge);
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
      Console::kFunctionCount));

  LOG_DEBUG("Successfully installed agent");
  return true;
}

// Expands to a bridge for each api function that calls the object stored in
// the agent's delegate field.
#define __EMIT_BRIDGE__(Name, name, RET, PARAMS, ARGS)                         \
  RET WINAPI ConsoleAgent::name##_bridge PARAMS {                              \
    IF_USE_UNPATCHED_MONITOR(UnpatchedMonitor::Disable disable,);              \
    return ConsoleAgent::delegate().name ARGS;                                 \
  }
FOR_EACH_CONAPI_FUNCTION(__EMIT_BRIDGE__)
#undef __EMIT_BRIDGE__

address_t ConsoleAgent::get_delegate_bridge(int key) {
  switch (key) {
#define __EMIT_CASE__(Name, name, ...)                                         \
    case Console::name##_key: {                                                \
      Console::name##_t result = ConsoleAgent::name##_bridge;                  \
      return Code::upcast(result);                                             \
    }
  FOR_EACH_CONAPI_FUNCTION(__EMIT_CASE__)
#undef __EMIT_CASE__
    default:
      return NULL;
  }
}

UnpatchedMonitor::Disable::Disable(UnpatchedMonitor *monitor)
  : monitor_(monitor)
  , prev_is_active_(monitor->is_active_) {
  monitor_->is_active_ = false;
}

UnpatchedMonitor::Disable::Disable()
  : monitor_(installed_)
  , prev_is_active_(monitor_->is_active_) {
  monitor_->is_active_ = false;
}

UnpatchedMonitor::Disable::~Disable() {
  monitor_->is_active_ = prev_is_active_;
}

UnpatchedMonitor *UnpatchedMonitor::installed_ = NULL;

// This list is experimentally determined, it would be great with a well-defined
// whitelist instead but I don't know how you'd create one. The values and the
// functions they correspond to is determined using stack traces.
ushort_t UnpatchedMonitor::kApiIndexBlacklist[kApiIndexBlacklistSize] = {
  35, // TerminateThread
  76  // GetModuleHandleW
};

bool UnpatchedMonitor::is_blacklisted(short_t dll_index, short_t api_index) {
  if (dll_index != 0)
    // The blacklist only contains stuff for dll 0.
    return false;
  for (size_t i = 0; i < kApiIndexBlacklistSize; i++) {
    if (kApiIndexBlacklist[i] == api_index)
      return true;
  }
  return false;
}

UnpatchedMonitor::UnpatchedMonitor()
  : is_active_(true) { }

// Generate the static bridge functions.
#define LPC_BRIDGE_IMPL(Name, name, RET, PARAMS, ARGS)                         \
  RET NTAPI UnpatchedMonitor::name##_bridge PARAMS {                           \
    return installed_->name ARGS;                                              \
  }
  FOR_EACH_LPC_FUNCTION(LPC_BRIDGE_IMPL)
#undef LPC_BRIDGE_IMPL

// Expands to the trampoline function for the given lpc api function.
#define LPC_TRAMPOLINE(name) (patches_[name##_k].get_trampoline(name##_bridge))

ntstatus_t NTAPI UnpatchedMonitor::nt_request_wait_reply_port(
    handle_t port_handle, lpc_message_t *request, lpc_message_t *incoming_reply) {
  if (is_active_) {
    ulong_t api_number = request->api_number;
    ushort_t dll_index = static_cast<ushort_t>((api_number >> 16) & 0xFFFF);
    ushort_t api_index = (api_number & 0xFFFF);
    if (!is_blacklisted(dll_index, api_index)) {
      // Just to be on the safe side don't intercept any nested lpc calls caused
      // by logging.
      Disable disable(this);
      fprintf(stderr, "--- unhandled lpc %i/%i ---\n", dll_index, api_index);
      print_stack_trace(stderr);
      fflush(stderr);
    }
  }
  return LPC_TRAMPOLINE(nt_request_wait_reply_port)(port_handle, request,
      incoming_reply);
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
