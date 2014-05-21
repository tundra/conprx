//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Console-related definitions for posix. The posix console api is just a fake
/// for testing so the concrete choices here aren't that important, they just
/// have to play the same role as the windows definitions but don't have to be
/// compatible otherwise.

typedef void *handle_t;
typedef long dword_t;
