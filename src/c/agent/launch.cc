//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent/agent.hh"
#include "agent/launch.hh"
#include "async/promise-inl.hh"

BEGIN_C_INCLUDES
#include "utils/log.h"
#include "utils/string-inl.h"
#include "utils/trybool.h"
END_C_INCLUDES

using namespace tclib;
using namespace conprx;
using namespace plankton;

AgentOwnerService::AgentOwnerService(Launcher *launcher)
  : launcher_(launcher) {
  register_method("log", new_callback(&AgentOwnerService::on_log, this));
  register_method("is_ready", new_callback(&AgentOwnerService::on_is_ready, this));
  register_method("is_done", new_callback(&AgentOwnerService::on_is_done, this));
  register_method("poke", new_callback(&AgentOwnerService::on_poke, this));
  register_method("get_cp", new_callback(&AgentOwnerService::on_get_cp, this));
  set_fallback(new_callback(&AgentOwnerService::message_not_understood, this));
}

void AgentOwnerService::on_log(rpc::RequestData *data, ResponseCallback resp) {
  TextWriter writer;
  writer.write(data->argument(0));
  INFO("AGENT LOG: %s", *writer);
  resp(rpc::OutgoingResponse::success(Variant::null()));
}

void AgentOwnerService::on_is_ready(rpc::RequestData *data, ResponseCallback resp) {
  launcher()->agent_is_ready_ = true;
  resp(rpc::OutgoingResponse::success(Variant::null()));
}

void AgentOwnerService::on_is_done(rpc::RequestData *data, ResponseCallback resp) {
  launcher()->agent_is_done_ = true;
  resp(rpc::OutgoingResponse::success(Variant::null()));
}

template <typename T>
static void forward_response(Response<T> resp, rpc::Service::ResponseCallback callback) {
  if (resp.has_error()) {
    callback(rpc::OutgoingResponse::failure(Variant::integer(resp.error())));
  } else {
    callback(rpc::OutgoingResponse::success(resp.value()));
  }
}

void AgentOwnerService::on_poke(rpc::RequestData *data, ResponseCallback resp) {
  int64_t value = data->argument(0).integer_value();
  forward_response(launcher()->backend()->on_poke(value), resp);
}

void AgentOwnerService::on_get_cp(rpc::RequestData *data, ResponseCallback resp) {
  forward_response(launcher()->backend()->get_cp(), resp);
}

void AgentOwnerService::message_not_understood(rpc::RequestData *data,
    ResponseCallback resp) {
  TextWriter writer;
  writer.write(data->selector());
  WARN("Unknown agent message %s", *writer);
  resp(rpc::OutgoingResponse::failure(Variant::null()));
}

Launcher::Launcher()
  : agent_is_ready_(false)
  , agent_is_done_(false)
  , state_(lsConstructed)
  , service_(this)
  , agent_monitor_done_(Drawbridge::dsLowered)
  , backend_(NULL) {
  process_.set_flags(pfStartSuspendedOnWindows);
}

fat_bool_t InjectingLauncher::prepare_start() {
  F_TRY(up_.open(NativePipe::pfDefault));
  F_TRY(down_.open(NativePipe::pfDefault));
  return F_TRUE;
}

fat_bool_t InjectingLauncher::start_connect_to_agent() {
  // Set up the connection data to pass into the dll.
  connect_data_t data;
  data.magic = ConsoleAgent::kConnectDataMagic;
  data.parent_process_id = IF_MSVC(GetCurrentProcessId(), 0);
  data.agent_in_handle = down_.in()->to_raw_handle();
  data.agent_out_handle = up_.out()->to_raw_handle();
  blob_t blob_in = blob_new(&data, sizeof(data));
  injection()->set_connector(new_c_string("ConprxAgentConnect"), blob_in,
      blob_empty());

  // Start the injection running.
  F_TRY(process()->start_inject_library(injection()));
  return F_TRUE;
}

opaque_t InjectingLauncher::ensure_agent_ready_background() {
  fat_bool_t attached = attach_agent_service();
  if (!attached)
    return f2o(attached);
  fat_bool_t ready = ensure_agent_service_ready();
  if (!ready)
    return f2o(ready);
  return f2o(F_TRUE);
}

fat_bool_t InjectingLauncher::complete_connect_to_agent() {
  // Spin off a thread to communicate with the agent while we wait for the
  // injection to complete. Ideally we'd be able to wait for both on the same
  // thread but this works and we can look into a smarter way to do this if
  // there turns out to be a problem.
  tclib::NativeThread connector(new_callback(
      &InjectingLauncher::ensure_agent_ready_background, this));
  F_TRY(connector.start());
  fat_bool_t injected = process()->complete_inject_library(injection());
  if (!injected)
    F_TRY(abort_agent_service());
  opaque_t ready = o0();
  F_TRY(connector.join(&ready));
  F_TRY(o2f(ready));
  F_TRY(injected);
  F_TRY(ensure_process_resumed());
  return F_TRUE;
}

fat_bool_t Launcher::initialize() {
  CHECK_EQ("launch interaction out of order", lsConstructed, state_);
  if (!agent_monitor_done()->initialize())
    return F_FALSE;
  state_ = lsInitialized;
  return F_TRUE;
}

fat_bool_t Launcher::start(utf8_t command, size_t argc, utf8_t *argv) {
  CHECK_EQ("launch interaction out of order", lsInitialized, state_);
  F_TRY(prepare_start());

  // Start the process -- if it's possible to start processes suspended it will
  // be suspended.
  F_TRY(process_.start(command, argc, argv));

  if (use_agent()) {
    // If we're using the agent connect it; this will also take care of resuming
    // the process.
    F_TRY(connect_agent());
  } else {
    // There is no agent so we resume the process manually.
    F_TRY(ensure_process_resumed());
  }

  state_ = lsStarted;
  return connect_service();
}

fat_bool_t Launcher::connect_agent() {
  // Start connecting but don't block.
  F_TRY(start_connect_to_agent());

  // Wait for the agent to finish connecting.
  F_TRY(complete_connect_to_agent());

  return ensure_agent_service_ready();
}

fat_bool_t Launcher::attach_agent_service() {
  tclib::InStream *oin = owner_in();
  CHECK_FALSE("no owner in", oin == NULL);
  tclib::OutStream *oout = owner_out();
  CHECK_FALSE("no owner out", oout == NULL);
  agent_ = new (kDefaultAlloc) StreamServiceConnector(oin, oout);
  return agent()->init(service()->handler());
}

fat_bool_t Launcher::abort_agent_service() {
  if (!owner_in()->close())
    return F_FALSE;
  if (!owner_out()->close())
    return F_FALSE;
  return F_TRUE;
}

fat_bool_t Launcher::ensure_agent_service_ready() {
  while (!agent_is_ready_)
    F_TRY(agent()->input()->process_next_instruction(NULL));
  return F_TRUE;
}

fat_bool_t Launcher::ensure_process_resumed() {
  // If we can suspend/resume the process will have been started suspended. This
  // is orthogonal to whether we inject or not.
  if (!NativeProcess::kCanSuspendResume)
    return F_TRUE;
  return process_.resume();
}

fat_bool_t Launcher::connect_service() {
  return F_TRUE;
}

opaque_t Launcher::monitor_agent() {
  if (agent_monitor_done()->pass(Duration::instant()))
    return f2o(F_FALSE);
  fat_bool_t processed = process_messages();
  // Even if processing fails we need to lower the drawbridge because there may
  // be other threads waiting for it and if we bail at this point they'll be
  // stuck.
  if (!agent_monitor_done()->lower())
    return f2o(F_FALSE);
  return f2o(processed);
}

fat_bool_t Launcher::process_messages() {
  CHECK_EQ("launch interaction out of order", lsStarted, state_);
  while (!agent_is_done_) {
    InputSocket::ProcessInstrStatus status;
    F_TRY(agent()->input()->process_next_instruction(&status));
    F_TRY(F_BOOL(!status.is_error()));
  }
  return F_TRUE;
}

fat_bool_t Launcher::close_agent() {
  if (!agent_monitor_done()->pass())
    // We have to wait for the agent monitor to be done, otherwise we might
    // close the streams while it's reading them.
    return F_FALSE;
  if (use_agent()) {
    if (!owner_in()->close())
      return F_FALSE;
    if (!owner_out()->close())
      return F_FALSE;
  }
  return F_TRUE;
}

fat_bool_t Launcher::join(int *exit_code_out) {
  ProcessWaitIop wait(&process_, o0());
  if (!wait.execute()) {
    LOG_ERROR("Failed to wait for process.");
    return F_FALSE;
  }
  *exit_code_out = process_.exit_code().peek_value(1);
  return F_TRUE;
}
