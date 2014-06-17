//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Posix-specific type declarations used by the console api.

// CONSOLE_CURSOR_INFO
struct console_cursor_info_t {
  dword_t dwSize;
  bool_t bVisible;
};

// CONSOLE_READCONSOLE_CONTROL
struct console_readconsole_control_t {

};
