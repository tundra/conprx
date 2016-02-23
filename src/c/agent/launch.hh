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
  virtual ~Launcher() { }

  // Initialize this launcher's resources but don't actually launch anything.
  // Won't block.
  virtual bool initialize();

  // Start the process running and wait for it to be brought up, but don't
  // connect the agent owner service.
  bool start(utf8_t command, size_t argc, utf8_t *argv);

  // Connect the agent owner service and wait for the agent to report back that
  // it's ready. Subtypes can override this to perform additional work before
  // and/or after.
  virtual bool connect_service();

  // Run the agent owner service.
  bool process_messages();

  // Wait for the process to exit, storing the exit code in the out param.
  virtual bool join(int *exit_code_out);

protected:
  // Override this to do any work that needs to be performed before the process
  // can be launched.
  virtual bool prepare_start() { return true; }

  // Override to perform any configuration of the process immediately after it
  // has been launched before the agent owner service has been started.
  virtual bool start_connect_to_agent() { return true; }

  // Override to perform any configuration of the process after the agent owner
  // service has been started.
  virtual bool complete_connect_to_agent() { return true; }

  // Return true if this launcher intends to launch the agent. If it does it
  // must implement start_connect_to_agent and complete_connect_to_agent such
  // that one of them resumes the process using ensure_process_resumed.
  virtual bool use_agent() = 0;

  // Must be called by the concrete implementation at the time appropriate to
  // them to resume the child process if it has been started suspended.
  bool ensure_process_resumed();

  // Loop around waiting for the agent to report to the owner service that
  // it's ready. Does nothing if the agent is already ready.
  bool ensure_agent_ready();

  // There are four streams in play here: the owner pair and the agent pair.
  // The owner in stream allows the owner to read what is written to the agent
  // out stream. The agent in stream allows the agent to read what is written to
  // the owner out stream. Don't get these mixed up -- the owner may handle the
  // agent's streams but shouldn't use them itself.
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

  // Does the work of connecting the agent.
  bool connect_agent();

  friend class AgentOwnerService;
  tclib::NativeProcess process_;

  bool agent_is_ready_;
  State state_;

  tclib::def_ref_t<StreamServiceConnector> agent_;
  StreamServiceConnector *agent() { return *agent_; }
  AgentOwnerService service_;
  AgentOwnerService *service() { return &service_; }
};

// A launcher that works by injecting the agent as a dll into the process.
class InjectingLauncher : public Launcher {
public:
  InjectingLauncher(utf8_t agent_dll)
    : agent_dll_(agent_dll)
    , injection_(agent_dll) { }
  virtual void default_destroy() { tclib::default_delete_concrete(this); }

protected:
  // Before we can start we need to open up the pipes over which we'll be
  // communicating. These can be created without blocking so we do that early
  // on.
  virtual bool prepare_start();

  // Inject the dll and start it running. At some point after this has been
  // called the dll installation code will wait to connect back to the owner
  // and this call doesn't block to wait for that.
  virtual bool start_connect_to_agent();

  // Here we'll perform the owner's side of the connection procedure, knowing
  // that barring some unforeseen problem the agent will eventually be on the
  // other side of the connection ready to connect. We will also release the
  // process so the main program can start running.
  virtual bool complete_connect_to_agent();

  virtual bool use_agent() { return true; }
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
