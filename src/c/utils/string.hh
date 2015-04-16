//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _STRING
#define _STRING

#include "c/stdc.h"
#include "types.hh"

namespace conprx {

// Various string utilities.
class String {
public:
  // Given a wide string with a length, converts it to UTF8. The result is
  // stored in the utf8_out parameter and the number of bytes of the utf8 string
  // is returned. The result is allocated fresh using new[] for each call.
  static size_t utf16_to_utf8(wide_cstr_t wide_str, size_t wide_length,
      char **utf8_out);

  // Returns the index of the first \0 in the given string.
  static size_t wstrlen(wide_cstr_t str);
};

} // namespace conprx

#endif // _STRING
