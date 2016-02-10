//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent.hh"
#include "binpatch.hh"
#include "conapi.hh"

BEGIN_C_INCLUDES
#include "utils/log.h"
END_C_INCLUDES

using namespace conprx;

class OriginalConsole : public Console {
public:
  OriginalConsole(Vector<PatchRequest*> patches) : patches_(patches) { }
  virtual void default_destroy() { tclib::default_delete_concrete(this); }
#define __EMIT_TRAMPOLINE_DECL__(Name, name, FLAGS, SIG)                       \
  virtual SIG(GET_SIG_RET) name SIG(GET_SIG_PARAMS);
  FOR_EACH_CONAPI_FUNCTION(__EMIT_TRAMPOLINE_DECL__)
#undef __EMIT_TRAMPOLINE_DECL__
private:
  Vector<PatchRequest*> patches_;
};

#define __EMIT_TRAMPOLINE_IMPL__(Name, name, FLAGS, SIG)                       \
  SIG(GET_SIG_RET) OriginalConsole::name SIG(GET_SIG_PARAMS) {                 \
    PatchRequest *patch = patches_[Console::name##_key];                       \
    Console::name##_t imposter = patch->get_imposter<Console::name##_t>();     \
    return imposter SIG(GET_SIG_ARGS);                                         \
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
      Console::kFunctionCount));

  LOG_DEBUG("Successfully installed agent");
  return true;
}

// Expands to a bridge for each api function that calls the object stored in
// the agent's delegate field.
#define __EMIT_BRIDGE__(Name, name, FLAGS, SIG)                                \
  SIG(GET_SIG_RET) WINAPI ConsoleAgent::name##_bridge SIG(GET_SIG_PARAMS) {    \
    return ConsoleAgent::delegate().name SIG(GET_SIG_ARGS);                    \
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
