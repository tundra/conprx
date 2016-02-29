//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent.hh"
#include "agent/conconn.hh"
#include "agent/response.hh"
#include "async/promise-inl.hh"
#include "binpatch.hh"
#include "confront.hh"
#include "marshal-inl.hh"
#include "utils/log.hh"

BEGIN_C_INCLUDES
#include "utils/string-inl.h"
END_C_INCLUDES

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
  virtual dword_t get_last_error() { return 0; }
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

LogEntry::LogEntry() {
  log_entry_default_init(&entry_, llInfo, NULL, 0, string_empty(), string_empty());
}

DefaultSeedType<LogEntry> LogEntry::kSeedType("conprx.LogEntry");

LogEntry *LogEntry::new_instance(Variant header, Factory *factory) {
  return new (factory) LogEntry();
}

Variant LogEntry::to_seed(Factory *factory) {
  Seed seed = factory->new_seed(seed_type());
  seed.set_field("level", entry_.level);
  seed.set_field("file", entry_.file);
  seed.set_field("line", entry_.line);
  seed.set_field("message", entry_.message.chars);
  seed.set_field("timestamp", entry_.timestamp.chars);
  return seed;
}

void LogEntry::init(plankton::Seed payload, plankton::Factory *factory) {
  log_level_t level = static_cast<log_level_t>(payload.get_field("level").integer_value());
  const char *file = payload.get_field("file").string_chars();
  uint32_t line = static_cast<uint32_t>(payload.get_field("line").integer_value());
  const char *message = payload.get_field("message").string_chars();
  const char *timestamp = payload.get_field("timestamp").string_chars();
  log_entry_default_init(&entry_, level, file, line, new_c_string(message),
      new_c_string(timestamp));
}

bool StreamingLog::record(log_entry_t *entry) {
  rpc::OutgoingRequest req(Variant::null(), "log");
  LogEntry entry_data(entry);
  Native entry_var = req.factory()->new_native(&entry_data);
  req.set_arguments(1, &entry_var);
  rpc::IncomingResponse resp = out_->socket()->send_request(&req);
  while (!resp->is_settled())
    out_->input()->process_next_instruction(NULL);
  return propagate(entry);
}

ConsoleAgent::ConsoleAgent()
  : agent_in_(NULL)
  , agent_out_(NULL) {
  // Clear the redirect mask.
  memset(lpc_redirect_mask_, 0, sizeof(*lpc_redirect_mask_) * kLpcRedirectMaskBlocks);
#define __SET_LPC_MASK__(Name, DLL, API) add_to_lpc_redirect_mask((DLL), (API));
  FOR_EACH_LPC_TO_INTERCEPT(__SET_LPC_MASK__)
#undef __SET_LPC_MASK__
}

void ConsoleAgent::add_to_lpc_redirect_mask(ushort_t dll, ushort_t api) {
  uint32_t index = (dll << 16) | api;
  uint32_t block_index = index >> 3;
  uint8_t bit_index = index & 7;
  CHECK_REL("index too big", block_index, <, sizeof(*lpc_redirect_mask_) * kLpcRedirectMaskBlocks);
  uint8_t *block = lpc_redirect_mask_ + block_index;
  *block = *block | static_cast<uint8_t>(1 << bit_index);
}

bool ConsoleAgent::should_redirect_lpc(ulong_t number) {
  uint32_t block_index = number >> 3;
  uint8_t bit_index = number & 7;
  CHECK_REL("index too big", block_index, <, sizeof(*lpc_redirect_mask_) * kLpcRedirectMaskBlocks);
  return (lpc_redirect_mask_[block_index] & (1 << bit_index)) != 0;
}

const char *ConsoleAgent::get_lpc_name(ulong_t number) {
  switch (number) {
#define __GEN_CASE__(Name, DLL, API) case ((DLL << 16) | API): return #Name;
  FOR_EACH_LPC_TO_INTERCEPT(__GEN_CASE__)
  FOR_EACH_OTHER_KNOWN_LPC(__GEN_CASE__)
#undef __GEN_CASE__
  default:
    return NULL;
  }
}

fat_bool_t ConsoleAgent::on_message(lpc::Interceptor *interceptor,
    lpc::Message *request, lpc::message_data_t *incoming_reply) {
  switch (request->api_number()) {
    case 0x3C:
      return on_get_cp(request, &request->payload()->get_cp);
    default:
      return F_FALSE;
  }
}

fat_bool_t ConsoleAgent::on_get_cp(lpc::Message *req, lpc::get_cp_m *get_cp) {
  Response<int64_t> resp = connector()->get_console_cp();
  req->data()->return_value = static_cast<ulong_t>(resp.error());
  get_cp->code_page_id = (resp.has_error() ? 0 : resp.value());
  return F_TRUE;
}

fat_bool_t ConsoleAgent::install_agent(tclib::InStream *agent_in,
    tclib::OutStream *agent_out) {
  agent_in_ = agent_in;
  agent_out_ = agent_out;
  owner_ = new (kDefaultAlloc) rpc::StreamServiceConnector(agent_in, agent_out);
  if (!owner()->init(empty_callback()))
    return F_FALSE;
  log()->set_destination(owner());
  log()->ensure_installed();
  F_TRY(install_agent_platform());
  send_is_ready();
  return F_TRUE;
}

fat_bool_t ConsoleAgent::uninstall_agent() {
  send_is_done();
  F_TRY(uninstall_agent_platform());
  log()->ensure_uninstalled();
  if (agent_in_ != NULL)
    F_TRY(F_BOOL(agent_in_->close()));
  if (agent_out_ != NULL)
    F_TRY(F_BOOL(agent_out_->close()));
  return F_TRUE;
}

fat_bool_t ConsoleAgent::send_is_ready() {
  rpc::OutgoingRequest req(Variant::null(), "is_ready", 0, NULL);
  rpc::IncomingResponse resp;
  return send_request(&req, &resp);
}

fat_bool_t ConsoleAgent::send_is_done() {
  rpc::OutgoingRequest req(Variant::null(), "is_done", 0, NULL);
  rpc::IncomingResponse resp;
  return send_request(&req, &resp);
}

fat_bool_t ConsoleAgent::send_request(rpc::OutgoingRequest *request,
    rpc::IncomingResponse *resp_out) {
  rpc::IncomingResponse resp = owner()->socket()->send_request(request);
  while (!resp->is_settled())
    F_TRY(owner()->input()->process_next_instruction(NULL));
  *resp_out = resp;
  return F_TRUE;
}

fat_bool_t ConsoleAgent::install(Options &options, ConsoleFrontend &delegate,
    ConsoleFrontend **original_out) {
  LOG_DEBUG("Installing agent");
  // Get and initialize the platform.
  LOG_DEBUG("Initializing platform");
  Platform &platform = Platform::get();
  if (!platform.ensure_initialized())
    return F_FALSE;

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
    return F_FALSE;

  *original_out = new OriginalConsole(Vector<PatchRequest*>(key_to_request,
      ConsoleFrontend::kFunctionCount));

  LOG_DEBUG("Successfully installed agent");
  return F_TRUE;
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
