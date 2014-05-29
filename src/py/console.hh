// Copyright 2014 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Console function python bindings.

#ifndef _CONSOLE
#define _CONSOLE

#include "pyth.hh"

namespace condrv {

// Expands the given macro for each api function the console object understands.
#define FOR_EACH_CONSOLE_FUNCTION(F)                                           \
  F(GetConsoleMode,             get_console_mode)                              \
  F(GetConsoleTitleA,           get_console_title_a)                           \
  F(GetConsoleTitleW,           get_console_title_w)                           \
  F(GetStdHandle,               get_std_handle)                                \
  F(SetConsoleTitleA,           set_console_title_a)                           \
  F(SetConsoleTitleW,           set_console_title_w)                           \
  F(WriteConsoleA,              write_console_a)

#define ADD_ONE(Name, name) + 1
#define kConsoleFunctionCount (0 FOR_EACH_CONSOLE_FUNCTION(ADD_ONE))
#define kConsoleMemberCount 3

// A wrapper around the windows console.
class Console : public PyObject {
public:
  // __new__
  static PyObject *create(PyTypeObject *type, PyObject *args, PyObject *kwds);

  // Returns the type object that describes the console. The console object is
  // different on different platform so each one gets to construct it in their
  // own way. Returns NULL if construction fails.
  static PyType<Console> *type();

  // The console function adapters are below here. Because windows.h is creative
  // with the names of these functions they use a completely different naming
  // convention just to be on the safe side.

#define DECLARE_CONSOLE_FUNCTION(Name, name)                                   \
  static PyObject *name(PyObject *object, PyObject *args);
  FOR_EACH_CONSOLE_FUNCTION(DECLARE_CONSOLE_FUNCTION)
#undef DECLARE_CONSOLE_FUNCTION

  // Array of all the console methods, plus the end sentinel.
  static PyMethodDef methods[kConsoleFunctionCount + 1];
  static PyMemberDef members[kConsoleMemberCount + 1];

private:
  // Standard handles.
  long std_input_handle_;
  long std_output_handle_;
  long std_error_handle_;
};

} // namespace condrv

#endif // _CONSOLE
