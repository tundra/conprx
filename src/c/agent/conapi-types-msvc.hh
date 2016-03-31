//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// MSVC-specific type declarations used by the console api.

#include "utils/types.hh"

typedef CONSOLE_CURSOR_INFO console_cursor_info_t;
typedef CONSOLE_READCONSOLE_CONTROL console_readconsole_control_t;
typedef COORD coord_t;
typedef SMALL_RECT small_rect_t;
typedef CONSOLE_SCREEN_BUFFER_INFO console_screen_buffer_info_t;
typedef CHAR_INFO char_info_t;
typedef COLORREF colorref_t;

#define MSVC_STDCALL __stdcall
