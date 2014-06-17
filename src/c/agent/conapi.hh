//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Declarations of the console api.

#ifndef _CONAPI
#define _CONAPI

#include "agent/binpatch.hh"
#include "utils/types.hh"
#include "utils/vector.hh"

namespace conprx {

#define FOR_EACH_CONAPI_FUNCTION(F)                                            \
  F(AllocConsole, alloc_console, bool_t,                                       \
      (),                                                                      \
      ())                                                                      \
  F(GetConsoleTitleA, get_console_title_a, dword_t,                            \
      (ansi_str_t console_title, dword_t size),                                \
      (console_title, size))                                                   \
  F(GetConsoleTitleW, get_console_title_w, dword_t,                            \
      (wide_str_t console_title, dword_t size),                                \
      (console_title, size))                                                   \
  F(GetStdHandle, get_std_handle, handle_t,                                    \
      (dword_t std_handle),                                                    \
      (std_handle))                                                            \
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
       number_of_chars_written, reserved))

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

#define __DECLARE_CONAPI_METHOD__(Name, name, RET, PARAMS, ARGS)               \
  virtual RET name PARAMS;
  FOR_EACH_CONAPI_FUNCTION(__DECLARE_CONAPI_METHOD__)
#undef __DECLARE_CONAPI_METHOD__

private:
  Console *delegate_;
};

} // conprx

#endif // _CONAPI
