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

  // TODO: wrap all the size/position etc. functions into a single call that
  //   gets all the info at once.

  // The size of this terminal window.
  virtual coord_t size() = 0;

  // The position within the terminal window of the cursor.
  virtual coord_t cursor_position() = 0;

  // ?
  virtual word_t attributes() = 0;

  // The position on the screen of the terminal window.
  virtual small_rect_t position() = 0;

  // As large as the system allows this window to become.
  virtual coord_t maximum_window_size() = 0;

  // Read up to to the buffer's capacity from the wty's input.
  virtual response_t<uint32_t> read(tclib::Blob buffer, bool is_unicode) = 0;

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
