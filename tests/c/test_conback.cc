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
class SimulatedFrontendAdaptor {
public:
  SimulatedFrontendAdaptor(ConsoleBackend *backend);
  fat_bool_t initialize();
  ConsoleFrontend *operator->() { return *frontend_; }
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

TEST(conback, simple) {
  BasicConsoleBackend backend;
  SimulatedFrontendAdaptor frontend(&backend);
  ASSERT_F_TRUE(frontend.initialize());
  ASSERT_EQ(0, frontend->poke_backend(423));
  ASSERT_EQ(423, frontend->poke_backend(653));
  ASSERT_EQ(653, frontend->poke_backend(374));
  ASSERT_EQ(374, backend.last_poke());
}
