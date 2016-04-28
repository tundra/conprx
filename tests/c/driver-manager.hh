//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// The client side of the driver connection, that is, the process that talks to
// the driver -- the driver itself is the server.

#ifndef _CONPRX_DRIVER_MANAGER_HH
#define _CONPRX_DRIVER_MANAGER_HH

#include "agent/confront.hh"
#include "async/promise-inl.hh"
#include "driver.hh"
#include "io/iop.hh"
#include "marshal-inl.hh"
#include "plankton-inl.hh"
#include "rpc.hh"
#include "server/launch.hh"
#include "sync/pipe.hh"
#include "sync/process.hh"
#include "sync/thread.hh"

namespace conprx {

// Using-declarations within headers is suspect for subtle reasons (google it
// for details) so just use the stuff we need.
using plankton::Variant;
using plankton::Factory;
using plankton::Arena;
using plankton::rpc::OutgoingRequest;
using plankton::rpc::IncomingResponse;
using plankton::rpc::StreamServiceConnector;

class DriverManager;

// An individual request to the driver.
class DriverRequest {
public:
  // All the different methods you can call through this request.
#define __DECL_PROXY_METHOD__(name, SIG, ARGS)                                 \
  Variant name SIG;
  FOR_EACH_REMOTE_MESSAGE(__DECL_PROXY_METHOD__)
#undef __DECL_PROXY_METHOD__

#define __DECL_PROXY_METHOD__(Name, name, FLAGS, SIG, PSIG)                    \
  Variant name PSIG(GET_SIG_PARAMS);
  FOR_EACH_CONAPI_FUNCTION(__DECL_PROXY_METHOD__)
#undef __DECL_PROXY_METHOD__

  // Returns a factory that will live as long as the request, so you can use it
  // to allocate data for sending with your request.
  Factory *factory() { return &arena_; }

  // Returns the resulting value of this request, assuming it succeeded. If it
  // is not yet resolved or failed this will fail.
  const Variant &operator*();

  // Returns the error resulting from this request, assuming it failed and
  // failing if it didn't.
  const Variant &error();

  // Returns a pointer to the value of this request, assuming it succeeded. If
  // not, same as *.
  const Variant *operator->() { return &this->operator*(); }

  bool is_rejected() { return response_->is_rejected(); }

private:
  friend class DriverManager;
  DriverRequest(DriverManager *connection)
    : is_used_(false)
    , manager_(connection) { }

  // Returns a blob variant corresponding to the given blob.
  Variant from_blob(tclib::Blob blob);

  Variant send(Variant selector);
  Variant send(Variant selector, Variant arg0);
  Variant send(Variant selector, Variant arg0, Variant arg1);
  Variant send(Variant selector, Variant arg0, Variant arg1, Variant arg2);
  Variant send_request(OutgoingRequest *req);

  bool is_used_;
  DriverManager *manager_;
  IncomingResponse response_;
  Variant result_;
  Arena arena_;
};

// A launcher that knows how to start the driver and communicate with the fake
// launcher that the driver supports. It can't be used for anything other than
// testing.
class FakeAgentLauncher : public Launcher {
public:
  FakeAgentLauncher();
  virtual void default_destroy() { tclib::default_delete_concrete(this); }

  // Allocate the underlying channel.
  fat_bool_t allocate();

  tclib::ServerChannel *agent_channel() { return *agent_channel_; }

protected:
  virtual tclib::InStream *owner_in() { return agent_channel()->in(); }
  virtual tclib::OutStream *owner_out() { return agent_channel()->out(); }

  // The fake agent launcher needs the process to be running before it can start
  // talking to the remote agent so this does almost all of the work, it
  // releases the process from being suspended and opens up the connection.
  virtual fat_bool_t start_connect_to_agent();

  virtual bool use_agent() { return true; }

private:

  tclib::def_ref_t<tclib::ServerChannel> agent_channel_;
};

// Launcher that knows how to launch the driver with no agent. Useful for
// testing the plain driver.
class NoAgentLauncher : public Launcher {
public:
  virtual void default_destroy() { tclib::default_delete_concrete(this); }
  virtual bool use_agent() { return false; }

protected:
  // This is only present such that we can report an error if anyone tries to
  // call it.
  virtual fat_bool_t start_connect_to_agent();

};

// A manager that manages the lifetime of the driver, including starting it up,
// and allows communication with it.
class DriverManager {
public:
  // Which kind of agent to use.
  enum AgentType {
    atNone,
    atFake,
    atReal
  };

  DriverManager();

  // Start up the driver executable.
  fat_bool_t start();

  // Connect to the running driver.
  fat_bool_t connect();

  // Wait for the driver to terminate.
  fat_bool_t join(int *exit_code_out);

  // When devutils are enabled, allows you to trace all the requests that go
  // through this manager.
  ONLY_ALLOW_DEVUTILS(void set_trace(bool value) { trace_ = value; })

  // Sets the type of agent to use for this manager. Note that not all agents
  // are supported on all platforms.
  void set_agent_type(AgentType type);

  // Sets the frontend type the driver should use. This setter is sanity checked
  // against the agent type so you need to set the agent type first.
  void set_frontend_type(DriverFrontendType type);

  // Overrides the default agent path (set via an env variable) with the given
  // value.
  void set_agent_path(utf8_t path) { agent_path_ = path; }

  // Instruct the driver to not print log messages. Useful for when the driver
  // is made to do stuff that we know will fail, for testing.
  void set_silence_log(bool value) { silence_log_ = value; }

  // Returns a new request object that can be used to perform a single call to
  // the driver. The value returned by the call is only valid as long as the
  // request is.
  DriverRequest new_request() { return DriverRequest(this); }

  Launcher *operator->() { return launcher(); }

  // Sets the backend to eventually pass to the launcher once it's been created.
  void set_backend(ConsoleBackend *backend) { backend_ = backend; }

  // Constant that's true when the real agent can work.
  static const bool kSupportsRealAgent = kIsMsvc;

  // Shorthands for requests that don't need the request object before sending
  // the message.
#define __DECL_PROXY_METHOD__(name, SIG, ARGS)                                 \
  DriverRequest name SIG {                                                     \
    DriverRequest req(this);                                                   \
    req.result_ = req.name ARGS;                                               \
    return req;                                                                \
  }
  FOR_EACH_REMOTE_MESSAGE(__DECL_PROXY_METHOD__)
#undef __DECL_PROXY_METHOD__

#define __DECL_PROXY_METHOD__(Name, name, FLAGS, SIG, PSIG)                    \
  DriverRequest name PSIG(GET_SIG_PARAMS) {                                    \
    DriverRequest req(this);                                                   \
    req.result_ = req.name PSIG(GET_SIG_ARGS);                                 \
    return req;                                                                \
  }
  FOR_EACH_CONAPI_FUNCTION(__DECL_PROXY_METHOD__)
#undef __DECL_PROXY_METHOD__

  IncomingResponse send(OutgoingRequest *req);

private:
  tclib::def_ref_t<Launcher> launcher_;
  Launcher *launcher() { return *launcher_; }

  plankton::TypeRegistry registry_;

  // The channel through which we control the driver.
  tclib::def_ref_t<tclib::ServerChannel> channel_;
  tclib::ServerChannel *channel() { return *channel_;}
  tclib::def_ref_t<StreamServiceConnector> connector_;
  StreamServiceConnector *connector() { return *connector_; }

  tclib::NativeThread agent_monitor_;
  bool has_started_agent_monitor_;

  bool silence_log_;
  bool trace_;
  plankton::rpc::TracingMessageSocketObserver tracer_;
  utf8_t agent_path_;
  utf8_t agent_path();
  AgentType agent_type_;
  AgentType agent_type() { return agent_type_; }
  bool use_agent() { return agent_type_ != atNone; }
  DriverFrontendType frontend_type_;
  DriverFrontendType frontend_type() { return frontend_type_; }
  ConsoleBackend *backend_;

  static utf8_t executable_path();
  static utf8_t default_agent_path();
};

} // namespace conprx

#endif // _CONPRX_DRIVER_MANAGER_HH
