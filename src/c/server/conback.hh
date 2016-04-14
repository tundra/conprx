//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _CONPRX_SERVER_CONBACK
#define _CONPRX_SERVER_CONBACK

#include "rpc.hh"
#include "server/handman.hh"
#include "share/protocol.hh"
#include "sync/pipe.hh"
#include "sync/process.hh"
#include "sync/thread.hh"
#include "utils/blob.hh"
#include "utils/fatbool.hh"
#include "utils/string.hh"

namespace conprx {

using plankton::Variant;
using plankton::Factory;
using plankton::Arena;

class Launcher;

// A console window provides the information the console api needs about the
// window it's running in.
class ConsoleWindow {
public:
  virtual ~ConsoleWindow() { }
  virtual coord_t size() = 0;
  virtual coord_t cursor_position() = 0;
  virtual word_t attributes() = 0;
  virtual small_rect_t position() = 0;
  virtual coord_t maximum_window_size() = 0;
};

// Virtual type, implementations of which can be used as the implementation of
// a console.
class ConsoleBackend {
public:
  virtual ~ConsoleBackend() { }

  // Debug/test call.
  virtual response_t<int64_t> poke(int64_t value) = 0;

  // Return either the input or output code page.
  virtual response_t<uint32_t> get_console_cp(bool is_output) = 0;

  // Set either the input or output code page.
  virtual response_t<bool_t> set_console_cp(uint32_t value, bool is_output) = 0;

  // Fill the given buffer with the title.
  virtual response_t<uint32_t> get_console_title(tclib::Blob buffer, bool is_unicode,
      size_t *bytes_written_out) = 0;

  // Set the title to the contents of the given buffer.
  virtual response_t<bool_t> set_console_title(tclib::Blob title, bool is_unicode) = 0;

  // Returns the current mode of the buffer with the given handle.
  virtual response_t<uint32_t> get_console_mode(Handle handle) = 0;

  // Sets the mode of the buffer with the given handle.
  virtual response_t<bool_t> set_console_mode(Handle handle, uint32_t mode) = 0;

  virtual response_t<uint32_t> write_console(Handle output, tclib::Blob data,
      bool is_unicode) = 0;

  // Fill in the given output parameter with information about the buffer with
  // the given handle.
  virtual response_t<bool_t> get_console_screen_buffer_info(Handle buffer,
      console_screen_buffer_infoex_t *info_out) = 0;
};

// A complete implementation of a console backend.
class BasicConsoleBackend : public ConsoleBackend {
public:
  BasicConsoleBackend();
  virtual ~BasicConsoleBackend();
  virtual response_t<int64_t> poke(int64_t value);
  virtual response_t<uint32_t> get_console_cp(bool is_output);
  virtual response_t<bool_t> set_console_cp(uint32_t value, bool is_output);
  virtual response_t<uint32_t> get_console_title(tclib::Blob buffer,
      bool is_unicode, size_t *bytes_written_out);
  virtual response_t<bool_t> set_console_title(tclib::Blob title,
      bool is_unicode);
  virtual response_t<uint32_t> get_console_mode(Handle handle);
  virtual response_t<bool_t> set_console_mode(Handle handle, uint32_t mode);
  virtual response_t<bool_t> get_console_screen_buffer_info(Handle buffer,
      console_screen_buffer_infoex_t *info_out);
  virtual response_t<uint32_t> write_console(Handle output, tclib::Blob data,
      bool is_unicode);

  // Sets the console window backing this backend. If you don't set one a
  // dummy one will be used.
  void set_window(ConsoleWindow *window) { window_ = window; }

  // Returns the value of the last poke that was sent.
  int64_t last_poke() { return last_poke_; }

  // The current title encoded as utf-8.
  ucs16_t title() { return title_; }

  // Converts a blob that may or may not be unicode to a ucs16-string.
  static ucs16_t blob_to_ucs16(tclib::Blob blob, bool is_unicode);

  // Returns the size of an individual character under unicode/non-unicode.
  static size_t get_char_size(bool is_unicode);

private:
  // Get-title for ansi strings.
  response_t<uint32_t> get_console_title_ansi(tclib::Blob buffer,
      size_t *bytes_written_out);

  // Get-title for wide strings.
  response_t<uint32_t> get_console_title_wide(tclib::Blob buffer,
      size_t *bytes_written_out);

  ConsoleWindow *window() { return window_; }
  int64_t last_poke_;
  uint32_t input_codepage_;
  uint32_t output_codepage_;
  ucs16_t title_;
  // TODO: for now all handles have the same mode value. There needs to be a
  //   more nuanced way to set those.
  uint32_t mode_;
  ConsoleWindow *window_;
};

// The service the driver will call back to when it wants to access the manager.
class ConsoleBackendService : public plankton::rpc::Service {
public:
  ConsoleBackendService();
  virtual ~ConsoleBackendService() { }

  // Returns true once the agent has reported that it's ready.
  bool agent_is_ready() { return agent_is_ready_; }

  // Returns true once the agent has reported that it's done.
  bool agent_is_done() { return agent_is_done_; }

  void set_backend(ConsoleBackend *backend) { backend_ = backend; }

  // Returns the type registry to use for this backend.
  plankton::TypeRegistry *registry() { return &registry_; }

private:
  // Handles logs entries logged by the agent.
  void on_log(plankton::rpc::RequestData*, ResponseCallback);

  // Called when the agent has completed its setup.
  void on_is_ready(plankton::rpc::RequestData*, ResponseCallback);
  void on_is_done(plankton::rpc::RequestData*, ResponseCallback);

  // For testing and debugging -- a call that doesn't do anything but is just
  // passed through to the implementation.
  void on_poke(plankton::rpc::RequestData*, ResponseCallback);

  // Returns a blob corresponding to the given blob variant. If the variant is
  // not a plankton blob the empty blob will be returned.
  static tclib::Blob to_blob(Variant value);

#define __GEN_HANDLER__(Name, name, DLL, API)                                  \
  void on_##name(plankton::rpc::RequestData*, ResponseCallback);
  FOR_EACH_LPC_TO_INTERCEPT(__GEN_HANDLER__)
#undef __GEN_HANDLER__

  // The fallback to call on unknown messages.
  void message_not_understood(plankton::rpc::RequestData*, ResponseCallback);

  ConsoleBackend *backend_;
  ConsoleBackend *backend() { return backend_; }

  plankton::TypeRegistry registry_;

  HandleManager *handles() { return &handles_; }
  HandleManager handles_;

  bool agent_is_ready_;
  bool agent_is_done_;
};

} // namespace conprx

#endif // _CONPRX_SERVER_CONBACK
