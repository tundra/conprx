//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _CONPRX_AGENT_LAUNCH
#define _CONPRX_AGENT_LAUNCH

#include "agent/protocol.hh"
#include "rpc.hh"
#include "sync/pipe.hh"
#include "sync/process.hh"
#include "sync/thread.hh"
#include "utils/fatbool.hh"

namespace conprx {

using plankton::Variant;
using plankton::Factory;
using plankton::Arena;
using plankton::rpc::OutgoingRequest;
using plankton::rpc::IncomingResponse;
using plankton::rpc::StreamServiceConnector;

class Launcher;

// Virtual type, implementations of which can be used as the implementation of
// a console.
class ConsoleBackend {
public:
  virtual ~ConsoleBackend() { }

  // Debug/test call.
  virtual Response<int64_t> on_poke(int64_t value) = 0;

  virtual Response<uint32_t> get_cp() = 0;

  virtual Response<bool_t> set_cp(uint32_t value) = 0;
};

// The service the driver will call back to when it wants to access the manager.
class AgentOwnerService : public plankton::rpc::Service {
public:
  AgentOwnerService(Launcher *launcher);
  virtual ~AgentOwnerService() { }

private:
  // Handles logs entries logged by the agent.
  void on_log(plankton::rpc::RequestData*, ResponseCallback);

  // Called when the agent has completed its setup.
  void on_is_ready(plankton::rpc::RequestData*, ResponseCallback);
  void on_is_done(plankton::rpc::RequestData*, ResponseCallback);

  // For testing and debugging -- a call that doesn't do anything but is just
  // passed through to the implementation.
  void on_poke(plankton::rpc::RequestData*, ResponseCallback);

#define __GEN_HANDLER__(Name, name, DLL, API)                                  \
  void on_##name(plankton::rpc::RequestData*, ResponseCallback);
  FOR_EACH_LPC_TO_INTERCEPT(__GEN_HANDLER__)
#undef __GEN_HANDLER__

  // The fallback to call on unknown messages.
  void message_not_understood(plankton::rpc::RequestData*, ResponseCallback);

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
  virtual fat_bool_t initialize();

  // Start the process running and wait for it to be brought up, but don't
  // connect the agent owner service.
  fat_bool_t start(utf8_t command, size_t argc, utf8_t *argv);

  // Connect the agent owner service and wait for the agent to report back that
  // it's ready. Subtypes can override this to perform additional work before
  // and/or after.
  virtual fat_bool_t connect_service();

  // Run the agent owner service.
  fat_bool_t process_messages();

  // Wait for the process to exit, storing the exit code in the out param.
  virtual fat_bool_t join(int *exit_code_out);

  fat_bool_t close_agent();

  // Must be called by the concrete implementation at the time appropriate to
  // them to resume the child process if it has been started suspended.
  fat_bool_t ensure_process_resumed();

  // Sets the backend the owner agent will delegate calls to when it receives
  // them from the agent.
  void set_backend(ConsoleBackend *backend) { backend_ = backend; }

  // Returns the custom backend backing this launcher.
  ConsoleBackend *backend() { return backend_; }

  // Runs the loop that monitors the agent service and handles requests. While
  // the launch is running there needs to be a thread doing this, which they
  // do by calling this.
  opaque_t monitor_agent();

  // Returns a drawbridge that gets lowered when the the agent monitor is done.
  // This is useful when running the agent in a separate thread: you close the
  // connection which causes the agent to wind down and then wait for this
  // to be lowered.
  tclib::Drawbridge *agent_monitor_done() { return &agent_monitor_done_; }

protected:
  // Override this to do any work that needs to be performed before the process
  // can be launched.
  virtual fat_bool_t prepare_start() { return F_TRUE; }

  // Override to perform any configuration of the process immediately after it
  // has been launched before the agent owner service has been started.
  virtual fat_bool_t start_connect_to_agent() { return F_TRUE; }

  // Override to perform any configuration of the process after the agent owner
  // service has been started.
  virtual fat_bool_t complete_connect_to_agent() { return F_TRUE; }

  // Return true if this launcher intends to launch the agent. If it does it
  // must implement start_connect_to_agent and complete_connect_to_agent such
  // that one of them resumes the process using ensure_process_resumed.
  virtual bool use_agent() = 0;

  // Create the agent service and open the connection to the agent.
  fat_bool_t attach_agent_service();

  fat_bool_t abort_agent_service();

  // Loop around waiting for the agent to report to the owner service that
  // it's ready. Does nothing if the agent is already ready.
  fat_bool_t ensure_agent_service_ready();

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
  fat_bool_t connect_agent();

  friend class AgentOwnerService;
  tclib::NativeProcess process_;

  bool agent_is_ready_;
  bool agent_is_done_;
  State state_;

  tclib::def_ref_t<StreamServiceConnector> agent_;
  StreamServiceConnector *agent() { return *agent_; }
  AgentOwnerService service_;
  AgentOwnerService *service() { return &service_; }
  tclib::Drawbridge agent_monitor_done_;

  ConsoleBackend *backend_;
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
  virtual fat_bool_t prepare_start();

  // Inject the dll and start it running. At some point after this has been
  // called the dll installation code will wait to connect back to the owner
  // and this call doesn't block to wait for that.
  virtual fat_bool_t start_connect_to_agent();

  // Here we'll perform the owner's side of the connection procedure, knowing
  // that barring some unforeseen problem the agent will eventually be on the
  // other side of the connection ready to connect. We will also release the
  // process so the main program can start running.
  virtual fat_bool_t complete_connect_to_agent();

  opaque_t ensure_agent_ready_background();

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
