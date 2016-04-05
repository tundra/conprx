//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Console api implementation specific to the windows platform.

#include "agent/agent.hh"
#include "agent/conconn.hh"
#include "agent/confront.hh"

BEGIN_C_INCLUDES
#include "utils/string-inl.h"
END_C_INCLUDES

using namespace conprx;
using namespace tclib;

class AbstractSimulatedMessage;

// The fallback console is a console implementation that works on all platforms.
// The implementation is just placeholders but they should be functional enough
// that they can be used for testing.
class SimulatingConsoleFrontend : public ConsoleFrontend {
public:
  SimulatingConsoleFrontend(ConsoleAgent *agent, lpc::AddressXform xform);
  ~SimulatingConsoleFrontend();
  virtual void default_destroy() { tclib::default_delete_concrete(this); }

#define __DECLARE_CONAPI_METHOD__(Name, name, FLAGS, SIG, PSIG)                \
  virtual SIG(GET_SIG_RET) name SIG(GET_SIG_PARAMS);
  FOR_EACH_CONAPI_FUNCTION(__DECLARE_CONAPI_METHOD__)
#undef __DECLARE_CONAPI_METHOD__

  uint32_t get_console_cp(bool is_output);
  bool_t set_console_cp(uint32_t value, bool is_output);

  bool_t write_console_aw(handle_t output, const void *buffer, dword_t chars_to_write,
      dword_t *chars_written, bool is_unicode);

  virtual NtStatus get_last_error();

  virtual int64_t poke_backend(int64_t value);

  lpc::AddressXform xform() { return xform_; }

  // Sets the last_error_ field appropriate based on the state of the given
  // message and returns true if the state was successful, making this value
  // appropriate as a return value from any boolean console functions.
  bool update_last_error(AbstractSimulatedMessage *message);

private:
  ConsoleAgent *agent() { return agent_; }
  ConsoleAgent *agent_;
  lpc::AddressXform xform_;
  NtStatus last_error_;

};

SimulatingConsoleFrontend::SimulatingConsoleFrontend(ConsoleAgent *agent,
    lpc::AddressXform xform)
  : agent_(agent)
  , xform_(xform)
  , last_error_(NtStatus::success()) { }

SimulatingConsoleFrontend::~SimulatingConsoleFrontend() {
}

int64_t SimulatingConsoleFrontend::poke_backend(int64_t value) {
  response_t<int64_t> result = agent()->adaptor()->poke(value);
  if (result.has_error()) {
    last_error_ = NtStatus::from_response(result);
    return 0;
  } else {
    last_error_ = NtStatus::success();
    return result.value();
  }
}

handle_t SimulatingConsoleFrontend::get_std_handle(dword_t n) {
  int32_t sn = static_cast<int32_t>(n);
  if (-12 <= sn && sn <= -10) {
    return reinterpret_cast<handle_t>(static_cast<size_t>(100 - sn));
  } else {
    return reinterpret_cast<handle_t>(IF_32_BIT(-1, -1LL));
  }
}

// Static mapping from message keys to some types and functions it's convenient
// to have bundled together.
template <ConsoleAgent::lpc_method_key_t K> struct MessageInfo { };

#define __DECLARE_MESSAGE_INFO__(Name, name, APINUM, FLAGS)                    \
  template <> struct MessageInfo<ConsoleAgent::lm##Name> {                     \
    typedef lpc::name##_m data_m;                                              \
    static data_m *get_payload(lpc::message_data_t *data) { return &data->payload.name; } \
  };
FOR_EACH_LPC_TO_INTERCEPT(__DECLARE_MESSAGE_INFO__)
#undef __DECLARE_MESSAGE_INFO__

// Convenience class for constructing simulated messages.
class AbstractSimulatedMessage {
public:
  AbstractSimulatedMessage(SimulatingConsoleFrontend *frontend);
  lpc::Message *message() { return &message_; }
  lpc::message_data_t *operator->() { return data(); }

  int32_t return_value() { return data()->header.return_value; }

protected:
  lpc::message_data_t *data() { return &data_; }

private:
  lpc::message_data_t data_;
  lpc::Message message_;
};

template <ConsoleAgent::lpc_method_key_t K>
class SimulatedMessage : public AbstractSimulatedMessage {
public:
  SimulatedMessage(SimulatingConsoleFrontend *frontend);
  typename MessageInfo<K>::data_m *payload() { return MessageInfo<K>::get_payload(data()); }
private:
};

template <ConsoleAgent::lpc_method_key_t K>
SimulatedMessage<K>::SimulatedMessage(SimulatingConsoleFrontend *frontend)
  : AbstractSimulatedMessage(frontend) {
  data()->header.api_number = K;
  size_t data_length = sizeof(lpc::message_data_header_t) + sizeof(typename MessageInfo<K>::data_m);
  data()->port_message.u1.s1.data_length = static_cast<uint16_t>(data_length);
  size_t total_length = lpc::total_message_length_from_data_length(data_length);
  data()->port_message.u1.s1.total_length = static_cast<uint16_t>(total_length);
}

bool SimulatingConsoleFrontend::update_last_error(AbstractSimulatedMessage *message) {
  NtStatus status = NtStatus::from_nt(message->return_value());
  last_error_ = status;
  return status.is_success();
}

AbstractSimulatedMessage::AbstractSimulatedMessage(SimulatingConsoleFrontend *frontend)
  : message_(data(), data(), NULL, frontend->xform()) {
  struct_zero_fill(data_);
}

uint32_t SimulatingConsoleFrontend::get_console_cp(bool is_output) {
  SimulatedMessage<ConsoleAgent::lmGetConsoleCP> message(this);
  lpc::get_console_cp_m *payload = message.payload();
  payload->is_output = is_output;
  agent()->on_message(message.message());
  update_last_error(&message);
  return payload->code_page_id;
}

uint32_t SimulatingConsoleFrontend::get_console_cp() {
  return get_console_cp(false);
}

uint32_t SimulatingConsoleFrontend::get_console_output_cp() {
  return get_console_cp(true);
}

bool_t SimulatingConsoleFrontend::set_console_cp(uint32_t value, bool is_output) {
  SimulatedMessage<ConsoleAgent::lmSetConsoleCP> message(this);
  lpc::set_console_cp_m *payload = message.payload();
  payload->is_output = is_output;
  payload->code_page_id = value;
  agent()->on_message(message.message());
  return update_last_error(&message);
}

bool_t SimulatingConsoleFrontend::set_console_cp(uint32_t value) {
  return set_console_cp(value, false);
}


bool_t SimulatingConsoleFrontend::set_console_output_cp(uint32_t value) {
  return set_console_cp(value, true);
}

bool_t SimulatingConsoleFrontend::set_console_title_a(ansi_cstr_t str) {
  SimulatedMessage<ConsoleAgent::lmSetConsoleTitle> message(this);
  lpc::set_console_title_m *payload = message.payload();
  tclib::Blob blob = StringUtils::as_blob(str);
  payload->size_in_bytes_in = static_cast<uint32_t>(blob.size());
  payload->title = xform().local_to_remote(blob.start());
  payload->is_unicode = false;
  agent()->on_message(message.message());
  return update_last_error(&message);
}

dword_t SimulatingConsoleFrontend::get_console_title_a(ansi_str_t str, dword_t n) {
  SimulatedMessage<ConsoleAgent::lmGetConsoleTitle> message(this);
  lpc::get_console_title_m *payload = message.payload();
  payload->size_in_bytes_in = n;
  payload->title = xform().local_to_remote(str);
  payload->is_unicode = false;
  agent()->on_message(message.message());
  update_last_error(&message);
  return payload->length_in_chars_out;
}

bool_t SimulatingConsoleFrontend::set_console_title_w(wide_cstr_t str) {
  SimulatedMessage<ConsoleAgent::lmSetConsoleTitle> message(this);
  lpc::set_console_title_m *payload = message.payload();
  tclib::Blob blob = StringUtils::as_blob(str);
  payload->size_in_bytes_in = static_cast<uint32_t>(blob.size());
  payload->title = xform().local_to_remote(blob.start());
  payload->is_unicode = true;
  agent()->on_message(message.message());
  return update_last_error(&message);
}

dword_t SimulatingConsoleFrontend::get_console_title_w(wide_str_t str, dword_t n) {
  SimulatedMessage<ConsoleAgent::lmGetConsoleTitle> message(this);
  lpc::get_console_title_m *payload = message.payload();
  payload->size_in_bytes_in = n;
  payload->title = xform().local_to_remote(str);
  payload->is_unicode = true;
  agent()->on_message(message.message());
  update_last_error(&message);
  return payload->length_in_chars_out;
}

bool_t SimulatingConsoleFrontend::write_console_a(handle_t output, const void *buffer,
    dword_t chars_to_write, dword_t *chars_written, void *reserved) {
  return write_console_aw(output, buffer, chars_to_write, chars_written, false);
}

bool_t SimulatingConsoleFrontend::write_console_w(handle_t output, const void *buffer,
    dword_t chars_to_write, dword_t *chars_written, void *reserved) {
  return write_console_aw(output, buffer, chars_to_write, chars_written, true);
}

bool_t SimulatingConsoleFrontend::write_console_aw(handle_t output, const void *buffer,
    dword_t chars_to_write, dword_t *chars_written, bool is_unicode) {
  SimulatedMessage<ConsoleAgent::lmWriteConsole> message(this);
  lpc::write_console_m *payload = message.payload();
  size_t char_size = is_unicode ? sizeof(wide_char_t) : 1;
  size_t size_in_bytes = chars_to_write * char_size;
  payload->output = output;
  payload->size_in_bytes = static_cast<uint32_t>(size_in_bytes);
  payload->is_unicode = is_unicode;
  if (size_in_bytes <= lpc::kMaxInlineBytes) {
    payload->is_inline = true;
    payload->contents = const_cast<void*>(buffer);
  } else {
    payload->is_inline = false;
    payload->contents = const_cast<void*>(xform().local_to_remote(buffer));
  }
  agent()->on_message(message.message());
  if (chars_written != NULL)
    *chars_written = static_cast<dword_t>(payload->size_in_bytes / char_size);
  return update_last_error(&message);
}

bool_t SimulatingConsoleFrontend::get_console_mode(handle_t handle, dword_t *mode_out) {
  SimulatedMessage<ConsoleAgent::lmGetConsoleMode> message(this);
  lpc::get_console_mode_m *payload = message.payload();
  payload->handle = handle;
  payload->mode = 0;
  agent()->on_message(message.message());
  *mode_out = payload->mode;
  return update_last_error(&message);
}

bool_t SimulatingConsoleFrontend::set_console_mode(handle_t handle, dword_t mode) {
  SimulatedMessage<ConsoleAgent::lmSetConsoleMode> message(this);
  lpc::set_console_mode_m *payload = message.payload();
  payload->handle = handle;
  payload->mode = mode;
  agent()->on_message(message.message());
  return update_last_error(&message);
}

// Helper that copies data from an lpc payload into a screen buffer info
// struct.
static void store_console_screen_buffer_info(lpc::get_console_screen_buffer_info_m *payload,
    console_screen_buffer_info_t *info_out) {
  info_out->dwSize = payload->size;
  info_out->dwCursorPosition = payload->cursor_position;
  info_out->wAttributes = payload->attributes;
  info_out->dwMaximumWindowSize = payload->maximum_window_size;
  short_t window_left = payload->window_top_left.X;
  short_t window_top = payload->window_top_left.Y;
  info_out->srWindow.Left = window_left;
  info_out->srWindow.Top = window_top;
  info_out->srWindow.Right = static_cast<short_t>(window_left + payload->window_extent.X - 1);
  info_out->srWindow.Bottom = static_cast<short_t>(window_top + payload->window_extent.Y - 1);
}

bool_t SimulatingConsoleFrontend::get_console_screen_buffer_info(handle_t handle,
    console_screen_buffer_info_t *info_out) {
  SimulatedMessage<ConsoleAgent::lmGetConsoleScreenBufferInfo> message(this);
  lpc::get_console_screen_buffer_info_m *payload = message.payload();
  payload->output = handle;
  agent()->on_message(message.message());
  store_console_screen_buffer_info(payload, info_out);
  return update_last_error(&message);
}

bool_t SimulatingConsoleFrontend::get_console_screen_buffer_info_ex(handle_t handle,
    console_screen_buffer_infoex_t *info_out) {
  SimulatedMessage<ConsoleAgent::lmGetConsoleScreenBufferInfo> message(this);
  lpc::get_console_screen_buffer_info_m *payload = message.payload();
  payload->output = handle;
  agent()->on_message(message.message());
  store_console_screen_buffer_info(payload, console_screen_buffer_info_from_ex(info_out));
  info_out->wPopupAttributes = payload->popup_attributes;
  info_out->bFullscreenSupported = payload->fullscreen_supported;
  memcpy(info_out->ColorTable, payload->color_table, sizeof(info_out->ColorTable));
  return update_last_error(&message);
}

NtStatus SimulatingConsoleFrontend::get_last_error() {
  return last_error_;
}

pass_def_ref_t<ConsoleFrontend> ConsoleFrontend::new_simulating(ConsoleAgent *agent,
    ssize_t delta) {
  return new (kDefaultAlloc) SimulatingConsoleFrontend(agent,
      lpc::AddressXform(delta));
}
