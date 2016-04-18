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
#define sigWideStrDWordToDWord(F) F(dword_t, (wide_str_t str, dword_t n), (str, n))
#define sigWideCStrToBool(F) F(bool_t, (wide_cstr_t str), (str))
#define sigAnsiStrDwordToDWord(F) F(dword_t, (ansi_str_t str, dword_t n), (str, n))
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
#define sigGetConsoleScreenBufferInfoEx(F) F(bool_t,                           \
    (handle_t console_output, console_screen_buffer_infoex_t *buffer_info),    \
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
    (handle_t output, const void *buffer, dword_t chars_to_write,              \
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

#define psigInt64(F) F(_, (int64_t n), (n))
#define psigAnsiCStr(F) F(_, (ansi_cstr_t str), (str))
#define psigWideCStr(F) F(_, (wide_cstr_t str), (str))
#define psigVoid(F) F(_, (), ())
#define psigUInt32(F) F(_, (uint32_t value), (value))
#define psigSetConsoleMode(F) F(_,                                             \
    (Handle console_handle, uint32_t mode),                                    \
    (console_handle, mode))
#define psigHandle(F) F(_, (Handle handle), (handle))
#define psigHandleBlob(F) F(_, (Handle output, tclib::Blob data), (output, data))

// Table of console api functions that need some form of treatment. To make it
// easier to read the function signatures are defined in separate macros above.
// The flags are,
//
//   - _Pf_, is this a platform function, otherwise it's a frontend one.
//
//  CamelName                   underscore_name                      (Pf) sigSignature                     psigProxySignature
#define FOR_EACH_CONAPI_FUNCTION(F)                                                                                               \
  F(WriteConsoleA,                write_console_a,                   (_), sigWriteConsoleA,                psigHandleBlob)        \
  F(WriteConsoleW,                write_console_w,                   (_), sigWriteConsoleW,                psigHandleBlob)        \
  F(GetConsoleTitleA,             get_console_title_a,               (_), sigAnsiStrDwordToDWord,          psigInt64)             \
  F(SetConsoleTitleA,             set_console_title_a,               (_), sigAnsiCStrToBool,               psigAnsiCStr)          \
  F(GetConsoleTitleW,             get_console_title_w,               (_), sigWideStrDWordToDWord,          psigInt64)             \
  F(SetConsoleTitleW,             set_console_title_w,               (_), sigWideCStrToBool,               psigWideCStr)          \
  F(GetConsoleCP,                 get_console_cp,                    (_), sigVoidToUInt,                   psigVoid)              \
  F(SetConsoleCP,                 set_console_cp,                    (_), sigUIntToBool,                   psigUInt32)            \
  F(GetConsoleOutputCP,           get_console_output_cp,             (_), sigVoidToUInt,                   psigVoid)              \
  F(SetConsoleOutputCP,           set_console_output_cp,             (_), sigUIntToBool,                   psigUInt32)            \
  F(GetConsoleMode,               get_console_mode,                  (_), sigGetConsoleMode,               psigHandle)            \
  F(SetConsoleMode,               set_console_mode,                  (_), sigSetConsoleMode,               psigSetConsoleMode)    \
  F(GetConsoleScreenBufferInfo,   get_console_screen_buffer_info,    (_), sigGetConsoleScreenBufferInfo,   psigHandle)            \
  F(GetConsoleScreenBufferInfoEx, get_console_screen_buffer_info_ex, (_), sigGetConsoleScreenBufferInfoEx, psigHandle)            \
  F(GetStdHandle,                 get_std_handle,                    (X), sigDWordToHandle,                psigInt64)             \


#define FOR_EACH_FULL_CONAPI_FUNCTION(F)                                                                                   \
  FOR_EACH_CONAPI_FUNCTION(F)                                                                                              \
  F(AllocConsole,               alloc_console,                  (_), sigVoidToBool,                 _)                     \
  F(FreeConsole,                free_console,                   (_), sigVoidToBool,                 _)                     \
  F(GetConsoleCursorInfo,       get_console_cursor_info,        (_), sigGetConsoleCursorInfo,       _)                     \
  F(GetConsoleOutputCP,         get_console_output_cp,          (_), sigVoidToUInt,                 _)                     \
  F(GetConsoleWindow,           get_console_window,             (_), sigVoidToHWnd,                 _)                     \
  F(ReadConsoleA,               read_console_a,                 (_), sigReadConsoleA,               _)                     \
  F(ReadConsoleW,               read_console_w,                 (_), sigReadConsoleW,               _)                     \
  F(ReadConsoleOutputA,         read_console_output_a,          (_), sigReadConsoleOutputA,         _)                     \
  F(ReadConsoleOutputW,         read_console_output_w,          (_), sigReadConsoleOutputW,         _)                     \
  F(SetConsoleCursorInfo,       set_console_cursor_info,        (_), sigSetConsoleCursorInfo,       _)                     \
  F(SetConsoleCursorPosition,   set_console_cursor_position,    (_), sigSetConsoleCursorPosition,   _)                     \
  F(WriteConsoleW,              write_console_w,                (_), sigWriteConsoleW,              _)                     \
  F(WriteConsoleOutputA,        write_console_output_a,         (_), sigWriteConsoleOutputA,        _)                     \
  F(WriteConsoleOutputW,        write_console_output_w,         (_), sigWriteConsoleOutputW,        _)

#define mfPf(PF) PF
#define mfOnlyPf(FLAGS, E) mfPf FLAGS (E, )
#define mfUnlessPf(FLAGS, E) mfPf FLAGS (, E)

class ConsoleAgent;
class InMemoryConsolePlatform;

// A container that holds the various definitions used by the other console
// types.
class ConsoleFrontend : public tclib::DefaultDestructable {
public:
  virtual ~ConsoleFrontend() { }

  // The types of the naked console functions.
#define __DECLARE_FRONTEND_FUNCTION__(Name, name, FLAGS, SIG, PSIG)            \
  typedef SIG(GET_SIG_RET) (WINAPI *name##_t)SIG(GET_SIG_PARAMS);
  FOR_EACH_CONAPI_FUNCTION(__DECLARE_FRONTEND_FUNCTION__)
#undef __DECLARE_FRONTEND_FUNCTION__

#define __DECLARE_FRONTEND_METHOD__(Name, name, FLAGS, SIG, PSIG)              \
  mfUnlessPf(FLAGS, virtual SIG(GET_SIG_RET) name SIG(GET_SIG_PARAMS) = 0);
  FOR_EACH_CONAPI_FUNCTION(__DECLARE_FRONTEND_METHOD__)
#undef __DECLARE_FRONTEND_METHOD__

  virtual NtStatus get_last_error() = 0;

  virtual int64_t poke_backend(int64_t value) { return 0; }

  // Creates and returns a new instance of the native console frontend.
  static tclib::pass_def_ref_t<ConsoleFrontend> new_native();

  // Creates and returns a new dummy console frontend.
  static tclib::pass_def_ref_t<ConsoleFrontend> new_dummy();

  // Returns a new simulating console frontend that simulates interaction with
  // the given backend.
  static tclib::pass_def_ref_t<ConsoleFrontend> new_simulating(
      ConsoleAgent *agent, InMemoryConsolePlatform *platform, ssize_t delta = 0);
};

// A console platform is similar to a console frontend, the difference being
// that what's in the frontend are functions we want to replace whereas the
// platform is console functions we want to have access to but not replace.
class ConsolePlatform : public tclib::DefaultDestructable {
public:
  virtual ~ConsolePlatform() { }

#define __DECLARE_PLATFORM_METHOD__(Name, name, FLAGS, SIG, PSIG)              \
  mfOnlyPf(FLAGS, virtual SIG(GET_SIG_RET) name SIG(GET_SIG_PARAMS) = 0);
  FOR_EACH_CONAPI_FUNCTION(__DECLARE_PLATFORM_METHOD__)
#undef __DECLARE_PLATFORM_METHOD__

  // Creates and returns a new native platform. Only available on windows.
  static tclib::pass_def_ref_t<ConsolePlatform> new_native();
};

class InMemoryConsolePlatform : public ConsolePlatform {
public:
  virtual uint32_t get_console_mode(Handle handle) = 0;
  virtual void set_console_mode(Handle handle, uint32_t mode) = 0;

  // Creates and returns a new platform simulator.
  static tclib::pass_def_ref_t<InMemoryConsolePlatform> new_simulating();
};

} // conprx

#endif // _AGENT_CONSOLE_FRONTEND_HH
