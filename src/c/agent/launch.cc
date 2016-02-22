//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent/agent.hh"
#include "agent/launch.hh"
#include "async/promise-inl.hh"

BEGIN_C_INCLUDES
#include "utils/log.h"
#include "utils/string-inl.h"
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
  launcher()->agent_is_ready_.lower();
  resp(rpc::OutgoingResponse::success(Variant::null()));
}

Launcher::Launcher()
  : agent_is_ready_(Drawbridge::dsRaised)
  , state_(lsConstructed)
  , service_(this) {
  process_.set_flags(pfStartSuspendedOnWindows);
}

Launcher::~Launcher() {
}

bool InjectingLauncher::prepare_start() {
  return up_.open(NativePipe::pfDefault) && down_.open(NativePipe::pfDefault);
}

bool InjectingLauncher::start_connect_to_agent() {
  connect_data_t data;
  data.magic = ConsoleAgent::kConnectDataMagic;
  data.parent_process_id = IF_MSVC(GetCurrentProcessId(), 0);
  data.owner_in_handle = owner_in()->to_raw_handle();
  data.owner_out_handle = owner_out()->to_raw_handle();
  blob_t blob_in = blob_new(&data, sizeof(data));
  injection()->set_connector(new_c_string("ConprxAgentConnect"), blob_in,
      blob_empty());
  if (!process()->start_inject_library(injection())) {
    LOG_ERROR("Failed to start injecting %s.", agent_dll_.chars);
    return false;
  }
  return true;
}

bool InjectingLauncher::complete_connect_to_agent() {
  return process()->complete_inject_library(injection());
}

bool Launcher::initialize() {
  CHECK_EQ("launch interaction out of order", lsConstructed, state_);
  if (!agent_is_ready_.initialize())
    return false;
  state_ = lsInitialized;
  return true;
}

bool Launcher::start(utf8_t command, size_t argc, utf8_t *argv) {
  CHECK_EQ("launch interaction out of order", lsInitialized, state_);
  if (!prepare_start())
    return false;

  // Start the process -- if it's possible to start processes suspended it will
  // be suspended.
  if (!process_.start(command, argc, argv)) {
    LOG_ERROR("Failed to start %s.", command.chars);
    return false;
  }

  if (use_agent()) {
    // Start connecting but don't block.
    if (!start_connect_to_agent())
      return false;

    // Start the agent service and try to connect it to the running agent.
    tclib::InStream *oin = owner_in();
    CHECK_FALSE("no owner in", oin == NULL);
    tclib::OutStream *oout = owner_out();
    CHECK_FALSE("no owner out", oout == NULL);
    agent_ = new (kDefaultAlloc) StreamServiceConnector(oin, oout);
    if (!agent()->init(service()->handler()))
      return false;

    // Wait for the agent to finish connecting.
    if (!complete_connect_to_agent())
      return false;
  } else {
    ensure_process_resumed();
  }

  state_ = lsStarted;
  return true;
}

bool Launcher::ensure_process_resumed() {
  // If we can suspend/resume the process will have been started suspended. This
  // is orthogonal to whether we inject or not.
  return !NativeProcess::kCanSuspendResume || process_.resume();
}

bool Launcher::connect() {
  CHECK_EQ("launch interaction out of order", lsStarted, state_);
  if (use_agent() && !agent_is_ready_.pass())
    return false;
  state_ = lsConnected;
  return true;
}

bool Launcher::process_messages() {
  CHECK_REL("launch interaction out of order", lsStarted, <=, state_);
  return agent()->process_all_messages();
}

bool Launcher::join(int *exit_code_out) {
  if (use_agent()) {
    if (!owner_in()->close() || !owner_out()->close())
      return false;
  }
  ProcessWaitIop wait(&process_, o0());
  if (!wait.execute()) {
    LOG_ERROR("Failed to wait for process.");
    return false;
  }
  *exit_code_out = process_.exit_code().peek_value(1);
  return true;
}
