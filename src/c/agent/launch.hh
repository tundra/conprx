//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _CONPRX_AGENT_LAUNCH
#define _CONPRX_AGENT_LAUNCH

#include "rpc.hh"
#include "sync/pipe.hh"
#include "sync/process.hh"
#include "sync/thread.hh"

namespace conprx {

using plankton::Variant;
using plankton::Factory;
using plankton::Arena;
using plankton::rpc::OutgoingRequest;
using plankton::rpc::IncomingResponse;
using plankton::rpc::StreamServiceConnector;

class Launcher;

// The service the driver will call back to when it wants to access the manager.
class AgentOwnerService : public plankton::rpc::Service {
public:
  AgentOwnerService(Launcher *launcher);
  virtual ~AgentOwnerService() { }

private:
  // Handles logs entries logged by the agent.
  void on_log(plankton::rpc::RequestData&, ResponseCallback);

  // Called when the agent has completed its setup.
  void on_is_ready(plankton::rpc::RequestData&, ResponseCallback);

  Launcher *launcher_;
  Launcher *launcher() { return launcher_; }
};

// Encapsulates launching a child process and injecting the agent dll.
class Launcher : public tclib::DefaultDestructable {
public:
  Launcher();
  virtual ~Launcher();

  // Initialize this launcher's resources but don't actually launch anything.
  // Won't block.
  bool initialize();

  // Start the process running.
  bool start(utf8_t command, size_t argc, utf8_t *argv);

  // Wait for the agent to report back.
  bool connect();

  // Run the agent owner service.
  bool process_messages();

  // Wait for the process to exit, storing the exit code in the out param.
  virtual bool join(int *exit_code_out);

protected:
  // Override this to do any work that needs to be performed before the process
  // can be launched.
  virtual bool prepare_start() { return true; }

  // Override to perform any configuration of the process immediately after it
  // has been launched, before it's resumed.
  virtual bool start_connect_to_agent() { return true; }

  virtual bool use_agent() { return true; }

  virtual bool complete_connect_to_agent() { return true; }

  // Must be called by the concrete implementation at the time appropriate to
  // them to resume the child process if it has been started suspended.
  bool ensure_process_resumed();

  // Accessors used to get the in and out streams to use to talk to the agent.
  virtual tclib::InStream *owner_in() { return NULL; }
  virtual tclib::OutStream *owner_out() { return NULL; }

  tclib::NativeProcess *process() { return &process_; }

private:
  enum State {
    lsConstructed = 0,
    lsInitialized = 1,
    lsStarted = 2,
    lsConnected = 3
  };

  friend class AgentOwnerService;
  tclib::NativeProcess process_;

  tclib::Drawbridge agent_is_ready_;
  State state_;

  tclib::def_ref_t<StreamServiceConnector> agent_;
  StreamServiceConnector *agent() { return *agent_; }
  AgentOwnerService service_;
  AgentOwnerService *service() { return &service_; }
};

class InjectingLauncher : public Launcher {
public:
  InjectingLauncher(utf8_t agent_dll)
    : agent_dll_(agent_dll)
    , injection_(agent_dll) { }
  virtual void default_destroy() { tclib::default_delete_concrete(this); }

protected:
  virtual bool prepare_start();
  virtual bool start_connect_to_agent();
  virtual bool complete_connect_to_agent();
  virtual tclib::InStream *owner_in() { return up_.in(); }
  virtual tclib::OutStream *owner_out() { return down_.out(); }

private:
  utf8_t agent_dll_;
  tclib::NativePipe up_;
  tclib::NativePipe down_;
  tclib::NativeProcess::InjectRequest injection_;
  tclib::NativeProcess::InjectRequest *injection() { return &injection_; }
};

} // namespace conprx

#endif // _CONPRX_AGENT_LAUNCH
