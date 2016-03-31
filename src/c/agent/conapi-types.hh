//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Type declarations used by the console api.

#ifndef _CONAPI_TYPES
#define _CONAPI_TYPES

#include "c/stdc.h"
#include "utils/types.hh"

#define ntSuccess 0

// Include custom headers for each toolchain.
#ifdef IS_MSVC
#include "conapi-types-msvc.hh"
#else // !IS_MSVC
#include "conapi-types-posix.hh"
#endif

// Given an extended screen buffer info returns a pointer into it that makes
// up a non-extended version.
static inline console_screen_buffer_info_t *console_screen_buffer_info_from_ex(
    console_screen_buffer_infoex_t *ex) {
  return reinterpret_cast<console_screen_buffer_info_t*>(&ex->dwSize);
}

#endif // _CONAPI_TYPES
