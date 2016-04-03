//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "string.hh"

using namespace conprx;

size_t StringUtils::wstrlen(wide_cstr_t str) {
  wide_cstr_t p = str;
  while (*p)
    p++;
  return p - str;
}

tclib::Blob StringUtils::as_blob(ansi_cstr_t str, bool include_null) {
  return tclib::Blob(str, strlen(str) + (include_null ? 1 : 0));
}

tclib::Blob StringUtils::as_blob(wide_cstr_t str, bool include_null) {
  size_t char_count = wstrlen(str) + (include_null ? 1 : 0);
  return tclib::Blob(str, char_count * sizeof(wide_char_t));
}

#ifdef IS_MSVC
#include "string-msvc.cc"
#else
#include "string-posix.cc"
#endif
