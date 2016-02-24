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
}

void AgentOwnerService::on_log(rpc::RequestData& data, ResponseCallback resp) {
  TextWriter writer;
  writer.write(data[0]);
  INFO("AGENT LOG: %s", *writer);
  resp(rpc::OutgoingResponse::success(Variant::null()));
}

void AgentOwnerService::on_is_ready(rpc::RequestData& data, ResponseCallback resp) {
  launcher()->agent_is_ready_ = true;
  resp(rpc::OutgoingResponse::success(Variant::null()));
}

Launcher::Launcher()
  : agent_is_ready_(Drawbridge::dsRaised)
  , state_(lsConstructed)
  , service_(this) {
  process_.set_flags(pfStartSuspendedOnWindows);
}

bool InjectingLauncher::prepare_start() {
  B_TRY(up_.open(NativePipe::pfDefault));
  B_TRY(down_.open(NativePipe::pfDefault));
  return true;
}

bool InjectingLauncher::start_connect_to_agent() {
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
  if (!process()->start_inject_library(injection())) {
    LOG_ERROR("Failed to start injecting %s.", agent_dll_.chars);
    return false;
  }
  return true;
}

opaque_t InjectingLauncher::ensure_agent_ready_background() {
  if (!attach_agent_service())
    return b2o(false);
  if (!ensure_agent_service_ready())
    return b2o(false);
  return b2o(true);
}

bool InjectingLauncher::complete_connect_to_agent() {
  // Spin off a thread to communicate with the agent while we wait for the
  // injection to complete. Ideally we'd be able to wait for both on the same
  // thread but this works and we can look into a smarter way to do this if
  // there turns out to be a problem.
  tclib::NativeThread connector(new_callback(
      &InjectingLauncher::ensure_agent_ready_background, this));
  B_TRY(connector.start());
  bool injected = process()->complete_inject_library(injection());
  if (!injected)
    B_TRY(abort_agent_service());
  opaque_t ready = o0();
  B_TRY(connector.join(&ready));
  B_TRY(o2b(ready));
  B_TRY(injected);
  B_TRY(ensure_process_resumed());
  return true;
}

bool Launcher::initialize() {
  CHECK_EQ("launch interaction out of order", lsConstructed, state_);
  // There's actually nothing to do here at the moment but that's likely to
  // change so leave it in.
  state_ = lsInitialized;
  return true;
}

bool Launcher::start(utf8_t command, size_t argc, utf8_t *argv) {
  CHECK_EQ("launch interaction out of order", lsInitialized, state_);
  B_TRY(prepare_start());

  // Start the process -- if it's possible to start processes suspended it will
  // be suspended.
  if (!process_.start(command, argc, argv)) {
    LOG_ERROR("Failed to start %s.", command.chars);
    return false;
  }

  if (use_agent()) {
    // If we're using the agent connect it; this will also take care of resuming
    // the process.
    B_TRY(connect_agent());
  } else {
    // There is no agent so we resume the process manually.
    B_TRY(ensure_process_resumed());
  }

  state_ = lsStarted;
  return connect_service();
}

bool Launcher::connect_agent() {
  // Start connecting but don't block.
  B_TRY(start_connect_to_agent());

  // Wait for the agent to finish connecting.
  B_TRY(complete_connect_to_agent());

  return ensure_agent_service_ready();
}

bool Launcher::attach_agent_service() {
  tclib::InStream *oin = owner_in();
  CHECK_FALSE("no owner in", oin == NULL);
  tclib::OutStream *oout = owner_out();
  CHECK_FALSE("no owner out", oout == NULL);
  agent_ = new (kDefaultAlloc) StreamServiceConnector(oin, oout);
  return agent()->init(service()->handler());
}

bool Launcher::abort_agent_service() {
  B_TRY(owner_in()->close());
  B_TRY(owner_out()->close());
  return true;
}

bool Launcher::ensure_agent_service_ready() {
  while (!agent_is_ready_)
    B_TRY(agent()->input()->process_next_instruction(NULL));
  return true;
}

bool Launcher::ensure_process_resumed() {
  // If we can suspend/resume the process will have been started suspended. This
  // is orthogonal to whether we inject or not.
  return !NativeProcess::kCanSuspendResume || process_.resume();
}

bool Launcher::connect_service() {
  return true;
}

bool Launcher::process_messages() {
  CHECK_EQ("launch interaction out of order", lsStarted, state_);
  return agent()->process_all_messages();
}

bool Launcher::join(int *exit_code_out) {
  if (use_agent()) {
    B_TRY(owner_in()->close());
    B_TRY(owner_out()->close());
  }
  ProcessWaitIop wait(&process_, o0());
  if (!wait.execute()) {
    LOG_ERROR("Failed to wait for process.");
    return false;
  }
  *exit_code_out = process_.exit_code().peek_value(1);
  return true;
}
