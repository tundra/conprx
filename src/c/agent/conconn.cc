//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Implementation of a console connector that is backed by a remote service
/// over plankton rpc.

#include "agent.hh"
#include "async/promise-inl.hh"
#include "conconn.hh"
#include "plankton-inl.hh"
#include "utils/misc-inl.h"

BEGIN_C_INCLUDES
#include "utils/log.h"
END_C_INCLUDES

using namespace conprx;
using namespace plankton;
using namespace tclib;

fat_bool_t ConsoleAdaptor::get_console_cp(lpc::Message *req, lpc::get_console_cp_m *data) {
  response_t<uint32_t> resp = connector()->get_console_cp(data->is_output);
  req->data()->return_value = static_cast<ulong_t>(resp.error_code());
  data->code_page_id = (resp.has_error() ? 0 : resp.value());
  return F_TRUE;
}

fat_bool_t ConsoleAdaptor::set_console_cp(lpc::Message *req, lpc::set_console_cp_m *data) {
  response_t<bool_t> resp = connector()->set_console_cp(
      static_cast<uint32_t>(data->code_page_id), data->is_output);
  req->data()->return_value = static_cast<ulong_t>(resp.error_code());
  return F_TRUE;
}

fat_bool_t ConsoleAdaptor::set_console_title(lpc::Message *req,
    lpc::set_console_title_m *data) {
  void *start = req->xform().remote_to_local(data->title);
  tclib::Blob blob(start, data->length);
  response_t<bool_t> resp = connector()->set_console_title(blob, data->is_unicode);
  req->data()->return_value = static_cast<ulong_t>(resp.error_code());
  return F_TRUE;
}

fat_bool_t ConsoleAdaptor::get_console_title(lpc::Message *req,
    lpc::get_console_title_m *data) {
  void *start = req->xform().remote_to_local(data->title);
  tclib::Blob scratch(start, data->length);
  response_t<uint32_t> resp = connector()->get_console_title(scratch, data->is_unicode);
  req->data()->return_value = static_cast<ulong_t>(resp.error_code());
  data->length = resp.value();
  return F_TRUE;
}

response_t<int64_t> ConsoleAdaptor::poke(int64_t value) {
  return connector()->poke(value);
}

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
DECLARE_CONVERTER(Variant, variant);

PrpcConsoleConnector::PrpcConsoleConnector(rpc::MessageSocket *socket,
    InputSocket *in)
  : socket_(socket)
  , in_(in) { }

template <typename T, typename C>
response_t<T> PrpcConsoleConnector::send_request(rpc::OutgoingRequest *request,
    rpc::IncomingResponse *resp_out) {
  rpc::IncomingResponse resp = *resp_out = socket()->send_request(request);
  while (!resp->is_settled()) {
    if (!in()->process_next_instruction(NULL))
      return response_t<T>::error(1);
  }
  if (resp->is_fulfilled()) {
    return response_t<T>::of(C::convert(resp->peek_value(Variant::null())));
  } else {
    Variant error = resp->peek_error(Variant::null());
    return response_t<T>::error(static_cast<dword_t>(error.integer_value()));
  }
}

template <typename T>
response_t<T> PrpcConsoleConnector::send_request_default(rpc::OutgoingRequest *request,
    rpc::IncomingResponse *response_out) {
  return send_request< T, DefaultConverter<T> >(request, response_out);
}

response_t<int64_t> PrpcConsoleConnector::poke(int64_t value) {
  Variant arg = value;
  rpc::OutgoingRequest req(Variant::null(), "poke", 1, &arg);
  rpc::IncomingResponse resp;
  return send_request_default<int64_t>(&req, &resp);
}

response_t<uint32_t> PrpcConsoleConnector::get_console_cp(bool is_output) {
  Variant args[1] = {Variant::boolean(is_output)};
  rpc::OutgoingRequest req(Variant::null(), "get_console_cp", 1, args);
  rpc::IncomingResponse resp;
  return send_request_default<uint32_t>(&req, &resp);
}

response_t<bool_t> PrpcConsoleConnector::set_console_cp(uint32_t value, bool is_output) {
  Variant args[2] = {value, Variant::boolean(is_output)};
  rpc::OutgoingRequest req(Variant::null(), "set_console_cp", 2, args);
  rpc::IncomingResponse resp;
  return send_request_default<bool_t>(&req, &resp);
}

response_t<bool_t> PrpcConsoleConnector::set_console_title(tclib::Blob data,
    bool is_unicode) {
  Variant args[2] = {
      Variant::blob(data.start(), static_cast<uint32_t>(data.size())),
      Variant::boolean(is_unicode)
  };
  rpc::OutgoingRequest req(Variant::null(), "set_console_title", 2, args);
  rpc::IncomingResponse resp;
  return send_request_default<bool_t>(&req, &resp);
}

response_t<uint32_t> PrpcConsoleConnector::get_console_title(tclib::Blob buffer,
    bool is_unicode) {
  Variant args[2] = {buffer.size(), Variant::boolean(is_unicode)};
  rpc::OutgoingRequest req(Variant::null(), "get_console_title", 2, args);
  rpc::IncomingResponse resp;
  response_t<Variant> result = send_request_default<Variant>(&req, &resp);
  if (result.has_error())
    return response_t<uint32_t>::error(result);
  plankton::Blob presult = result.value();
  uint32_t amount = static_cast<uint32_t>(min_size(presult.size(), buffer.size()));
  blob_fill(buffer, 0);
  blob_copy_to(tclib::Blob(presult.data(), amount), buffer);
  return response_t<uint32_t>::of(amount);
}

pass_def_ref_t<ConsoleConnector> PrpcConsoleConnector::create(
    rpc::MessageSocket *socket, plankton::InputSocket *in) {
  return pass_def_ref_t<ConsoleConnector>(new (kDefaultAlloc) PrpcConsoleConnector(socket, in));
}
