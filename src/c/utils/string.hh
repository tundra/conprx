//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _STRING
#define _STRING

#include "c/stdc.h"
#include "types.hh"

#include "utils/blob.hh"

namespace conprx {

// Various string utilities.
class StringUtils {
public:
  // Given a wide string with a length, converts it to UTF8. The result is
  // stored in the utf8_out parameter and the number of bytes of the utf8 string
  // is returned. The result is allocated fresh using new[] for each call.
  static size_t utf16_to_utf8(wide_cstr_t wide_str, size_t wide_length,
      char **utf8_out);

  // Returns the index of the first \0 in the given string.
  static size_t wstrlen(wide_cstr_t str);

  // Returns a blob that extends from the beginning of the given string to the
  // end, optionally including the null terminator.
  static tclib::Blob as_blob(ansi_cstr_t str, bool include_null=false);

  // Returns a blob that extends from the beginning of the given string to the
  // end, optionally including the null terminator.
  static tclib::Blob as_blob(wide_cstr_t str, bool include_null=false);
};

} // namespace conprx

#endif // _STRING
