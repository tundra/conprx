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

#define DECLARE_CONVERTER(type_t, EXPR)                                        \
  template <> class DefaultConverter<type_t> {                                 \
  public:                                                                      \
    static type_t convert(const Variant &variant) {                            \
      return (EXPR);                                                           \
    }                                                                          \
  }

DECLARE_CONVERTER(int64_t, variant.integer_value());
DECLARE_CONVERTER(bool_t, variant.bool_value());
DECLARE_CONVERTER(uint32_t, static_cast<uint32_t>(variant.integer_value()));

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

Response<uint32_t> PrpcConsoleConnector::get_console_cp(bool is_output) {
  Variant args[1] = {Variant::boolean(is_output)};
  rpc::OutgoingRequest req(Variant::null(), "get_console_cp", 1, args);
  rpc::IncomingResponse resp;
  return send_request_default<uint32_t>(&req, &resp);
}

Response<bool_t> PrpcConsoleConnector::set_console_cp(uint32_t value, bool is_output) {
  Variant args[2] = {value, Variant::boolean(is_output)};
  rpc::OutgoingRequest req(Variant::null(), "set_console_cp", 2, args);
  rpc::IncomingResponse resp;
  return send_request_default<bool_t>(&req, &resp);
}

Response<bool_t> PrpcConsoleConnector::set_console_title(tclib::Blob data,
    bool is_unicode) {
  Variant args[2] = {
      Variant::blob(data.start(), static_cast<uint32_t>(data.size())),
      Variant::boolean(is_unicode)
  };
  rpc::OutgoingRequest req(Variant::null(), "set_console_title", 2, args);
  rpc::IncomingResponse resp;
  return send_request_default<bool_t>(&req, &resp);
}

pass_def_ref_t<ConsoleConnector> PrpcConsoleConnector::create(
    rpc::MessageSocket *socket, plankton::InputSocket *in) {
  return pass_def_ref_t<ConsoleConnector>(new (kDefaultAlloc) PrpcConsoleConnector(socket, in));
}
