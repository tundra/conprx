//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// The console proxy host utility that can launch an executable in such a way
/// that the proxy is injected.

#ifndef _CONPRX_SERVER_WTY
#define _CONPRX_SERVER_WTY

#include "agent/conapi-types.hh"
#include "agent/confront.hh"
#include "share/protocol.hh"
#include "utils/alloc.hh"
#include "utils/blob.hh"
#include "utils/types.hh"

namespace conprx {

// A windows-compatible terminal i/o abstraction that can be used as the backend
// for console functions.
class WinTty : public tclib::DefaultDestructable {
public:
  virtual ~WinTty() { }

  // Write information about this wty into the given screen buffer info.
  virtual response_t<bool_t> get_screen_buffer_info(bool is_error,
      ConsoleScreenBufferInfo *info_out) = 0;

  // Read up to to the buffer's capacity from the wty's input.
  virtual response_t<uint32_t> read(tclib::Blob buffer, bool is_unicode,
      ReadConsoleControl *input_control) = 0;

  // Write up to the buffer's capacity to the wty's output, either standard or
  // error.
  virtual response_t<uint32_t> write(tclib::Blob blob, bool is_unicode, bool is_error) = 0;

  // Returns a new wty backed by the given frontend and platform. Since
  // frontends can be backed by wtys be sure not to back a wty by a frontend
  // it itself backs.
  static tclib::pass_def_ref_t<WinTty> new_adapted(ConsoleFrontend *frontend,
      ConsolePlatform *platform);
};

} // conprx

#endif // _CONPRX_SERVER_WTY
