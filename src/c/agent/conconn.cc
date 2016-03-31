//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Implementation of a console connector that is backed by a remote service
/// over plankton rpc.

#include "agent.hh"
#include "async/promise-inl.hh"
#include "conconn.hh"
#include "marshal-inl.hh"
#include "plankton-inl.hh"
#include "utils/misc-inl.h"

BEGIN_C_INCLUDES
#include "utils/log.h"
END_C_INCLUDES

using namespace conprx;
using namespace plankton;
using namespace tclib;

// Validates the given message, including the assumption that the message's
// payload is of the given type.
template <typename P>
static NtStatus validate_message(lpc::Message *req, P *payload) {
  size_t data_length = sizeof(lpc::message_data_header_t) + sizeof(P);
  if (data_length != req->data_length()) {
    WARN("Unexpected data length [%x]: expected %i, found %i", req->api_number(),
        data_length, req->data_length());
    return NtStatus::from(CONPRX_ERROR_INVALID_DATA_LENGTH);
  }
  size_t total_length = lpc::total_message_length_from_data_length(data_length);
  if (total_length != req->total_length()) {
    WARN("Unexpected total length [%x]: expected %i, found %i", req->api_number(),
        total_length, req->total_length());
    return NtStatus::from(CONPRX_ERROR_INVALID_TOTAL_LENGTH);
  }
  return NtStatus::success();
}

// Runs message validation, returning the error code on failure.
#define VALIDATE_MESSAGE_OR_BAIL(REQ, PAYLOAD) do {                            \
  NtStatus __status__ = validate_message((REQ), (PAYLOAD));                    \
  if (!__status__.is_success())                                                \
    return __status__;                                                         \
} while (false)

NtStatus ConsoleAdaptor::get_console_cp(lpc::Message *req,
    lpc::get_console_cp_m *payload) {
  VALIDATE_MESSAGE_OR_BAIL(req, payload);
  response_t<uint32_t> resp = connector()->get_console_cp(payload->is_output);
  req->set_return_value(NtStatus::from_response(resp));
  payload->code_page_id = (resp.has_error() ? 0 : resp.value());
  return NtStatus::success();
}

NtStatus ConsoleAdaptor::set_console_cp(lpc::Message *req,
    lpc::set_console_cp_m *payload) {
  VALIDATE_MESSAGE_OR_BAIL(req, payload);
  response_t<bool_t> resp = connector()->set_console_cp(
      static_cast<uint32_t>(payload->code_page_id), payload->is_output);
  req->set_return_value(NtStatus::from_response(resp));
  return NtStatus::success();
}

NtStatus ConsoleAdaptor::set_console_title(lpc::Message *req,
    lpc::set_console_title_m *payload) {
  VALIDATE_MESSAGE_OR_BAIL(req, payload);
  void *start = req->xform().remote_to_local(payload->title);
  tclib::Blob blob(start, payload->length);
  response_t<bool_t> resp = connector()->set_console_title(blob, payload->is_unicode);
  req->set_return_value(NtStatus::from_response(resp));
  return NtStatus::success();
}

NtStatus ConsoleAdaptor::get_console_title(lpc::Message *req,
    lpc::get_console_title_m *payload) {
  VALIDATE_MESSAGE_OR_BAIL(req, payload);
  void *start = req->xform().remote_to_local(payload->title);
  tclib::Blob scratch(start, payload->length);
  response_t<uint32_t> resp = connector()->get_console_title(scratch, payload->is_unicode);
  req->set_return_value(NtStatus::from_response(resp));
  payload->length = resp.value();
  return NtStatus::success();
}

NtStatus ConsoleAdaptor::set_console_mode(lpc::Message *req,
    lpc::set_console_mode_m *payload) {
  VALIDATE_MESSAGE_OR_BAIL(req, payload);
  response_t<bool_t> resp = connector()->set_console_mode(
      payload->handle, payload->mode);
  req->set_return_value(NtStatus::from_response(resp));
  return NtStatus::success();
}

NtStatus ConsoleAdaptor::get_console_mode(lpc::Message *req,
    lpc::get_console_mode_m *payload) {
  VALIDATE_MESSAGE_OR_BAIL(req, payload);
  response_t<uint32_t> resp = connector()->get_console_mode(payload->handle);
  req->set_return_value(NtStatus::from_response(resp));
  payload->mode = resp.value();
  return NtStatus::success();
}

NtStatus ConsoleAdaptor::get_console_screen_buffer_info(lpc::Message *req,
    lpc::get_console_screen_buffer_info_m *payload) {
  VALIDATE_MESSAGE_OR_BAIL(req, payload);
  console_screen_buffer_info_t info;
  struct_zero_fill(info);
  response_t<bool_t> resp = connector()->get_console_screen_buffer_info(
      payload->output, &info);
  req->set_return_value(NtStatus::from_response(resp));
  payload->size = info.dwSize;
  payload->cursor_position = info.dwCursorPosition;
  payload->attributes = info.wAttributes;
  payload->maximum_window_size = info.dwMaximumWindowSize;
  payload->window_top_left.X = info.srWindow.Left;
  payload->window_top_left.Y = info.srWindow.Top;
  payload->window_extent.X = static_cast<int16_t>(info.srWindow.Right - info.srWindow.Left + 1);
  payload->window_extent.Y = static_cast<int16_t>(info.srWindow.Bottom - info.srWindow.Top + 1);
  return NtStatus::success();
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
      return response_t<T>::error(CONPRX_ERROR_PROCESSING_INSTRUCTIONS);
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

response_t<uint32_t> PrpcConsoleConnector::get_console_mode(handle_t raw_handle) {
  rpc::OutgoingRequest req(Variant::null(), "get_console_mode");
  Handle handle(raw_handle);
  Variant args[1] = {req.factory()->new_native(&handle)};
  req.set_arguments(1, args);
  rpc::IncomingResponse resp;
  return send_request_default<uint32_t>(&req, &resp);
}

response_t<bool_t> PrpcConsoleConnector::set_console_mode(handle_t raw_handle,
    uint32_t mode) {
  rpc::OutgoingRequest req(Variant::null(), "set_console_mode");
  Handle handle(raw_handle);
  Variant args[2] = {req.factory()->new_native(&handle), mode};
  req.set_arguments(2, args);
  rpc::IncomingResponse resp;
  return send_request_default<bool_t>(&req, &resp);
}

response_t<bool_t> PrpcConsoleConnector::get_console_screen_buffer_info(
    Handle buffer, console_screen_buffer_info_t *info_out) {
  rpc::OutgoingRequest req(Variant::null(), "get_console_screen_buffer_info");
  Variant args[1] = {req.factory()->new_native(&buffer)};
  req.set_arguments(1, args);
  rpc::IncomingResponse resp;
  response_t<Variant> result = send_request_default<Variant>(&req, &resp);
  if (result.has_error())
    return response_t<bool_t>::error(result);
  console_screen_buffer_info_t *info = result.value().native_as<console_screen_buffer_info_t>();
  if (info == NULL)
    return response_t<bool_t>::error(CONPRX_ERROR_INVALID_RESPONSE);
  *info_out = *info;
  return response_t<bool_t>::yes();
}

pass_def_ref_t<ConsoleConnector> PrpcConsoleConnector::create(
    rpc::MessageSocket *socket, plankton::InputSocket *in) {
  return new (kDefaultAlloc) PrpcConsoleConnector(socket, in);
}
