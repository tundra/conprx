//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _CONPRX_CONBACK_UTILS_HH
#define _CONPRX_CONBACK_UTILS_HH

#include "agent/agent.hh"
#include "agent/conconn.hh"
#include "agent/confront.hh"
#include "bytestream.hh"
#include "server/conback.hh"

namespace conprx {

// The minimum of agent-like behavior that makes the simulating frontend work.
class SimulatedAgent : public ConsoleAgent {
public:
  SimulatedAgent(ConsoleConnector *connector) : adaptor_(connector) { }
  virtual void default_destroy() { tclib::default_delete_concrete(this); }
  virtual fat_bool_t install_agent_platform() { return F_TRUE; }
  virtual fat_bool_t uninstall_agent_platform() { return F_TRUE; }
  virtual ConsoleAdaptor *adaptor() { return &adaptor_; }

private:
  ConsoleAdaptor adaptor_;
};

// An in-memory adaptor that provides a front-end backed by simulating requests
// to the given backend. The reason for testing backends by calling a frontend
// instead of calls to the backend directly is that it allows the tests to be
// run both against the simulated frontend but also the actual windows console,
// that way ensuring that they behave the same way which is what we're really
// interested in.
class SimulatedFrontendAdaptor : public tclib::DefaultDestructable {
public:
  SimulatedFrontendAdaptor(ConsoleBackend *backend);
  void set_trace(bool value) { trace_ = value; }
  virtual ~SimulatedFrontendAdaptor() { }
  virtual void default_destroy() { tclib::default_delete_concrete(this); }
  fat_bool_t initialize();
  ConsoleFrontend *operator->() { return frontend(); }
  ConsoleFrontend *frontend() { return *frontend_; }
  InMemoryConsolePlatform *platform() { return *platform_; }
private:
  ConsoleBackend *backend_;
  tclib::ByteBufferStream buffer_;
  plankton::rpc::StreamServiceConnector streams_;
  PrpcConsoleConnector connector_;
  SimulatedAgent agent_;
  tclib::def_ref_t<ConsoleFrontend> frontend_;
  tclib::def_ref_t<InMemoryConsolePlatform> platform_;
  ConsoleBackendService service_;
  bool trace_;
  plankton::rpc::TracingMessageSocketObserver tracer_;
};

// A class that holds a frontend, either a native one (only on windows) or a
// simulated one on top of a basic backend.
class FrontendMultiplexer {
public:
  FrontendMultiplexer(bool use_native)
    : use_native_(use_native)
    , frontend_(NULL)
    , platform_(NULL)
    , trace_(false) { }
  ConsoleFrontend *operator->() { return frontend(); }
  fat_bool_t initialize();
  void set_trace(bool value) { trace_ = value; }

  ConsolePlatform *platform() { return platform_; }
  ConsoleFrontend *frontend() { return frontend_; }

private:
  bool use_native_;
  ConsoleFrontend *frontend_;
  ConsolePlatform *platform_;
  BasicConsoleBackend backend_;
  tclib::def_ref_t<SimulatedFrontendAdaptor> fake_frontend_;
  tclib::def_ref_t<ConsoleFrontend> native_frontend_;
  tclib::def_ref_t<ConsolePlatform> native_platform_;
  bool trace_;
};

} // namespace conprx

// Declare a console backend test that runs both the same tests against the
// native and basic backend.
#define CONBACK_TEST(SUITE, NAME) MULTITEST(SUITE, NAME, bool_t, use_native, ("native", true), ("basic", false))

// Dummy function to suppress unused variable warnings.
template <typename T>
static inline void use(const T &value) { }

// Put this at the beginning of a test to get a frontend and platform that have
// been suitably initialized.
#define CONBACK_TEST_PREAMBLE()                                                \
  if (use_native && !kIsMsvc)                                                  \
    SKIP_TEST("msvc only");                                                    \
  FrontendMultiplexer __multiplexer__(use_native);                             \
  ASSERT_F_TRUE(__multiplexer__.initialize());                                 \
  ConsoleFrontend *frontend = __multiplexer__.frontend();                      \
  ConsolePlatform *platform = __multiplexer__.platform();                      \
  do {                                                                         \
    if (false) {                                                               \
      use(platform);                                                           \
      use(frontend);                                                           \
    }                                                                          \
  } while (false)

#endif // _CONPRX_CONBACK_UTILS_HH
