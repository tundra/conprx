//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Console-related definitions for windows.

#pragma warning(push, 0)
#include <windows.h>
#pragma warning(pop)

#define dword_t DWORD
#define word_t WORD
#define win_size_t SIZE_T
#define module_t HMODULE
#define bool_t BOOL

#define ansi_c_char_t CHAR
#define ansi_c_str_t LPSTR

#define wide_c_char_t WCHAR
#define wide_c_str_t LPWSTR

#define c_str_t LPCTSTR
#define c_char_t TCHAR

#define ssize_t SSIZE_T
#define hkey_t HKEY
