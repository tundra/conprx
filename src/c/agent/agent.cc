//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent.hh"
#include "binpatch.hh"
#include "conapi.hh"
#include "utils/log.hh"

using namespace conprx;

class OriginalConsole : public Console {
public:
  OriginalConsole(Vector<PatchRequest> patches) : patches_(patches) { }
#define __EMIT_TRAMPOLINE_DECL__(Name, name, RET, PARAMS, ARGS)                \
  virtual RET name PARAMS;
  FOR_EACH_CONAPI_FUNCTION(__EMIT_TRAMPOLINE_DECL__)
#undef __EMIT_TRAMPOLINE_DECL__
private:
  Vector<PatchRequest> patches_;
};

#define __EMIT_TRAMPOLINE_IMPL__(Name, name, RET, PARAMS, ARGS)                \
  RET OriginalConsole::name PARAMS {                                           \
    PatchRequest &patch = patches_[Console::name##_key];                       \
    Console::name##_t trampoline = patch.get_trampoline<Console::name##_t>();  \
    return trampoline ARGS;                                                    \
  }
FOR_EACH_CONAPI_FUNCTION(__EMIT_TRAMPOLINE_IMPL__)
#undef __EMIT_TRAMPOLINE_IMPL__

Console *ConsoleAgent::delegate_ = NULL;

bool ConsoleAgent::install(Console &delegate, Console **original_out) {
  delegate_ = &delegate;

  // Get and initialize the platform.
  Platform &platform = Platform::get();
  if (!platform.ensure_initialized())
    return false;

  // Identify the set of functions we're going to patch. Some of them may not
  // be available in which case we'll just not try to patch them.
  Vector<Console::FunctionInfo> functions = Console::functions();
  // An array of all the requests that could be constructed, in a contiguous
  // range.
  PatchRequest *packed_requests = new PatchRequest[functions.length()];
  // An array where the key'th entry holds the request to patch the function
  // with the given key. Some of these may be empty.
  PatchRequest *key_to_request = new PatchRequest[Console::kFunctionCount];
  // The current offset within the packed requests.
  size_t packed_offset = 0;
  for (size_t i = 0; i < functions.length(); i++) {
    Console::FunctionInfo &info = functions[i];
    address_t original = get_console_function_address(info.name);
    address_t bridge = get_delegate_bridge(info.key);
    if (original == NULL || bridge == NULL)
      continue;
    PatchRequest request(original, bridge);
    packed_requests[packed_offset++] = request;
    key_to_request[info.key] = request;
  }
  Vector<PatchRequest> requests = Vector<PatchRequest>(packed_requests, packed_offset);

  // Create a patch set and apply it.
  PatchSet patches(platform, requests);
  if (!patches.apply())
    return false;

  *original_out = new OriginalConsole(Vector<PatchRequest>(key_to_request, Console::kFunctionCount));
  return true;
}

// Expands to a bridge for each api function that calls the object stored in
// the agent's delegate field.
#define __EMIT_BRIDGE__(Name, name, RET, PARAMS, ARGS)                         \
  RET ConsoleAgent::name##_bridge PARAMS {                                     \
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

#ifdef IS_MSVC
#include "agent-msvc.cc"
#else
#include "agent-posix.cc"
#endif
