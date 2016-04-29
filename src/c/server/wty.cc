//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "server/wty.hh"

using namespace conprx;
using namespace tclib;

// A wintty that's backed by an actual console api. Using this you can have one
// implementation of a frontend (say, simulated) backed by an actual console, or
// alternatively one console intercepted and backed by another genuine one. It
// makes sense is what I'm saying.
class AdaptedWinTty : public WinTty {
public:
  AdaptedWinTty(ConsoleFrontend *frontend, ConsolePlatform *platform)
    : frontend_(frontend)
    , platform_(platform) { }
  virtual void default_destroy() { tclib::default_delete_concrete(this); }
  virtual response_t<bool_t> get_screen_buffer_info(bool is_error,
      ScreenBufferInfo *info_out);
  virtual response_t<uint32_t> read(tclib::Blob buffer, bool is_unicode,
      ReadConsoleControl *input_control);
  virtual response_t<uint32_t> write(tclib::Blob blob, bool is_unicode, bool is_error);

private:
  ConsoleFrontend *frontend() { return frontend_; }
  ConsoleFrontend *frontend_;
  ConsolePlatform *platform() { return platform_; }
  ConsolePlatform *platform_;
};

response_t<bool_t> AdaptedWinTty::get_screen_buffer_info(bool is_error,
    ScreenBufferInfo *info_out) {
  handle_t handle = platform()->get_std_handle(is_error ? kStdErrorHandle : kStdOutputHandle);
  if (frontend()->get_console_screen_buffer_info_ex(handle, info_out->raw())) {
    return response_t<bool_t>::yes();
  } else {
    return response_t<bool_t>::error(frontend()->get_last_error().to_nt());
  }
}

response_t<uint32_t> AdaptedWinTty::read(tclib::Blob buffer, bool is_unicode,
    ReadConsoleControl *input_control) {
  handle_t handle = platform()->get_std_handle(kStdInputHandle);
  dword_t chars_read = 0;
  bool read_succeeded = false;
  if (is_unicode) {
    read_succeeded = frontend()->read_console_w(handle, buffer.start(),
        static_cast<dword_t>(buffer.size() / sizeof(wide_char_t)), &chars_read,
        input_control->raw());
  } else {
    read_succeeded = frontend()->read_console_a(handle, buffer.start(),
        static_cast<dword_t>(buffer.size()), &chars_read,
        input_control->raw());
  }
  return read_succeeded
      ? response_t<uint32_t>::of(chars_read)
      : response_t<uint32_t>::error(frontend()->get_last_error().to_nt());
}

response_t<uint32_t> AdaptedWinTty::write(tclib::Blob blob, bool is_unicode,
    bool is_error) {
  handle_t handle = platform()->get_std_handle(is_error ? kStdErrorHandle : kStdOutputHandle);
  dword_t chars_written = 0;
  bool write_succeeded = false;
  if (is_unicode) {
    write_succeeded = frontend()->write_console_w(handle, blob.start(),
        static_cast<dword_t>(blob.size() / sizeof(wide_char_t)),
        &chars_written, NULL);
  } else {
    write_succeeded = frontend()->write_console_a(handle, blob.start(),
        static_cast<dword_t>(blob.size()), &chars_written, NULL);
  }
  return write_succeeded
      ? response_t<uint32_t>::of(chars_written)
      : response_t<uint32_t>::error(frontend()->get_last_error().to_nt());
}

pass_def_ref_t<WinTty> WinTty::new_adapted(ConsoleFrontend *frontend,
    ConsolePlatform *platform) {
  return new (kDefaultAlloc) AdaptedWinTty(frontend, platform);
}
