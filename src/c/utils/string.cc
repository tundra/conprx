//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "string.hh"

using namespace conprx;

size_t String::wstrlen(wide_cstr_t str) {
  wide_cstr_t p = str;
  while (*p)
    p++;
  return p - str;
}

#ifdef IS_MSVC
#include "string-msvc.cc"
#else
#include "string-posix.cc"
#endif
