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

  Variant send(Variant selector, Variant arg0);
  Variant send(Variant selector, Variant arg0, Variant arg1, Variant arg2);
  Variant send_request(OutgoingRequest *req);

  bool is_used_;
  DriverManager *manager_;
  IncomingResponse response_;
  Arena arena_;
};

class DriverManagerService : public plankton::rpc::Service, public tclib::DefaultDestructable {
public:
  DriverManagerService(DriverManager *manager);
  virtual ~DriverManagerService() { }
  virtual void default_destroy() { tclib::default_delete_concrete(this); }

private:
  // Handles logs entries logged by the agent.
  void on_log(plankton::rpc::RequestData&, ResponseCallback);

  // Called when the agent has completed its setup.
  void on_is_ready(plankton::rpc::RequestData&, ResponseCallback);

  DriverManager *manager_;
  DriverManager *manager() { return manager_; }
};

// A manager that manages the lifetime of the driver, including starting it up,
// and allows communication with it.
class DriverManager {
public:
  DriverManager();

  // Start up the driver executable.
  bool start();

  // Connect to the running driver.
  bool connect();

  // Wait for the driver to terminate.
  bool join();

  // When devutils are enabled, allows you to trace all the requests that go
  // through this manager.
  ONLY_ALLOW_DEVUTILS(void set_trace(bool value) { trace_ = value; })

  // Notify this manager that it should install the console agent. Once enabled
  // this can't be disabled.
  bool enable_agent();

  // Returns a new request object that can be used to perform a single call to
  // the driver. The value returned by the call is only valid as long as the
  // request is.
  DriverRequest new_request() { return DriverRequest(this); }

  // Shorthands for requests that don't need the request object before sending
  // the message.
#define __DECL_PROXY_METHOD__(name, SIG, ARGS)                                 \
  DriverRequest name SIG {                                                     \
    DriverRequest req(this);                                                   \
    req.name ARGS;                                                             \
    return req;                                                                \
  }
  FOR_EACH_REMOTE_MESSAGE(__DECL_PROXY_METHOD__)
#undef __DECL_PROXY_METHOD__

#define __DECL_PROXY_METHOD__(Name, name, FLAGS, SIG, PSIG)                    \
  DriverRequest name PSIG(GET_SIG_PARAMS) {                                    \
    DriverRequest req(this);                                                   \
    req.name PSIG(GET_SIG_ARGS);                                               \
    return req;                                                                \
  }
  FOR_EACH_CONAPI_FUNCTION(__DECL_PROXY_METHOD__)
#undef __DECL_PROXY_METHOD__

  IncomingResponse send(OutgoingRequest *req);

  void mark_agent_ready() { agent_is_ready_ = true; }

  bool agent_is_ready() { return agent_is_ready_; }

private:
  // Cross-platform (but only used on non-msvc) implementation of enabling the
  // agent that also works when dll injection doesn't exist. For testing only!
  bool enable_agent_fake();

  // Msvc-only implementation of enabling the agent that uses dll injection.
  bool enable_agent_dll_inject();

  // Main entry-point for the agent monitor thread.
  void *run_agent_monitor();

  tclib::NativeProcess process_;
  bool agent_is_ready_;

  // The channel through which we control the driver.
  tclib::def_ref_t<tclib::ServerChannel> channel_;
  tclib::ServerChannel *channel() { return *channel_;}
  tclib::def_ref_t<StreamServiceConnector> connector_;
  StreamServiceConnector *connector() { return *connector_; }

  // If there is a fake agent, this is our connection to it.
  tclib::def_ref_t<tclib::ServerChannel> fake_agent_channel_;
  tclib::ServerChannel *fake_agent_channel() { return *fake_agent_channel_; }
  tclib::def_ref_t<StreamServiceConnector> fake_agent_;
  StreamServiceConnector *fake_agent() { return *fake_agent_; }
  bool has_fake_agent() { return !fake_agent_channel_.is_null(); }
  tclib::def_ref_t<DriverManagerService> service_;
  DriverManagerService *service() { return *service_; }

  tclib::NativeThread agent_monitor_;

  bool trace_;
  static utf8_t executable_path();
};

} // namespace conprx

#endif // _CONPRX_DRIVER_MANAGER_HH
