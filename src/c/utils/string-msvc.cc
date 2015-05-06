//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

size_t String::utf16_to_utf8(wide_cstr_t wide_str, size_t wide_length,
    char **utf8_out) {
  size_t utf8_size = WideCharToMultiByte(CP_UTF8, 0, wide_str,
      static_cast<int>(wide_length), NULL, 0, NULL, NULL);
  char *utf8 = new char[utf8_size];
  *utf8_out = utf8;
  WideCharToMultiByte(CP_UTF8, 0, wide_str, static_cast<int>(wide_length), utf8,
      static_cast<int>(utf8_size), NULL, NULL);
  return utf8_size;
}
