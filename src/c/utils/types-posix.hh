//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Console-related definitions for posix. The posix console api is just a fake
/// for testing so the concrete choices here aren't that important, they just
/// have to play the same role as the windows definitions but don't have to be
/// compatible otherwise.

typedef void *handle_t;
typedef void *module_t;
typedef long dword_t;

// Because of the magic way windows handles strings we'll use this alias for
// them even on posix.
typedef const char *ansi_c_str_t;
typedef const wchar_t *wide_c_str_t;
typedef ansi_c_str_t c_str_t;

#define TEXT(STR) (STR)

// Windows-specific directives that we can just ignore when building elsewhere.
#define APIENTRY
#define WINAPI
