//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Console-related definitions for posix. The posix console api is just a fake
/// for testing so the concrete choices here aren't that important, they just
/// have to play the same role as the windows definitions but don't have to be
/// compatible otherwise.

typedef void *handle_t;
typedef void *module_t;
typedef long dword_t;
typedef bool bool_t;
typedef int16_t short_t;
typedef uint16_t ushort_t;
typedef uint16_t word_t;
typedef int ntstatus_t;
typedef uint32_t ulong_t;
typedef uint32_t uint_t;

// Because of the magic way windows handles strings we'll use this alias for
// them even on posix.
typedef char *ansi_str_t;
typedef const char *ansi_cstr_t;
typedef uint16_t *wide_str_t;
typedef const uint16_t *wide_cstr_t;
typedef ansi_str_t str_t;
typedef ansi_cstr_t cstr_t;

#define TEXT(STR) (STR)

// Windows-specific directives that we can just ignore when building elsewhere.
#define APIENTRY
#define WINAPI
#define NTAPI
