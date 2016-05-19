//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "async/promise-inl.hh"
#include "server/launch.hh"

BEGIN_C_INCLUDES
#include "utils/log.h"
#include "utils/string-inl.h"
END_C_INCLUDES

using namespace tclib;
using namespace conprx;
using namespace plankton;

ProcessAttachment::ProcessAttachment(NativeProcessHandle *process, Launcher *launcher)
  : process_(process)
  , service_(launcher)
  , agent_monitor_done_(Drawbridge::dsLowered)
  , backend_(NULL) { }

Launcher::Launcher()
  : state_(lsConstructed)
  , backend_(NULL) {
  process_.set_flags(pfStartSuspendedOnWindows | pfNewHiddenConsoleOnWindows);
}

fat_bool_t Launcher::prepare_start() {
  return attachment()->prepare_start();
}

fat_bool_t InjectingProcessAttachment::prepare_start() {
  F_TRY(up_.open(NativePipe::pfDefault));
  F_TRY(down_.open(NativePipe::pfDefault));
  return F_TRUE;
}

tclib::pass_def_ref_t<ProcessAttachment> InjectingLauncher::create_attachment(
    tclib::NativeProcessHandle *process) {
  return new (kDefaultAlloc) InjectingProcessAttachment(process, this, agent_dll_);
}

fat_bool_t InjectingProcessAttachment::start_connect_to_agent() {
  // Set up the connection data to pass into the dll.
  connect_data_t data;
  data.magic = connect_data_t::kMagic;
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

opaque_t InjectingProcessAttachment::ensure_agent_ready_background() {
  fat_bool_t attached = attach_agent_service();
  if (!attached)
    return f2o(attached);
  fat_bool_t ready = ensure_agent_service_ready();
  if (!ready)
    return f2o(ready);
  return f2o(F_TRUE);
}

fat_bool_t InjectingProcessAttachment::complete_connect_to_agent() {
  // Spin off a thread to communicate with the agent while we wait for the
  // injection to complete. Ideally we'd be able to wait for both on the same
  // thread but this works and we can look into a smarter way to do this if
  // there turns out to be a problem.
  tclib::NativeThread connector(new_callback(
      &InjectingProcessAttachment::ensure_agent_ready_background, this));
  F_TRY(connector.start());
  fat_bool_t injected = process()->complete_inject_library(injection());
  if (!injected)
    F_TRY(abort_agent_service());
  opaque_t ready = o0();
  F_TRY(connector.join(&ready));
  fat_bool_t succeeded = o2f(ready) & injected;
  if (!succeeded) {
    // If injecting fails we kill the process (so it doesn't hang) and abort.
    F_TRY(process()->kill());
    F_TRY(o2f(ready));
    F_TRY(injected);
  }
  return succeeded;
}

fat_bool_t ProcessAttachment::initialize() {
  F_TRY(agent_monitor_done()->initialize());
  return F_TRUE;
}

fat_bool_t Launcher::initialize() {
  CHECK_EQ("launch interaction out of order", lsConstructed, state_);
  attachment_ = create_attachment(process());
  if (backend_ != NULL)
    attachment_->set_backend(backend_);
  F_TRY(attachment()->initialize());
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
  F_TRY(attachment()->start_connect_to_agent());

  // Wait for the agent to finish connecting.
  F_TRY(attachment()->complete_connect_to_agent());

  F_TRY(ensure_process_resumed());

  return attachment()->ensure_agent_service_ready();
}

fat_bool_t ProcessAttachment::attach_agent_service() {
  tclib::InStream *oin = owner_in();
  CHECK_FALSE("no owner in", oin == NULL);
  tclib::OutStream *oout = owner_out();
  CHECK_FALSE("no owner out", oout == NULL);
  agent_ = new (kDefaultAlloc) StreamServiceConnector(oin, oout);
  agent()->set_default_type_registry(service()->registry());
  return agent()->init(service()->handler());
}

fat_bool_t ProcessAttachment::abort_agent_service() {
  if (!owner_in()->close())
    return F_FALSE;
  if (!owner_out()->close())
    return F_FALSE;
  return F_TRUE;
}

fat_bool_t ProcessAttachment::ensure_agent_service_ready() {
  while (!service()->agent_is_ready())
    F_TRY(agent()->input()->process_next_instruction(NULL));
  return F_TRUE;
}

fat_bool_t Launcher::inject_agent(tclib::NativeProcessHandle *process) {
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

void ProcessAttachment::set_backend(ConsoleBackend *backend) {
  backend_ = backend;
  service()->set_backend(backend);
}

void Launcher::set_backend(ConsoleBackend *backend) {
  CHECK_TRUE("too late to set backend", attachment_.is_null());
  backend_ = backend;
}

ProcessAttachment *Launcher::attachment() {
  CHECK_FALSE("no attachment", attachment_.is_null());
  return *attachment_;
}

opaque_t ProcessAttachment::monitor_agent() {
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

fat_bool_t ProcessAttachment::process_messages() {
  while (!service()->agent_is_done()) {
    InputSocket::ProcessInstrStatus status;
    F_TRY(agent()->input()->process_next_instruction(&status));
    F_TRY(F_BOOL(!status.is_error()));
  }
  return F_TRUE;
}

fat_bool_t ProcessAttachment::close_agent(bool use_agent) {
  if (!agent_monitor_done()->pass())
    // We have to wait for the agent monitor to be done, otherwise we might
    // close the streams while it's reading them.
    return F_FALSE;
  if (use_agent) {
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
