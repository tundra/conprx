//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "async/promise-inl.hh"
#include "server/conback.hh"

BEGIN_C_INCLUDES
#include "utils/log.h"
#include "utils/misc-inl.h"
#include "utils/string-inl.h"
END_C_INCLUDES

using namespace tclib;
using namespace conprx;
using namespace plankton;

ConsoleBackendService::ConsoleBackendService()
  : backend_(NULL)
  , agent_is_ready_(false)
  , agent_is_done_(false) {

  register_method("log", new_callback(&ConsoleBackendService::on_log, this));
  register_method("is_ready", new_callback(&ConsoleBackendService::on_is_ready, this));
  register_method("is_done", new_callback(&ConsoleBackendService::on_is_done, this));
  register_method("poke", new_callback(&ConsoleBackendService::on_poke, this));

#define __GEN_REGISTER__(Name, name, DLL, API)                                 \
  register_method(#name, new_callback(&ConsoleBackendService::on_##name, this));
  FOR_EACH_LPC_TO_INTERCEPT(__GEN_REGISTER__)
#undef __GEN_REGISTER__

  set_fallback(new_callback(&ConsoleBackendService::message_not_understood, this));
}

BasicConsoleBackend::BasicConsoleBackend()
  : last_poke_(0)
  , input_codepage_(cpUtf8)
  , output_codepage_(cpUtf8)
  , title_(string_empty()) { }

BasicConsoleBackend::~BasicConsoleBackend() {
  string_default_delete(title_);
}

response_t<int64_t> BasicConsoleBackend::poke(int64_t value) {
  int64_t response = last_poke_;
  last_poke_ = value;
  return response_t<int64_t>::of(response);
}

response_t<uint32_t> BasicConsoleBackend::get_console_cp(bool is_output) {
  return response_t<uint32_t>::of(is_output ? output_codepage_ : input_codepage_);
}

response_t<bool_t> BasicConsoleBackend::set_console_cp(uint32_t value, bool is_output) {
  (is_output ? output_codepage_ : input_codepage_) = value;
  return response_t<bool_t>::yes();
}

response_t<uint32_t> BasicConsoleBackend::get_console_title(tclib::Blob buffer,
    bool is_unicode) {
  size_t amount = min_size(buffer.size(), title().size);
  blob_copy_to(tclib::Blob(title().chars, amount), buffer);
  return response_t<uint32_t>::of(static_cast<uint32_t>(amount));
}

response_t<bool_t> BasicConsoleBackend::set_console_title(tclib::Blob title,
    bool is_unicode) {
  string_default_delete(title_);
  utf8_t new_title = new_string(static_cast<char*>(title.start()), title.size());
  title_ = string_default_dup(new_title);
  return response_t<bool_t>::yes();
}

void ConsoleBackendService::on_log(rpc::RequestData *data, ResponseCallback resp) {
  TextWriter writer;
  writer.write(data->argument(0));
  INFO("AGENT LOG: %s", *writer);
  resp(rpc::OutgoingResponse::success(Variant::null()));
}

void ConsoleBackendService::on_is_ready(rpc::RequestData *data, ResponseCallback resp) {
  agent_is_ready_ = true;
  resp(rpc::OutgoingResponse::success(Variant::null()));
}

void ConsoleBackendService::on_is_done(rpc::RequestData *data, ResponseCallback resp) {
  agent_is_done_ = true;
  resp(rpc::OutgoingResponse::success(Variant::null()));
}

template <typename T>
class VariantDefaultConverter {
public:
  static Variant convert(T value) {
    return Variant(value);
  }
};

template <>
class VariantDefaultConverter<bool_t> {
public:
  static Variant convert(bool_t value) {
    return Variant::boolean(value);
  }
};

template <typename T>
static void forward_response(response_t<T> resp, rpc::Service::ResponseCallback callback) {
  if (resp.has_error()) {
    callback(rpc::OutgoingResponse::failure(Variant::integer(resp.error_code())));
  } else {
    callback(rpc::OutgoingResponse::success(VariantDefaultConverter<T>::convert(resp.value())));
  }
}

void ConsoleBackendService::on_poke(rpc::RequestData *data, ResponseCallback resp) {
  int64_t value = data->argument(0).integer_value();
  forward_response(backend()->poke(value), resp);
}

void ConsoleBackendService::on_get_console_cp(rpc::RequestData *data, ResponseCallback resp) {
  bool is_output = data->argument(0).bool_value();
  forward_response(backend()->get_console_cp(is_output), resp);
}

void ConsoleBackendService::on_set_console_cp(rpc::RequestData *data, ResponseCallback resp) {
  uint32_t value = static_cast<uint32_t>(data->argument(0).integer_value());
  bool is_output = data->argument(1).bool_value();
  forward_response(backend()->set_console_cp(value, is_output), resp);
}

void ConsoleBackendService::on_get_console_title(rpc::RequestData *data, ResponseCallback resp) {
  uint32_t length = static_cast<uint32_t>(data->argument(0).integer_value());
  bool is_unicode = data->argument(1).bool_value();
  plankton::Blob scratch_blob = data->factory()->new_blob(length);
  tclib::Blob scratch(scratch_blob.mutable_data(), length);
  blob_fill(scratch, 0);
  response_t<uint32_t> result = backend()->get_console_title(scratch, is_unicode);
  if (result.has_error()) {
    resp(rpc::OutgoingResponse::failure(Variant::integer(result.error_code())));
  } else {
    plankton::Blob response_blob = Variant::blob(scratch_blob.data(), result.value());
    resp(rpc::OutgoingResponse::success(response_blob));
  }
}

void ConsoleBackendService::on_set_console_title(rpc::RequestData *data, ResponseCallback resp) {
  plankton::Blob pchars = data->argument(0);
  tclib::Blob chars(pchars.data(), pchars.size());
  bool is_unicode = data->argument(1).bool_value();
  forward_response(backend()->set_console_title(chars, is_unicode), resp);
}

void ConsoleBackendService::message_not_understood(rpc::RequestData *data,
    ResponseCallback resp) {
  TextWriter writer;
  writer.write(data->selector());
  WARN("Unknown agent message %s", *writer);
  resp(rpc::OutgoingResponse::failure(Variant::null()));
}
