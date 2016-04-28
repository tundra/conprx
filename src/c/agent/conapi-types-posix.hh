//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Posix-specific type declarations used by the console api.

#define MSVC_STDCALL

// CONSOLE_CURSOR_INFO
struct console_cursor_info_t {
  dword_t dwSize;
  bool_t bVisible;
};

// CONSOLE_READCONSOLE_CONTROL
struct console_readconsole_control_t {
  ulong_t nLength;
  ulong_t nInitialChars;
  ulong_t dwCtrlWakeupMask;
  ulong_t dwControlKeyState;
};

// COORD
struct coord_t {
  short_t X;
  short_t Y;
};

// SMALL_RECT
struct small_rect_t {
  short_t Left;
  short_t Top;
  short_t Right;
  short_t Bottom;
};

// CONSOLE_SCREEN_BUFFER_INFO
struct console_screen_buffer_info_t {
  coord_t dwSize;
  coord_t dwCursorPosition;
  word_t wAttributes;
  small_rect_t srWindow;
  coord_t dwMaximumWindowSize;
};

// COLORREF
typedef dword_t colorref_t;

// CONSOLE_SCREEN_BUFFER_INFOEX
struct console_screen_buffer_infoex_t {
  uint32_t cbSize;
  coord_t dwSize;
  coord_t dwCursorPosition;
  word_t wAttributes;
  small_rect_t srWindow;
  coord_t dwMaximumWindowSize;
  word_t wPopupAttributes;
  bool_t bFullscreenSupported;
  colorref_t ColorTable[16];
};

// CHAR_INFO
struct char_info_t {
  union {
    wide_char_t UnicodeChar;
    ansi_char_t AsciiChar;
  } Char;
  word_t Attributes;
};
