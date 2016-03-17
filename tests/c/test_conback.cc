//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent/conconn.hh"
#include "agent/confront.hh"
#include "bytestream.hh"
#include "rpc.hh"
#include "server/conback.hh"
#include "test.hh"

BEGIN_C_INCLUDES
#include "utils/string-inl.h"
#include "utils/strbuf.h"
END_C_INCLUDES

using namespace tclib;
using namespace plankton;
using namespace conprx;

// An in-memory adaptor that provides a front-end backed by simulating requests
// to the given backend. The reason for testing backends by calling a frontend
// instead of calls to the backend directly is that it allows the tests to be
// run both against the simulated frontend but also the actual windows console,
// that way ensuring that they behave the same way which is what we're really
// interested in.
class SimulatedFrontendAdaptor : public tclib::DefaultDestructable {
public:
  SimulatedFrontendAdaptor(ConsoleBackend *backend);
  virtual ~SimulatedFrontendAdaptor() { }
  virtual void default_destroy() { tclib::default_delete_concrete(this); }
  fat_bool_t initialize();
  ConsoleFrontend *operator->() { return frontend(); }
  ConsoleFrontend *frontend() { return *frontend_; }
private:
  ConsoleBackend *backend_;
  ByteBufferStream buffer_;
  rpc::StreamServiceConnector streams_;
  PrpcConsoleConnector connector_;
  ConsoleAdaptor adaptor_;
  def_ref_t<ConsoleFrontend> frontend_;
  ConsoleBackendService service_;
};

SimulatedFrontendAdaptor::SimulatedFrontendAdaptor(ConsoleBackend *backend)
  : backend_(backend)
  , buffer_(1024)
  , streams_(&buffer_, &buffer_)
  , connector_(streams_.socket(), streams_.input())
  , adaptor_(&connector_) {
  frontend_ = ConsoleFrontend::new_simulating(&adaptor_, 0);
  service_.set_backend(backend_);
}

fat_bool_t SimulatedFrontendAdaptor::initialize() {
  if (!buffer_.initialize())
    return F_FALSE;
  F_TRY(streams_.init(service_.handler()));
  return F_TRUE;
}

// Records all relevant state about a frontend (within reason) and restores it
// on destruction.
class FrontendSaveRestore {
public:
  FrontendSaveRestore();
  ~FrontendSaveRestore();
  void save(ConsoleFrontend *frontend);

private:
  ConsoleFrontend *frontend_;
  ConsoleFrontend *frontend() { return frontend_; }

  uint32_t input_codepage_;
  uint32_t output_codepage_;
};

FrontendSaveRestore::FrontendSaveRestore()
  : frontend_(NULL) { }

void FrontendSaveRestore::save(ConsoleFrontend *frontend) {
  frontend_ = frontend;
  input_codepage_ = frontend->get_console_cp();
  output_codepage_ = frontend->get_console_output_cp();
}

FrontendSaveRestore::~FrontendSaveRestore() {
  if (frontend() == NULL)
    return;
  frontend()->set_console_cp(input_codepage_);
  frontend()->set_console_output_cp(output_codepage_);
}

// A class that holds a frontend, either a native one (only on windows) or a
// simulated one on top of a basic backend. This also takes care of saving
// and restoring the state so mutating the native console doesn't mess up the
// window the test is running in.
class FrontendMultiplexer {
public:
  FrontendMultiplexer(bool use_native)
    : use_native_(use_native)
    , frontend_(NULL) { }
  ConsoleFrontend *operator->() { return frontend_; }
  fat_bool_t initialize();

private:
  bool use_native_;
  ConsoleFrontend *frontend_;
  BasicConsoleBackend backend_;
  def_ref_t<SimulatedFrontendAdaptor> fake_;
  def_ref_t<ConsoleFrontend> native_;
  FrontendSaveRestore save_restore_;
};

fat_bool_t FrontendMultiplexer::initialize() {
  if (use_native_) {
    native_ = IF_MSVC(ConsoleFrontend::new_native(), pass_def_ref_t<ConsoleFrontend>());
    frontend_ = *native_;
  } else {
    fake_ = new (kDefaultAlloc) SimulatedFrontendAdaptor(&backend_);
    F_TRY(fake_->initialize());
    frontend_ = fake_->frontend();
  }
  save_restore_.save(frontend_);
  return F_TRUE;
}

TEST(conback, simulated) {
  BasicConsoleBackend backend;
  SimulatedFrontendAdaptor frontend(&backend);
  ASSERT_TRUE(frontend.initialize());
  ASSERT_EQ(0, frontend->poke_backend(423));
  ASSERT_EQ(423, frontend->poke_backend(653));
  ASSERT_EQ(653, frontend->poke_backend(374));
  ASSERT_EQ(374, backend.last_poke());
}

MULTITEST(conback, set_cp, bool_t, use_native, ("native", true), ("basic", false)) {
  if (use_native && !kIsMsvc)
    SKIP_TEST("msvc only");
  FrontendMultiplexer frontend(use_native);
  ASSERT_F_TRUE(frontend.initialize());

  ASSERT_TRUE(frontend->set_console_cp(cpUtf8));
  ASSERT_TRUE(frontend->set_console_output_cp(cpUtf8));
  ASSERT_EQ(cpUtf8, frontend->get_console_cp());
  ASSERT_EQ(cpUtf8, frontend->get_console_output_cp());

  ASSERT_TRUE(frontend->set_console_cp(cpUsAscii));
  ASSERT_EQ(cpUsAscii, frontend->get_console_cp());
  ASSERT_EQ(cpUtf8, frontend->get_console_output_cp());

  ASSERT_TRUE(frontend->set_console_output_cp(cpUsAscii));
  ASSERT_EQ(cpUsAscii, frontend->get_console_cp());
  ASSERT_EQ(cpUsAscii, frontend->get_console_output_cp());
}
