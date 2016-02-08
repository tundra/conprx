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

#define GET_SIG_RET(RET, PARAMS, ARGS) RET
#define GET_SIG_PARAMS(RET, PARAMS, ARGS) PARAMS
#define GET_SIG_ARGS(RET, PARAMS, ARGS) ARGS

#define sigVoidToBool(F) F(bool_t, (), ())
#define sigVoidToUInt(F) F(uint_t, (), ())
#define sigVoidToHWnd(F) F(hwnd_t, (), ())
#define sigWideStrToDWord(F) F(dword_t, (wide_str_t str, dword_t n), (str, n))
#define sigWideCStrToBool(F) F(bool_t, (wide_cstr_t str), (str))
#define sigStrToDWord(F) F(dword_t, (str_t str, dword_t n), (str, n))
#define sigAnsiCStrToBool(F) F(bool_t, (ansi_cstr_t str), (str))
#define sigDWordToHandle(F) F(handle_t, (dword_t handle), (handle))

#define sigGetConsoleCursorInfo(F) F(bool_t,                                   \
    (handle_t console_output, console_cursor_info_t *console_cursor_info),     \
    (console_output, console_cursor_info))
#define sigGetConsoleMode(F) F(bool_t,                                         \
    (handle_t console_handle, dword_t *mode),                                  \
    (console_handle, mode))
#define sigGetConsoleScreenBufferInfo(F) F(bool_t,                             \
    (handle_t console_output, console_screen_buffer_info_t *buffer_info),      \
    (console_output, buffer_info))
#define sigReadConsoleA(F) F(bool_t,                                           \
    (handle_t console_input, void *buffer, dword_t chars_to_read,              \
     dword_t *chars_read, console_readconsole_control_t *input_control),       \
    (console_input, buffer, chars_to_read, chars_read, input_control))
#define sigReadConsoleW(F) F(bool_t,                                           \
    (handle_t console_input, void *buffer, dword_t chars_to_read,              \
     dword_t *chars_read, console_readconsole_control_t *input_control),       \
    (console_input, buffer, chars_to_read, chars_read, input_control))
#define sigReadConsoleOutputA(F) F(bool_t,                                     \
    (handle_t console_output, char_info_t *buffer, coord_t buffer_size,        \
     coord_t buffer_coord, small_rect_t *read_region),                         \
    (console_output, buffer, buffer_size, buffer_coord, read_region))
#define sigReadConsoleOutputW(F) F(bool_t,                                     \
    (handle_t console_output, char_info_t *buffer, coord_t buffer_size,        \
     coord_t buffer_coord, small_rect_t *read_region),                         \
    (console_output, buffer, buffer_size, buffer_coord, read_region))
#define sigSetConsoleCursorInfo(F) F(bool_t,                                   \
    (handle_t console_output, const console_cursor_info_t *info),              \
    (console_output, info))
#define sigSetConsoleCursorPosition(F) F(bool_t,                               \
    (handle_t console_output, coord_t cursor_position),                        \
    (console_output, cursor_position))
#define sigSetConsoleMode(F) F(bool_t,                                         \
    (handle_t console_handle, dword_t mode),                                   \
    (console_handle, mode))
#define sigWriteConsoleA(F) F(bool_t,                                          \
    (handle_t console_output, const void *buffer, dword_t chars_to_write,      \
     dword_t *chars_written, void *reserved),                                  \
    (console_output, buffer, chars_to_write, chars_written, reserved))
#define sigWriteConsoleW(F) F(bool_t,                                          \
    (handle_t console_output, const void *buffer, dword_t chars_to_write,      \
     dword_t *chars_written, void *reserved),                                  \
    (console_output, buffer, chars_to_write, chars_written, reserved))
#define sigWriteConsoleOutputA(F) F(bool_t,                                    \
    (handle_t console_output, const char_info_t *buffer, coord_t buffer_size,  \
     coord_t buffer_coord, small_rect_t *write_region),                        \
    (console_output, buffer, buffer_size, buffer_coord, write_region))
#define sigWriteConsoleOutputW(F) F(bool_t,                                    \
    (handle_t console_output, const char_info_t *buffer, coord_t buffer_size,  \
     coord_t buffer_coord, small_rect_t *write_region),                        \
    (console_output, buffer, buffer_size, buffer_coord, write_region))

// Table of console api functions that need some form of treatment. To make it
// easier to read the function signatures are defined in separate macros above.
// The flags are,
//
//   - _Or_, create a stub for calling the original function.
//
//  CamelName                   underscore_name                 (Or, Dr) sigSignature
#define FOR_EACH_CONAPI_FUNCTION(F)                                                                    \
  F(GetStdHandle,               get_std_handle,                 (X,  X), sigDWordToHandle)             \

#define FOR_EACH_FULL_CONAPI_FUNCTION(F)                                                               \
  FOR_EACH_CONAPI_FUNCTION(F)                                                                          \
  F(AllocConsole,               alloc_console,                  (X,  _), sigVoidToBool)                \
  F(FreeConsole,                free_console,                   (X,  _), sigVoidToBool)                \
  F(GetConsoleCP,               get_console_cp,                 (X,  _), sigVoidToUInt)                \
  F(GetConsoleCursorInfo,       get_console_cursor_info,        (X,  _), sigGetConsoleCursorInfo)      \
  F(GetConsoleMode,             get_console_mode,               (X,  _), sigGetConsoleMode)            \
  F(GetConsoleOutputCP,         get_console_output_cp,          (X,  _), sigVoidToUInt)                \
  F(GetConsoleScreenBufferInfo, get_console_screen_buffer_info, (X,  _), sigGetConsoleScreenBufferInfo)\
  F(GetConsoleTitleA,           get_console_title_a,            (X,  _), sigStrToDWord)                \
  F(GetConsoleTitleW,           get_console_title_w,            (X,  _), sigWideStrToDWord)            \
  F(GetConsoleWindow,           get_console_window,             (X,  _), sigVoidToHWnd)                \
  F(ReadConsoleA,               read_console_a,                 (X,  _), sigReadConsoleA)              \
  F(ReadConsoleW,               read_console_w,                 (X,  _), sigReadConsoleW)              \
  F(ReadConsoleOutputA,         read_console_output_a,          (X,  _), sigReadConsoleOutputA)        \
  F(ReadConsoleOutputW,         read_console_output_w,          (X,  _), sigReadConsoleOutputW)        \
  F(SetConsoleCursorInfo,       set_console_cursor_info,        (X,  _), sigSetConsoleCursorInfo)      \
  F(SetConsoleCursorPosition,   set_console_cursor_position,    (X,  _), sigSetConsoleCursorPosition)  \
  F(SetConsoleMode,             set_console_mode,               (X,  _), sigSetConsoleMode)            \
  F(SetConsoleTitleA,           set_console_title_a,            (X,  _), sigAnsiCStrToBool)            \
  F(SetConsoleTitleW,           set_console_title_w,            (X,  _), sigWideCStrToBool)            \
  F(WriteConsoleA,              write_console_a,                (X,  _), sigWriteConsoleA)             \
  F(WriteConsoleW,              write_console_w,                (X,  _), sigWriteConsoleW)             \
  F(WriteConsoleOutputA,        write_console_output_a,         (X,  _), sigWriteConsoleOutputA)       \
  F(WriteConsoleOutputW,        write_console_output_w,         (X,  _), sigWriteConsoleOutputW)

#define mfOr(OR, DR) OR
#define mfDr(OR, DR) DR

// A container that holds the various definitions used by the other console
// types.
class Console {
public:
  virtual ~Console();

  // The types of the naked console functions.
#define __DECLARE_CONAPI_FUNCTION__(Name, name, FLAGS, SIG)                    \
  typedef SIG(GET_SIG_RET) (WINAPI *name##_t)SIG(GET_SIG_PARAMS);
  FOR_EACH_FULL_CONAPI_FUNCTION(__DECLARE_CONAPI_FUNCTION__)
#undef __DECLARE_CONAPI_FUNCTION__

  enum key_t {
#define __DECLARE_CONAPI_KEY__(Name, name, FLAGS, SIG)                         \
    name##_key,
    FOR_EACH_FULL_CONAPI_FUNCTION(__DECLARE_CONAPI_KEY__)
#undef __DECLARE_CONAPI_KEY__
    kFunctionCount
  };

#define __DECLARE_CONAPI_METHOD__(Name, name, FLAGS, SIG)                      \
  virtual SIG(GET_SIG_RET) name SIG(GET_SIG_PARAMS) = 0;
  FOR_EACH_CONAPI_FUNCTION(__DECLARE_CONAPI_METHOD__)
#undef __DECLARE_CONAPI_METHOD__

  // A description of a console function.
  struct FunctionInfo {
    cstr_t name;
    int key;
  };

  static Console *native();

  // Returns a list containing descriptions of all the console functions.
  static Vector<FunctionInfo> functions();

};

// A console implementation that logs all interaction before forwarding it to
// a given delegate.
class LoggingConsole : public Console {
public:
  LoggingConsole(Console *delegate) : delegate_(delegate) { }

  void set_delegate(Console *delegate) { delegate_ = delegate; }

  // TODO: replace with a proper delegate when the console interface is the full
  //   set of methods.
  LoggingConsole &delegate() { return *static_cast<LoggingConsole*>(NULL); }

  // Sends the given complete log message.
  void emit_message(plankton::Variant message);

#define __DECLARE_CONAPI_METHOD__(Name, name, FLAGS, SIG)                      \
  virtual SIG(GET_SIG_RET) name SIG(GET_SIG_PARAMS);
  FOR_EACH_FULL_CONAPI_FUNCTION(__DECLARE_CONAPI_METHOD__)
#undef __DECLARE_CONAPI_METHOD__

private:
  Console *delegate_;
};

} // conprx

#endif // _CONAPI
