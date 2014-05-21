//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Declarations of the console api.

#include "stdc.h"

#ifdef IS_MSVC
#include "conapi-msvc.hh"
#else
#include "conapi-posix.hh"
#endif

// A virtual console api.
class Console {
public:
  virtual bool write_console_a(handle_t console_output, const void *buffer,
    dword_t number_of_chars_to_write, dword_t *number_of_chars_written,
    void *reserved) = 0;

  // Returns the native implementation of the windows console api. Only defined
  // on windows.
  static Console &wincon();
};
