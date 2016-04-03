//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "async/promise-inl.hh"
#include "marshal-inl.hh"
#include "server/conback.hh"
#include "utils/string.hh"

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

  registry()->add_fallback(ConsoleTypes::registry());

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

// Dummy implementation used when none has been explicitly specified.
class NoConsoleWindow : public ConsoleWindow {
public:
  virtual coord_t size() { return coord_zero(); }
  virtual coord_t cursor_position() { return coord_zero(); }
  virtual word_t attributes() { return 0; }
  virtual small_rect_t position() { return small_rect_zero(); }
  virtual coord_t maximum_window_size() { return coord_zero(); }
  static NoConsoleWindow *get();

private:
  static NoConsoleWindow instance;
  static coord_t coord_zero() { coord_t result = {0, 0}; return result; }
  static small_rect_t small_rect_zero() { small_rect_t result = {0, 0, 0, 0}; return result; }
};

NoConsoleWindow NoConsoleWindow::instance;

NoConsoleWindow *NoConsoleWindow::get() {
  return &instance;
}

BasicConsoleBackend::BasicConsoleBackend()
  : last_poke_(0)
  , input_codepage_(cpUtf8)
  , output_codepage_(cpUtf8)
  , title_(string_empty())
  , mode_(0)
  , window_(NoConsoleWindow::get()) { }

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
    bool is_unicode, size_t *bytes_written_out) {
  return is_unicode
      ? get_console_title_wide(buffer, bytes_written_out)
      : get_console_title_ansi(buffer, bytes_written_out);
}

response_t<uint32_t> BasicConsoleBackend::get_console_title_wide(tclib::Blob buffer,
    size_t *bytes_written_out) {
  if (buffer.size() == 0) {
    *bytes_written_out = 0;
    return response_t<uint32_t>::of(0);
  }
  // This is super verbose but it's sooo easy to get the whole title-length,
  // buffer-length, null/no-null mixed up.
  wide_str_t wide_buf = static_cast<wide_str_t>(buffer.start());
  size_t title_chars_no_null = title().size;
  size_t buffer_chars_with_null = buffer.size() / sizeof(wide_char_t);
  size_t buffer_chars_no_null = buffer_chars_with_null - 1;
  size_t char_to_copy_no_null = min_size(title_chars_no_null, buffer_chars_no_null);
  for (size_t i = 0; i < char_to_copy_no_null; i++)
    wide_buf[i] = title_.chars[i];
  wide_buf[char_to_copy_no_null] = '\0';
  *bytes_written_out = char_to_copy_no_null * sizeof(wide_char_t);
  return response_t<uint32_t>::of(static_cast<uint32_t>(title_chars_no_null * sizeof(wide_char_t)));
}

response_t<uint32_t> BasicConsoleBackend::get_console_title_ansi(tclib::Blob buffer,
    size_t *bytes_written_out) {
  size_t title_chars_no_null = title().size;
  if (buffer.size() < title_chars_no_null) {
    // We refuse to return less than the full title if the buffer is too small.
    // Still null-terminate though.
    if (buffer.size() > 0)
      static_cast<byte_t*>(buffer.start())[0] = 0;
    return response_t<uint32_t>::of(0);
  }
  blob_copy_to(tclib::Blob(title().chars, title_chars_no_null), buffer);
  // There's a weird corner case here where we'll allow a buffer that's the
  // null terminator too short to hold the complete terminated title -- but we
  // return the title anyway and overwrite the last character with the
  // terminator.
  size_t null_index = (buffer.size() == title_chars_no_null) ? (title_chars_no_null - 1) : title_chars_no_null;
  *bytes_written_out = null_index;
  return response_t<uint32_t>::of(static_cast<uint32_t>(title_chars_no_null));
}

utf8_t BasicConsoleBackend::blob_to_utf8_dumb(tclib::Blob title, bool is_unicode) {
  if (is_unicode) {
    wide_str_t wstr = static_cast<wide_str_t>(title.start());
    size_t length = title.size() / sizeof(wide_char_t);
    blob_t memory = allocator_default_malloc(length + 1);
    char *chars = static_cast<char*>(memory.start);
    for (size_t i = 0; i < length; i++)
      chars[i] = static_cast<char>(wstr[i]);
    chars[length] = '\0';
    return new_string(chars, length);
  } else {
    utf8_t as_utf8 = new_string(static_cast<char*>(title.start()), title.size());
    return string_default_dup(as_utf8);
  }
}

response_t<bool_t> BasicConsoleBackend::set_console_title(tclib::Blob title,
    bool is_unicode) {
  string_default_delete(title_);
  title_ = blob_to_utf8_dumb(title, is_unicode);
  return response_t<bool_t>::yes();
}

response_t<uint32_t> BasicConsoleBackend::get_console_mode(Handle handle) {
  WARN("Using stub get_console_mode(%p)", handle.ptr());
  return response_t<uint32_t>::of(mode_);
}

response_t<bool_t> BasicConsoleBackend::set_console_mode(Handle handle, uint32_t mode) {
  WARN("Using stub set_console_mode(%p, %x)", handle.ptr(), mode);
  mode_ = mode;
  return response_t<bool_t>::yes();
}

response_t<bool_t> BasicConsoleBackend::get_console_screen_buffer_info(
    Handle buffer, console_screen_buffer_infoex_t *info_out) {
  struct_zero_fill(*info_out);
  info_out->dwSize = window()->size();
  info_out->dwCursorPosition = window()->cursor_position();
  info_out->wAttributes = window()->attributes();
  info_out->srWindow = window()->position();
  info_out->dwMaximumWindowSize = window()->maximum_window_size();
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
  uint32_t byte_size = static_cast<uint32_t>(data->argument(0).integer_value());
  bool is_unicode = data->argument(1).bool_value();
  plankton::Blob scratch_blob = data->factory()->new_blob(byte_size);
  tclib::Blob scratch(scratch_blob.mutable_data(), byte_size);
  blob_fill(scratch, 0);
  size_t bytes_written = 0;
  response_t<uint32_t> result = backend()->get_console_title(scratch, is_unicode,
      &bytes_written);
  if (result.has_error()) {
    resp(rpc::OutgoingResponse::failure(Variant::integer(result.error_code())));
  } else {
    plankton::Blob response_blob = Variant::blob(scratch_blob.data(),
        static_cast<uint32_t>(bytes_written));
    Array pair = data->factory()->new_array(2);
    pair.add(response_blob);
    pair.add(result.value());
    resp(rpc::OutgoingResponse::success(pair));
  }
}

void ConsoleBackendService::on_set_console_title(rpc::RequestData *data, ResponseCallback resp) {
  plankton::Blob pchars = data->argument(0);
  tclib::Blob chars(pchars.data(), pchars.size());
  bool is_unicode = data->argument(1).bool_value();
  forward_response(backend()->set_console_title(chars, is_unicode), resp);
}

void ConsoleBackendService::on_set_console_mode(rpc::RequestData *data, ResponseCallback resp) {
  Handle *handle = data->argument(0).native_as<Handle>();
  if (handle == NULL)
    return resp(rpc::OutgoingResponse::failure(CONPRX_ERROR_EXPECTED_HANDLE));
  uint32_t mode = static_cast<uint32_t>(data->argument(1).integer_value());
  forward_response(backend()->set_console_mode(*handle, mode), resp);
}

void ConsoleBackendService::on_get_console_mode(rpc::RequestData *data, ResponseCallback resp) {
  Handle *handle = data->argument(0).native_as<Handle>();
  if (handle == NULL)
    return resp(rpc::OutgoingResponse::failure(CONPRX_ERROR_EXPECTED_HANDLE));
  forward_response(backend()->get_console_mode(*handle), resp);
}

void ConsoleBackendService::on_get_console_screen_buffer_info(rpc::RequestData *data,
    ResponseCallback resp) {
  Handle *output = data->argument(0).native_as<Handle>();
  if (output == NULL)
    return resp(rpc::OutgoingResponse::failure(CONPRX_ERROR_EXPECTED_HANDLE));
  console_screen_buffer_infoex_t *info = new (data->factory()) console_screen_buffer_infoex_t;
  struct_zero_fill(*info);
  response_t<bool_t> result = backend()->get_console_screen_buffer_info(*output, info);
  if (result.has_error()) {
    return resp(rpc::OutgoingResponse::failure(result.error_code()));
  } else {
    return resp(rpc::OutgoingResponse::success(data->factory()->new_native(info)));
  }
}

void ConsoleBackendService::message_not_understood(rpc::RequestData *data,
    ResponseCallback resp) {
  TextWriter writer;
  writer.write(data->selector());
  WARN("Unknown agent message %s", *writer);
  resp(rpc::OutgoingResponse::failure(Variant::null()));
}
