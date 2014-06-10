//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _TYPES
#define _TYPES

#include "stdc.h"

// Include the platform-specific type declarations.
#ifdef IS_MSVC
#include "types-msvc.hh"
#else
#include "types-posix.hh"
#endif

// Shorthand for bytes.
typedef unsigned char byte_t;

// Byte-size memory address (so addition increments by one byte at a time).
typedef byte_t *address_t;

// Integer datatype large enough to allow address arithmetic.
typedef size_t address_arith_t;

#endif // _TYPES
