//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Declarations of the console api.

#ifndef _CONAPI
#define _CONAPI

#include "agent/binpatch.hh"
#include "conapi-types.hh"
#include "utils/types.hh"
#include "utils/vector.hh"
#include "plankton-inl.hh"

namespace conprx {

#define FOR_EACH_CONAPI_FUNCTION(F)                                            \
  F(AllocConsole, alloc_console, bool_t,                                       \
      (),                                                                      \
      ())                                                                      \
  F(FreeConsole, free_console, bool_t,                                         \
      (),                                                                      \
      ())                                                                      \
  F(GetConsoleCP, get_console_cp, uint_t,                                      \
      (),                                                                      \
      ())                                                                      \
  F(GetConsoleCursorInfo, get_console_cursor_info, bool_t,                     \
      (handle_t console_output, console_cursor_info_t *console_cursor_info),   \
      (console_output, console_cursor_info))                                   \
  F(GetConsoleMode, get_console_mode, bool_t,                                  \
      (handle_t console_handle, dword_t *mode),                                \
      (console_handle, mode))                                                  \
  F(GetConsoleOutputCP, get_console_output_cp, uint_t,                         \
      (),                                                                      \
      ())                                                                      \
  F(GetConsoleScreenBufferInfo, get_console_screen_buffer_info, bool_t,        \
      (handle_t console_output,                                                \
       console_screen_buffer_info_t *console_screen_buffer_info),              \
      (console_output, console_screen_buffer_info))                            \
  F(GetConsoleTitleA, get_console_title_a, dword_t,                            \
      (ansi_str_t console_title, dword_t size),                                \
      (console_title, size))                                                   \
  F(GetConsoleTitleW, get_console_title_w, dword_t,                            \
      (wide_str_t console_title, dword_t size),                                \
      (console_title, size))                                                   \
  F(GetConsoleWindow, get_console_window, hwnd_t,                              \
      (),                                                                      \
      ())                                                                      \
  F(GetStdHandle, get_std_handle, handle_t,                                    \
      (dword_t std_handle),                                                    \
      (std_handle))                                                            \
  F(ReadConsoleA, read_console_a, bool_t,                                      \
      (handle_t console_input, void *buffer,                                   \
       dword_t number_of_chars_to_read, dword_t *number_of_chars_read,         \
       console_readconsole_control_t *input_control),                          \
      (console_input, buffer, number_of_chars_to_read,                         \
       number_of_chars_read, input_control))                                   \
  F(ReadConsoleW, read_console_w, bool_t,                                      \
      (handle_t console_input, void *buffer,                                   \
       dword_t number_of_chars_to_read, dword_t *number_of_chars_read,         \
       console_readconsole_control_t *input_control),                          \
      (console_input, buffer, number_of_chars_to_read,                         \
       number_of_chars_read, input_control))                                   \
  F(ReadConsoleOutputA, read_console_output_a, bool_t,                         \
      (handle_t console_output, char_info_t *buffer, coord_t buffer_size,      \
       coord_t buffer_coord, small_rect_t *read_region),                       \
      (console_output, buffer, buffer_size, buffer_coord, read_region))        \
  F(ReadConsoleOutputW, read_console_output_w, bool_t,                         \
      (handle_t console_output, char_info_t *buffer, coord_t buffer_size,      \
       coord_t buffer_coord, small_rect_t *read_region),                       \
      (console_output, buffer, buffer_size, buffer_coord, read_region))        \
  F(SetConsoleCursorInfo, set_console_cursor_info, bool_t,                     \
      (handle_t console_output,                                                \
       const console_cursor_info_t *console_cursor_info),                      \
      (console_output, console_cursor_info))                                   \
  F(SetConsoleCursorPosition, set_console_cursor_position, bool_t,             \
      (handle_t console_output, coord_t cursor_position),                      \
      (console_output, cursor_position))                                       \
  F(SetConsoleMode, set_console_mode, bool_t,                                  \
      (handle_t console_handle, dword_t mode),                                 \
      (console_handle, mode))                                                  \
  F(SetConsoleTitleA, set_console_title_a, bool_t,                             \
      (ansi_cstr_t console_title),                                             \
      (console_title))                                                         \
  F(SetConsoleTitleW, set_console_title_w, bool_t,                             \
      (wide_cstr_t console_title),                                             \
      (console_title))                                                         \
  F(WriteConsoleA, write_console_a, bool_t,                                    \
      (handle_t console_output, const void *buffer,                            \
       dword_t number_of_chars_to_write, dword_t *number_of_chars_written,     \
       void *reserved),                                                        \
      (console_output, buffer, number_of_chars_to_write,                       \
       number_of_chars_written, reserved))                                     \
  F(WriteConsoleW, write_console_w, bool_t,                                    \
      (handle_t console_output, const void *buffer,                            \
       dword_t number_of_chars_to_write, dword_t *number_of_chars_written,     \
       void *reserved),                                                        \
      (console_output, buffer, number_of_chars_to_write,                       \
       number_of_chars_written, reserved))                                     \
  F(WriteConsoleOutputA, write_console_output_a, bool_t,                       \
      (handle_t console_output, const char_info_t *buffer, coord_t buffer_size,\
       coord_t buffer_coord, small_rect_t *write_region),                      \
      (console_output, buffer, buffer_size, buffer_coord, write_region))       \
  F(WriteConsoleOutputW, write_console_output_w, bool_t,                       \
      (handle_t console_output, const char_info_t *buffer, coord_t buffer_size,\
       coord_t buffer_coord, small_rect_t *write_region),                      \
      (console_output, buffer, buffer_size, buffer_coord, write_region))


// A container that holds the various definitions used by the other console
// types.
class Console {
public:
  virtual ~Console();

  // The types of the naked console functions.
#define __DECLARE_CONAPI_FUNCTION__(Name, name, RET, PARAMS, ARGS)             \
  typedef RET (WINAPI *name##_t)PARAMS;
  FOR_EACH_CONAPI_FUNCTION(__DECLARE_CONAPI_FUNCTION__)
#undef __DECLARE_CONAPI_FUNCTION__

  enum key_t {
#define __DECLARE_CONAPI_KEY__(Name, name, RET, PARAMS, ARGS)                  \
    name##_key,
    FOR_EACH_CONAPI_FUNCTION(__DECLARE_CONAPI_KEY__)
#undef __DECLARE_CONAPI_KEY__
    kFunctionCount
  };

#define __DECLARE_CONAPI_METHOD__(Name, name, RET, PARAMS, ARGS)               \
  virtual RET name PARAMS = 0;
  FOR_EACH_CONAPI_FUNCTION(__DECLARE_CONAPI_METHOD__)
#undef __DECLARE_CONAPI_METHOD__

  // A description of a console function.
  struct FunctionInfo {
    cstr_t name;
    int key;
  };

  // Returns a list containing descriptions of all the console functions.
  static Vector<FunctionInfo> functions();

};

// A console implementation that logs all interaction before forwarding it to
// a given delegate.
class LoggingConsole : public Console {
public:
  LoggingConsole(Console *delegate) : delegate_(delegate) { }

  void set_delegate(Console *delegate) { delegate_ = delegate; }

  Console &delegate() { return *delegate_; }

  // Sends the given complete log message.
  void emit_message(plankton::Variant message);

#define __DECLARE_CONAPI_METHOD__(Name, name, RET, PARAMS, ARGS)               \
  virtual RET name PARAMS;
  FOR_EACH_CONAPI_FUNCTION(__DECLARE_CONAPI_METHOD__)
#undef __DECLARE_CONAPI_METHOD__

private:
  Console *delegate_;
};

} // conprx

#endif // _CONAPI
