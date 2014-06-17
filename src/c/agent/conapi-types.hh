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

#endif // _CONAPI_TYPES
