//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _TYPES
#define _TYPES

#include "c/stdc.h"

// Include the platform-specific type declarations.
#ifdef IS_MSVC
#include "types-msvc.hh"
#else
#include "types-posix.hh"
#endif

#endif // _TYPES
