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

  // Returns the size in bytes of characters in ansi/unicode mode.
  static inline uint32_t char_size(bool is_unicode) { return static_cast<uint32_t>(is_unicode ? sizeof(wide_char_t) : sizeof(ansi_char_t)); }
};

// Functions related to the default ms-dos (aka IBM PC, aka CP437, aka OEM-US,
// aka myriad other names) encoding.
class MsDosCodec {
public:
  // Returns the 16-bit unicode character corresponding to the given ms-dos
  // character. Note that the mapping isn't unique it's locale-dependent. So
  // this actually gives *a* mapping, *the* mapping is not well-defined.
  static uint16_t ansi_to_wide_char(uint8_t chr) { return kAnsiToWideMap[chr & 0xFF]; }

  // Returns the ms-dos character corresponding to the given 16-bit unicode.
  static uint8_t wide_to_ansi_char(uint16_t chr);

private:
  static const uint16_t kAnsiToWideMap[256];
  static const uint8_t kWideToDiacritic[96];
  static const uint8_t kWideToNonLatin[52];
  static const uint8_t kWideToGraphic[29];
};

// A ucs-16 string, that is, like utf-16 except that surrogate pairs are left
// uninterpreted.
struct ucs16_t {
  wide_str_t chars;
  size_t length;
};

// Wraps the given chars and length in a ucs16. A null terminator is optional,
// the length is the size without that.
static ucs16_t ucs16_new(wide_str_t chars, size_t length) {
  ucs16_t result = {chars, length};
  return result;
}

// Returns the empty ucs16 string which is different from a ucs16-string of
// length 0.
static ucs16_t ucs16_empty() {
  return ucs16_new(NULL, 0);
}

// Returns true iff the given value is the result of ucs16_empty.
static bool ucs16_is_empty(ucs16_t value) {
  return value.chars == NULL;
}

// Returns a copy of the given string allocated using the default allocator.
ucs16_t ucs16_default_dup(ucs16_t str);

// Deletes the given string using the default allocator.
void ucs16_default_delete(ucs16_t str);

} // namespace conprx

#endif // _STRING
