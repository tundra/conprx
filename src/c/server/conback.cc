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

#define __GEN_REGISTER__(Name, name, NUM, FLAGS)                               \
  lfPa FLAGS (, register_method(#name, new_callback(&ConsoleBackendService::on_##name, this)));
  FOR_EACH_LPC_TO_INTERCEPT(__GEN_REGISTER__)
#undef __GEN_REGISTER__

  set_fallback(new_callback(&ConsoleBackendService::message_not_understood, this));
}

// Dummy implementation used when none has been explicitly specified.
class NoWinTty : public WinTty {
public:
  virtual void default_destroy() { tclib::default_delete_concrete(this); }
  virtual response_t<bool_t> get_screen_buffer_info(bool is_error,
      ConsoleScreenBufferInfo *info_out);
  virtual response_t<uint32_t> write(tclib::Blob blob, bool is_unicode, bool is_error);
  virtual response_t<uint32_t> read(tclib::Blob buffer, bool is_unicode,
      ReadConsoleControl *input_control);
  static NoWinTty *get();

private:
  static NoWinTty instance;
  static coord_t coord_zero() { return coord_new(0, 0); }
  static small_rect_t small_rect_zero() { return small_rect_new(0, 0, 0, 0); }
};

response_t<bool_t> NoWinTty::get_screen_buffer_info(bool is_error,
    ConsoleScreenBufferInfo *info_out) {
  return response_t<bool_t>::error(CONPRX_ERROR_NOT_IMPLEMENTED);
}

response_t<uint32_t> NoWinTty::write(tclib::Blob blob, bool is_unicode, bool is_error) {
  return response_t<uint32_t>::error(CONPRX_ERROR_NOT_IMPLEMENTED);
}

response_t<uint32_t> NoWinTty::read(tclib::Blob buffer, bool is_unicode,
    ReadConsoleControl *input_control) {
  return response_t<uint32_t>::error(CONPRX_ERROR_NOT_IMPLEMENTED);
}

NoWinTty NoWinTty::instance;

NoWinTty *NoWinTty::get() {
  return &instance;
}

BasicConsoleBackend::BasicConsoleBackend()
  : last_poke_(0)
  , input_codepage_(cpUtf8)
  , output_codepage_(cpUtf8)
  , title_(ucs16_empty())
  , mode_(0)
  , wty_(NoWinTty::get()) { }

BasicConsoleBackend::~BasicConsoleBackend() {
  ucs16_default_delete(title_);
}

response_t<bool_t> BasicConsoleBackend::connect(Handle stdin_handle,
    Handle stdout_handle, Handle stderr_handle) {
  handles()->register_std_handle(kStdInputHandle, stdin_handle, 0);
  handles()->register_std_handle(kStdOutputHandle, stdout_handle, 0);
  handles()->register_std_handle(kStdErrorHandle, stderr_handle, 0);
  return response_t<bool_t>::yes();
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
  wide_str_t wstr = static_cast<wide_str_t>(buffer.start());
  size_t title_chars_no_null = title().length;
  size_t buffer_chars_with_null = buffer.size() / sizeof(wide_char_t);
  size_t buffer_chars_no_null = buffer_chars_with_null - 1;
  size_t char_to_copy_no_null = min_size(title_chars_no_null, buffer_chars_no_null);
  for (size_t i = 0; i < char_to_copy_no_null; i++)
    wstr[i] = title().chars[i];
  wstr[char_to_copy_no_null] = '\0';
  *bytes_written_out = char_to_copy_no_null * sizeof(wide_char_t);
  return response_t<uint32_t>::of(static_cast<uint32_t>(title_chars_no_null * sizeof(wide_char_t)));
}

response_t<uint32_t> BasicConsoleBackend::get_console_title_ansi(tclib::Blob buffer,
    size_t *bytes_written_out) {
  size_t title_chars_no_null = title().length;
  ansi_str_t astr = static_cast<ansi_str_t>(buffer.start());
  if (buffer.size() < title_chars_no_null) {
    // We refuse to return less than the full title if the buffer is too small.
    *bytes_written_out = 0;
    return response_t<uint32_t>::of(0);
  }
  for (size_t i = 0; i < title_chars_no_null; i++)
    astr[i] = MsDosCodec::wide_to_ansi_char(title().chars[i]);
  // There's a weird corner case here where we'll allow a buffer that's the
  // null terminator too short to hold the complete terminated title -- but we
  // return the title anyway and overwrite the last character with the
  // terminator.
  size_t null_index = (buffer.size() == title_chars_no_null) ? (title_chars_no_null - 1) : title_chars_no_null;
  *bytes_written_out = null_index;
  return response_t<uint32_t>::of(static_cast<uint32_t>(title_chars_no_null));
}

ucs16_t BasicConsoleBackend::blob_to_ucs16(tclib::Blob blob, bool is_unicode) {
  if (is_unicode) {
    wide_str_t wstr = static_cast<wide_str_t>(blob.start());
    size_t length = blob.size() / sizeof(wide_char_t);
    return ucs16_default_dup(ucs16_new(wstr, length));
  } else {
    size_t length = blob.size();
    size_t size = (blob.size() + 1) * sizeof(wide_char_t);
    wide_str_t wstr = static_cast<wide_str_t>(allocator_default_malloc(size).start);
    ansi_str_t astr = static_cast<ansi_str_t>(blob.start());
    for (size_t i = 0; i < length; i++)
      wstr[i] = MsDosCodec::ansi_to_wide_char(astr[i]);
    wstr[length] = 0;
    return ucs16_new(wstr, blob.size());
  }
}

size_t BasicConsoleBackend::ucs16_to_blob(ucs16_t str, tclib::Blob blob, bool is_unicode) {
  if (is_unicode) {
    size_t size = min_size(str.length * sizeof(wide_char_t), blob.size());
    memcpy(blob.start(), str.chars, size);
    return size;
  } else {
    size_t length = min_size(blob.size(), str.length);
    ansi_str_t astr = static_cast<ansi_str_t>(blob.start());
    for (size_t i = 0; i < length; i++)
      astr[i] = MsDosCodec::wide_to_ansi_char(str.chars[i]);
    return length;
  }
}

response_t<bool_t> BasicConsoleBackend::set_console_title(tclib::Blob title,
    bool is_unicode) {
  ucs16_default_delete(title_);
  title_ = blob_to_ucs16(title, is_unicode);
  return response_t<bool_t>::yes();
}

void BasicConsoleBackend::set_title(const char *value) {
  set_console_title(StringUtils::as_blob(value, false), false);
}

HandleShadow BasicConsoleBackend::get_handle_shadow(Handle handle) {
  return handles()->get_shadow(handle);
}

response_t<bool_t> BasicConsoleBackend::set_console_mode(Handle handle, uint32_t mode) {
  handles()->set_handle_mode(handle, mode);
  return response_t<bool_t>::yes();
}

response_t<bool_t> BasicConsoleBackend::get_console_screen_buffer_info(
    Handle buffer, ConsoleScreenBufferInfo *info_out) {
  HandleShadow shadow = get_handle_shadow(buffer);
  return wty()->get_screen_buffer_info(shadow.is_error(), info_out);
}

response_t<uint32_t> BasicConsoleBackend::write_console(Handle output,
    tclib::Blob data, bool is_unicode) {
  HandleShadow shadow = get_handle_shadow(output);
  return wty()->write(data, is_unicode, shadow.is_error());
}

response_t<uint32_t> BasicConsoleBackend::read_console(Handle input,
    tclib::Blob buffer, bool is_unicode, size_t *bytes_read_out,
    ReadConsoleControl *input_control) {
  response_t<uint32_t> resp = wty()->read(buffer, is_unicode, input_control);
  if (resp.has_error())
    return resp;
  *bytes_read_out = resp.value();
  return resp;
}

void ConsoleBackendService::on_log(rpc::RequestData *data, ResponseCallback resp) {
  TextWriter writer;
  writer.write(data->argument(0));
  INFO("AGENT LOG: %s", *writer);
  resp(rpc::OutgoingResponse::success(Variant::null()));
}

void ConsoleBackendService::on_is_ready(rpc::RequestData *data, ResponseCallback resp) {
  Handle *stdin_handle = data->argument("stdin").native_as<Handle>();
  Handle *stdout_handle = data->argument("stdout").native_as<Handle>();
  Handle *stderr_handle = data->argument("stderr").native_as<Handle>();
  if (stdin_handle == NULL || stdout_handle == NULL || stderr_handle == NULL)
    return resp(rpc::OutgoingResponse::failure(CONPRX_ERROR_EXPECTED_HANDLE));
  if (backend() != NULL)
    backend()->connect(*stdin_handle, *stdout_handle, *stderr_handle);
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

tclib::Blob ConsoleBackendService::to_blob(Variant value) {
  return tclib::Blob(value.blob_data(), value.blob_size());
}

void ConsoleBackendService::on_write_console(rpc::RequestData *data, ResponseCallback resp) {
  Handle *handle = data->argument(0).native_as<Handle>();
  if (handle == NULL)
    return resp(rpc::OutgoingResponse::failure(CONPRX_ERROR_EXPECTED_HANDLE));
  tclib::Blob chars = to_blob(data->argument(1));
  bool is_unicode = data->argument(2).bool_value();
  forward_response(backend()->write_console(*handle, chars, is_unicode), resp);
}

void ConsoleBackendService::on_read_console(rpc::RequestData *data, ResponseCallback resp) {
  Handle *handle = data->argument(0).native_as<Handle>();
  if (handle == NULL)
    return resp(rpc::OutgoingResponse::failure(CONPRX_ERROR_EXPECTED_HANDLE));
  uint32_t byte_size = static_cast<uint32_t>(data->argument(1).integer_value());
  bool is_unicode = data->argument(2).bool_value();
  console_readconsole_control_t *control_in = data->argument(3).native_as<console_readconsole_control_t>();
  if (control_in == NULL)
    return resp(rpc::OutgoingResponse::failure(CONPRX_ERROR_INVALID_ARGUMENT));
  ReadConsoleControl control_out(control_in);
  plankton::Blob scratch_blob = data->factory()->new_blob(byte_size);
  tclib::Blob scratch(scratch_blob.mutable_data(), byte_size);
  blob_fill(scratch, 0);
  size_t bytes_read = 0;
  response_t<uint32_t> result = backend()->read_console(*handle, scratch, is_unicode,
      &bytes_read, &control_out);
  if (result.has_error()) {
    resp(rpc::OutgoingResponse::failure(Variant::integer(result.error_code())));
  } else {
    plankton::Blob response_blob = Variant::blob(scratch_blob.data(),
        static_cast<uint32_t>(bytes_read));
    Map response = data->factory()->new_map();
    response.set("data", response_blob);
    response.set("result", result.value());
    NativeVariant control_var(control_out.as_winapi());
    response.set("input_control", control_var);
    resp(rpc::OutgoingResponse::success(response));
  }
}

void ConsoleBackendService::on_set_console_title(rpc::RequestData *data, ResponseCallback resp) {
  tclib::Blob chars = to_blob(data->argument(0));
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

void ConsoleBackendService::on_get_console_screen_buffer_info(rpc::RequestData *data,
    ResponseCallback resp) {
  Handle *output = data->argument(0).native_as<Handle>();
  if (output == NULL)
    return resp(rpc::OutgoingResponse::failure(CONPRX_ERROR_EXPECTED_HANDLE));
  ConsoleScreenBufferInfo *info = new (data->factory()) ConsoleScreenBufferInfo();
  response_t<bool_t> result = backend()->get_console_screen_buffer_info(*output, info);
  if (result.has_error()) {
    return resp(rpc::OutgoingResponse::failure(result.error_code()));
  } else {
    NativeVariant info_var(info->as_ex());
    return resp(rpc::OutgoingResponse::success(info_var));
  }
}

void ConsoleBackendService::message_not_understood(rpc::RequestData *data,
    ResponseCallback resp) {
  TextWriter writer;
  writer.write(data->selector());
  WARN("Unknown agent message %s", *writer);
  resp(rpc::OutgoingResponse::failure(Variant::null()));
}
