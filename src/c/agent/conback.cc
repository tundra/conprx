//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Implementation of a console backend that is backed by a remote service over
/// plankton rpc.

#include "agent.hh"
#include "async/promise-inl.hh"
#include "conback.hh"
#include "plankton-inl.hh"

BEGIN_C_INCLUDES
#include "utils/log.h"
END_C_INCLUDES

using namespace conprx;
using namespace plankton;
using namespace tclib;

int64_t RemoteConsoleBackend::poke(int64_t value) {
  Variant arg = value;
  rpc::OutgoingRequest req(Variant::null(), "poke", 1, &arg);
  rpc::IncomingResponse resp;
  if (!agent()->send_request(&req, &resp))
    return 0;
  return resp->peek_value(Variant::null()).integer_value();
}

pass_def_ref_t<ConsoleBackend> RemoteConsoleBackend::create(ConsoleAgent *agent) {
  return pass_def_ref_t<ConsoleBackend>(new (kDefaultAlloc) RemoteConsoleBackend(agent));
}
