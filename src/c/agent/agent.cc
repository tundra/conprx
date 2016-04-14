//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent.hh"
#include "agent/conconn.hh"
#include "async/promise-inl.hh"
#include "binpatch.hh"
#include "confront.hh"
#include "marshal-inl.hh"
#include "sync/thread.hh"
#include "utils/log.hh"

BEGIN_C_INCLUDES
#include "utils/string-inl.h"
END_C_INCLUDES

using namespace conprx;
using namespace plankton;
using namespace tclib;

// Set to true to make the message handler print any messages it doesn't
// understand.
static const bool kDumpUnknownMessages = false;
static const bool kSuspendOnUnknownMessages = false;

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
  LogEntry entry_data(entry);
  NativeVariant entry_var(&entry_data);
  rpc::OutgoingRequest req(Variant::null(), "log", 1, &entry_var);
  rpc::IncomingResponse resp = out_->socket()->send_request(&req);
  while (!resp->is_settled())
    out_->input()->process_next_instruction(NULL);
  return propagate(entry);
}

ConsoleAgent::ConsoleAgent()
  : agent_in_(NULL)
  , agent_out_(NULL)
  , platform_(NULL) { }

const char *ConsoleAgent::get_lpc_name(ulong_t number) {
  switch (number) {
#define __GEN_CASE__(Name, name, NUM, FLAGS) case NUM: return #Name;
  FOR_EACH_LPC_TO_INTERCEPT(__GEN_CASE__)
  FOR_EACH_OTHER_KNOWN_LPC(__GEN_CASE__)
#undef __GEN_CASE__
  default:
    return NULL;
  }
}

NtStatus ConsoleAgent::on_message(lpc::Message *request) {
  switch (request->api_number()) {
    // The messages we want to handle.
#define __EMIT_CASE__(Name, name, NUM, FLAGS)                                  \
    case lm##Name: {                                                           \
      lfTr FLAGS (trace_before(#name, request),);                              \
      NtStatus result = lfDa FLAGS (                                           \
          request->send_to_backend(),                                          \
          adaptor()->name(request, &request->data()->payload.name));           \
      lfTr FLAGS (trace_after(#name, request, result),);                       \
      return result;                                                           \
    }
  FOR_EACH_LPC_TO_INTERCEPT(__EMIT_CASE__)
#undef __EMIT_CASE__
    // The messages we know about but don't want to handle.
#define __EMIT_CASE__(Name, name, NUM, FLAGS) case NUM:
  FOR_EACH_OTHER_KNOWN_LPC(__EMIT_CASE__)
#undef __EMIT_CASE__
      return request->send_to_backend();
    // Unknown messages.
    default: {
      if (kDumpUnknownMessages) {
        lpc::Interceptor::Disable disable(request->interceptor());
        request->dump(FileSystem::native()->std_out());
      }
      if (kSuspendOnUnknownMessages) {
        NativeThread::sleep(Duration::seconds(30));
      }
      return request->send_to_backend();
    }
  }
}

void ConsoleAgent::trace_before(const char *name, lpc::Message *message) {
  OutStream *out = FileSystem::native()->std_err();
  out->printf("--- before %s ---\n", name);
  message->dump(out);
}

void ConsoleAgent::trace_after(const char *name, lpc::Message *message, NtStatus status) {
  OutStream *out = FileSystem::native()->std_err();
  out->printf("--- after %s (= %i) ---\n", name, status.to_nt());
  message->dump(out);
}

fat_bool_t ConsoleAgent::install_agent(tclib::InStream *agent_in,
    tclib::OutStream *agent_out, ConsolePlatform *platform) {
  agent_in_ = agent_in;
  agent_out_ = agent_out;
  platform_ = platform;
  owner_ = new (kDefaultAlloc) rpc::StreamServiceConnector(agent_in, agent_out);
  owner_->set_default_type_registry(ConsoleTypes::registry());
  if (!owner()->init(empty_callback()))
    return F_FALSE;
  log()->set_destination(owner());
  log()->ensure_installed();
  F_TRY(install_agent_platform());
  F_TRY(send_is_ready());
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
  rpc::OutgoingRequest req(Variant::null(), "is_ready");
  Handle stdin_handle(platform()->get_std_handle(kStdInputHandle));
  NativeVariant stdin_var(&stdin_handle);
  req.set_argument("stdin", stdin_var);
  Handle stdout_handle(platform()->get_std_handle(kStdOutputHandle));
  NativeVariant stdout_var(&stdout_handle);
  req.set_argument("stdout", stdout_var);
  Handle stderr_handle(platform()->get_std_handle(kStdErrorHandle));
  NativeVariant stderr_var(&stderr_handle);
  req.set_argument("stderr", stderr_var);
  rpc::IncomingResponse resp;
  return send_request(&req, &resp);
}

fat_bool_t ConsoleAgent::send_is_done() {
  rpc::OutgoingRequest req(Variant::null(), "is_done");
  rpc::IncomingResponse resp;
  return send_request(&req, &resp);
}

fat_bool_t ConsoleAgent::send_request(rpc::OutgoingRequest *request,
    rpc::IncomingResponse *resp_out) {
  rpc::IncomingResponse resp = owner()->socket()->send_request(request);
  while (!resp->is_settled())
    F_TRY(owner()->input()->process_next_instruction(NULL));
  if (resp->is_rejected())
    return F_FALSE;
  *resp_out = resp;
  return F_TRUE;
}

#ifdef IS_MSVC
#include "agent-msvc.cc"
#endif
