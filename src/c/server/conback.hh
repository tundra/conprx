//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _CONPRX_SERVER_CONBACK
#define _CONPRX_SERVER_CONBACK

#include "rpc.hh"
#include "share/protocol.hh"
#include "sync/pipe.hh"
#include "sync/process.hh"
#include "sync/thread.hh"
#include "utils/blob.hh"
#include "utils/fatbool.hh"

namespace conprx {

using plankton::Variant;
using plankton::Factory;
using plankton::Arena;

class Launcher;

// Virtual type, implementations of which can be used as the implementation of
// a console.
class ConsoleBackend {
public:
  virtual ~ConsoleBackend() { }

  // Debug/test call.
  virtual response_t<int64_t> poke(int64_t value) = 0;

  virtual response_t<uint32_t> get_console_cp(bool is_output) = 0;

  virtual response_t<bool_t> set_console_cp(uint32_t value, bool is_output) = 0;

  virtual response_t<uint32_t> get_console_title(tclib::Blob buffer, bool is_unicode) = 0;

  virtual response_t<bool_t> set_console_title(tclib::Blob title, bool is_unicode) = 0;
};

// The service the driver will call back to when it wants to access the manager.
class ConsoleBackendService : public plankton::rpc::Service {
public:
  ConsoleBackendService();
  virtual ~ConsoleBackendService() { }

  bool agent_is_ready() { return agent_is_ready_; }

  bool agent_is_done() { return agent_is_done_; }

  void set_backend(ConsoleBackend *backend) { backend_ = backend; }

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

  ConsoleBackend *backend_;
  ConsoleBackend *backend() { return backend_; }

  bool agent_is_ready_;
  bool agent_is_done_;
};

} // namespace conprx

#endif // _CONPRX_SERVER_CONBACK
