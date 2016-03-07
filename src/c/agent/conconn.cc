//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Implementation of a console connector that is backed by a remote service
/// over plankton rpc.

#include "agent.hh"
#include "async/promise-inl.hh"
#include "conconn.hh"
#include "plankton-inl.hh"

BEGIN_C_INCLUDES
#include "utils/log.h"
END_C_INCLUDES

using namespace conprx;
using namespace plankton;
using namespace tclib;

// Specializations of this class defines the default conversions for various
// types.
template <typename T>
class DefaultConverter { };

template <>
class DefaultConverter<int64_t> {
public:
  static int64_t convert(const Variant &variant) { return variant.integer_value(); }
};

PrpcConsoleConnector::PrpcConsoleConnector(rpc::MessageSocket *socket,
    InputSocket *in)
  : socket_(socket)
  , in_(in) { }

template <typename T, typename C>
Response<T> PrpcConsoleConnector::send_request(rpc::OutgoingRequest *request,
    rpc::IncomingResponse *resp_out) {
  rpc::IncomingResponse resp = *resp_out = socket()->send_request(request);
  while (!resp->is_settled()) {
    if (!in()->process_next_instruction(NULL))
      return Response<T>::error(1);
  }
  if (resp->is_fulfilled()) {
    return Response<T>::of(C::convert(resp->peek_value(Variant::null())));
  } else {
    Variant error = resp->peek_error(Variant::null());
    return Response<T>::error(static_cast<dword_t>(error.integer_value()));
  }
}

template <typename T>
Response<T> PrpcConsoleConnector::send_request_default(rpc::OutgoingRequest *request,
    rpc::IncomingResponse *response_out) {
  return send_request< T, DefaultConverter<T> >(request, response_out);
}

Response<int64_t> PrpcConsoleConnector::poke(int64_t value) {
  Variant arg = value;
  rpc::OutgoingRequest req(Variant::null(), "poke", 1, &arg);
  rpc::IncomingResponse resp;
  return send_request_default<int64_t>(&req, &resp);
}

Response<int64_t> PrpcConsoleConnector::get_console_cp() {
  rpc::OutgoingRequest req(Variant::null(), "get_cp");
  rpc::IncomingResponse resp;
  return send_request_default<int64_t>(&req, &resp);
}

pass_def_ref_t<ConsoleConnector> PrpcConsoleConnector::create(
    rpc::MessageSocket *socket, plankton::InputSocket *in) {
  return pass_def_ref_t<ConsoleConnector>(new (kDefaultAlloc) PrpcConsoleConnector(socket, in));
}
