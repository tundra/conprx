//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Type declarations used by the console api.

#ifndef _CONAPI_TYPES
#define _CONAPI_TYPES

#include "c/stdc.h"
#include "utils/types.hh"

// Include custom headers for each toolchain.
#ifdef IS_MSVC
#include "conapi-types-msvc.hh"
#else // !IS_MSVC
#include "conapi-types-posix.hh"
#endif

// Returns a coord struct with the given ordinates.
static inline coord_t coord_new(short_t x, short_t y) {
  coord_t result;
  result.X = x;
  result.Y = y;
  return result;
}

// Returns a rect with the given fields.
static inline small_rect_t small_rect_new(short_t left, short_t top,
    short_t right, short_t bottom) {
  small_rect_t result;
  result.Left = left;
  result.Top = top;
  result.Right = right;
  result.Bottom = bottom;
  return result;
}

// Given an extended screen buffer info returns a pointer into it that makes
// up a non-extended version.
static inline console_screen_buffer_info_t *console_screen_buffer_info_from_ex(
    console_screen_buffer_infoex_t *ex) {
  return reinterpret_cast<console_screen_buffer_info_t*>(&ex->dwSize);
}

#endif // _CONAPI_TYPES
