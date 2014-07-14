//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Type declarations used by the console api.

#ifndef _CONAPI_TYPES
#define _CONAPI_TYPES

#include "stdc.h"

// Include custom headers for each toolchain.
#ifdef IS_MSVC
#include "conapi-types-msvc.hh"
#else // !IS_MSVC
#include "conapi-types-posix.hh"
#endif

// This is an internal undocumented windows type. It may not be correct in all
// cases -- it's been pieced together from multiple sources from the web -- but
// the api_number value which we're interested in seems to generally live where
// this struct puts it.
struct lpc_message_t {
  ushort_t unused0;
  ushort_t unused1;
  ushort_t unused2;
  ushort_t unused3;
  ulong_t unused4;
  ulong_t unused5;
  ulong_t unused6;
  ulong_t unused7;
  void *unused8;
  ulong_t api_number;
};

#endif // _CONAPI_TYPES
