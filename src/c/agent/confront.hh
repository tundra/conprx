//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// A console frontend is a virtualized windows console api. It implements the
/// same functions, as virtual methods, and one implementation just calls
/// through to the actual windows console api. But using a frontend allows the
/// implementation to be swapped out for testing and such that we can run the
/// code on other platforms than windows for development convenience and, again,
/// testing.

#ifndef _AGENT_CONSOLE_FRONTEND_HH
#define _AGENT_CONSOLE_FRONTEND_HH

#include "agent/binpatch.hh"
#include "conapi-types.hh"
#include "plankton-inl.hh"
#include "sync/pipe.hh"
#include "utils/types.hh"
#include "utils/vector.hh"
#include "share/protocol.hh"

namespace conprx {

class ConsoleAdaptor;

#define GET_SIG_RET(RET, PARAMS, ARGS) RET
#define GET_SIG_PARAMS(RET, PARAMS, ARGS) PARAMS
#define GET_SIG_ARGS(RET, PARAMS, ARGS) ARGS

#define sigVoidToBool(F) F(bool_t, (), ())
#define sigVoidToUInt(F) F(uint32_t, (), ())
#define sigUIntToBool(F) F(bool_t, (uint32_t value), (value))
#define sigVoidToHWnd(F) F(hwnd_t, (), ())
#define sigWideStrToDWord(F) F(dword_t, (wide_str_t str, uint32_t n), (str, n))
#define sigWideCStrToBool(F) F(bool_t, (wide_cstr_t str), (str))
#define sigStrToDWord(F) F(dword_t, (str_t str, dword_t n), (str, n))
#define sigAnsiCStrToBool(F) F(bool_t, (ansi_cstr_t str), (str))
#define sigDWordToHandle(F) F(handle_t, (dword_t handle), (handle))

#define sigGetConsoleCursorInfo(F) F(bool_t,                                   \
    (handle_t console_output, console_cursor_info_t *console_cursor_info),     \
    (console_output, console_cursor_info))
#define sigGetConsoleMode(F) F(bool_t,                                         \
    (handle_t handle, dword_t *mode_out),                                      \
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

#define psigDWordToHandle(F) F(_, (int64_t n_std_handle), (n_std_handle))
#define psigWriteConsoleA(F) F(_,                                              \
    (Handle console_output, const char *string, int64_t chars_to_write),       \
    (console_output, string, chars_to_write))
#define psigDWordToStr(F) F(_, (int64_t bufsize), (bufsize))
#define psigAnsiCStrToVariant(F) F(_, (const char* string), (string))
#define psigVoidToDWord(F) F(_, (), ())
#define psigUIntToBool(F) F(_, (uint32_t value), (value))
#define psigGetConsoleMode(F) F(_, (Handle handle), (handle))
#define psigSetConsoleMode(F) F(_,                                             \
    (Handle console_handle, uint32_t mode),                                    \
    (console_handle, mode))

// Table of console api functions that need some form of treatment. To make it
// easier to read the function signatures are defined in separate macros above.
// The flags are,
//
//   - _Or_, create a stub for calling the original function.
//
//  CamelName                   underscore_name                 (Or) sigSignature                   psigProxySignature
#define FOR_EACH_CONAPI_FUNCTION(F)                                                                                        \
  F(GetStdHandle,               get_std_handle,                 (X), sigDWordToHandle,              psigDWordToHandle)     \
  F(WriteConsoleA,              write_console_a,                (X), sigWriteConsoleA,              psigWriteConsoleA)     \
  F(GetConsoleTitleA,           get_console_title_a,            (X), sigStrToDWord,                 psigDWordToStr)        \
  F(SetConsoleTitleA,           set_console_title_a,            (X), sigAnsiCStrToBool,             psigAnsiCStrToVariant) \
  F(GetConsoleCP,               get_console_cp,                 (X), sigVoidToUInt,                 psigVoidToDWord)       \
  F(SetConsoleCP,               set_console_cp,                 (X), sigUIntToBool,                 psigUIntToBool)        \
  F(GetConsoleOutputCP,         get_console_output_cp,          (X), sigVoidToUInt,                 psigVoidToDWord)       \
  F(SetConsoleOutputCP,         set_console_output_cp,          (X), sigUIntToBool,                 psigUIntToBool)        \
  F(GetConsoleMode,             get_console_mode,               (X), sigGetConsoleMode,             psigGetConsoleMode)    \
  F(SetConsoleMode,             set_console_mode,               (X), sigSetConsoleMode,             psigSetConsoleMode)    \


#define FOR_EACH_FULL_CONAPI_FUNCTION(F)                                                                                   \
  FOR_EACH_CONAPI_FUNCTION(F)                                                                                              \
  F(AllocConsole,               alloc_console,                  (X), sigVoidToBool,                 _)                     \
  F(FreeConsole,                free_console,                   (X), sigVoidToBool,                 _)                     \
  F(GetConsoleCursorInfo,       get_console_cursor_info,        (X), sigGetConsoleCursorInfo,       _)                     \
  F(GetConsoleOutputCP,         get_console_output_cp,          (X), sigVoidToUInt,                 _)                     \
  F(GetConsoleScreenBufferInfo, get_console_screen_buffer_info, (X), sigGetConsoleScreenBufferInfo, _)                     \
  F(GetConsoleTitleW,           get_console_title_w,            (X), sigWideStrToDWord,             _)                     \
  F(GetConsoleWindow,           get_console_window,             (X), sigVoidToHWnd,                 _)                     \
  F(ReadConsoleA,               read_console_a,                 (X), sigReadConsoleA,               _)                     \
  F(ReadConsoleW,               read_console_w,                 (X), sigReadConsoleW,               _)                     \
  F(ReadConsoleOutputA,         read_console_output_a,          (X), sigReadConsoleOutputA,         _)                     \
  F(ReadConsoleOutputW,         read_console_output_w,          (X), sigReadConsoleOutputW,         _)                     \
  F(SetConsoleCursorInfo,       set_console_cursor_info,        (X), sigSetConsoleCursorInfo,       _)                     \
  F(SetConsoleCursorPosition,   set_console_cursor_position,    (X), sigSetConsoleCursorPosition,   _)                     \
  F(SetConsoleTitleW,           set_console_title_w,            (X), sigWideCStrToBool,             _)                     \
  F(WriteConsoleW,              write_console_w,                (X), sigWriteConsoleW,              _)                     \
  F(WriteConsoleOutputA,        write_console_output_a,         (X), sigWriteConsoleOutputA,        _)                     \
  F(WriteConsoleOutputW,        write_console_output_w,         (X), sigWriteConsoleOutputW,        _)

#define mfOr(OR) OR

class ConsoleAgent;

// A container that holds the various definitions used by the other console
// types.
class ConsoleFrontend : public tclib::DefaultDestructable {
public:
  virtual ~ConsoleFrontend() { }

  // The types of the naked console functions.
#define __DECLARE_CONAPI_FUNCTION__(Name, name, FLAGS, SIG, PSIG)              \
  typedef SIG(GET_SIG_RET) (WINAPI *name##_t)SIG(GET_SIG_PARAMS);
  FOR_EACH_CONAPI_FUNCTION(__DECLARE_CONAPI_FUNCTION__)
#undef __DECLARE_CONAPI_FUNCTION__

#define __DECLARE_CONAPI_METHOD__(Name, name, FLAGS, SIG, PSIG)                \
  virtual SIG(GET_SIG_RET) name SIG(GET_SIG_PARAMS) = 0;
  FOR_EACH_CONAPI_FUNCTION(__DECLARE_CONAPI_METHOD__)
#undef __DECLARE_CONAPI_METHOD__

  virtual NtStatus get_last_error() = 0;

  virtual int64_t poke_backend(int64_t value) { return 0; }

  // Creates and returns a new instance of the native console frontend.
  static tclib::pass_def_ref_t<ConsoleFrontend> new_native();

  // Creates and returns a new dummy console frontend.
  static tclib::pass_def_ref_t<ConsoleFrontend> new_dummy();

  // Returns a new simulating console frontend that simulates interaction with
  // the given backend.
  static tclib::pass_def_ref_t<ConsoleFrontend> new_simulating(
      ConsoleAgent *agent, ssize_t delta = 0);
};

} // conprx

#endif // _AGENT_CONSOLE_FRONTEND_HH
