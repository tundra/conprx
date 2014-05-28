// Copyright 2014 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "console.hh"
#include "data.hh"

using namespace condrv;

// DWORD WINAPI GetConsoleTitle(
//   _Out_  LPTSTR lpConsoleTitle,
//   _In_   DWORD nSize
// );
PyObject *Console::get_console_title_a(PyObject *self, PyObject *args) {
  PyObject *console_title_obj = NULL;
  long size = 0;
  if (!PyArg_ParseTuple(args, "Ol", &console_title_obj, &size))
    return NULL;
  AnsiBuffer *console_title = AnsiBuffer::type.cast(console_title_obj);
  if (!console_title) {
    PyErr_SetString(PyExc_TypeError, "Unexpected buffer to GetConsoleTitleA");
    return NULL;
  }
  long result = GetConsoleTitleA(console_title->as_c_str(), size);
  return PyInt_FromLong(result);
}

// HANDLE WINAPI GetStdHandle(
//   _In_  DWORD nStdHandle
// );
PyObject *Console::get_std_handle(PyObject *self, PyObject *args) {
  long std_handle = 0;
  if (!PyArg_ParseTuple(args, "l", &std_handle))
    return NULL;
  handle_t handle = GetStdHandle(std_handle);
  return Handle::create(handle);
}

// BOOL WINAPI SetConsoleTitle(
//   _In_  LPCTSTR lpConsoleTitle
// );
PyObject *Console::set_console_title_a(PyObject *self, PyObject *args) {
  PyObject *console_title_obj = NULL;
  if (!PyArg_ParseTuple(args, "O", &console_title_obj))
    return NULL;
  AnsiBuffer *console_title = AnsiBuffer::type.cast(console_title_obj);
  if (!console_title) {
    PyErr_SetString(PyExc_TypeError, "Unexpected buffer to SetConsoleTitleA");
    return NULL;
  }
  bool result = SetConsoleTitleA((LPSTR) console_title->as_c_str());
  return PyBool_FromLong(result);
}

PyType<Console> *Console::type() {
  static PyType<Console> *instance = NULL;
  if (instance == NULL) {
    PyType<Console> *type = new PyType<Console>();
    type->set_name("condrv.Console");
    type->set_methods();
    type->set_members();
    type->set_new();
    if (!type->complete())
      return NULL;
    instance = type;
  }
  return instance;
}

PyObject *Console::create(PyTypeObject *type, PyObject *args, PyObject *kwds) {
  Console *result = Console::type()->cast(type->tp_alloc(type, 0));
  result->std_input_handle_ = STD_INPUT_HANDLE;
  result->std_output_handle_ = STD_OUTPUT_HANDLE;
  result->std_error_handle_ = STD_ERROR_HANDLE;
  return result;
}

PyMemberDef Console::members[kConsoleMemberCount + 1] = {
  PY_MEMBER("STD_INPUT_HANDLE", T_LONG, Console, std_input_handle_),
  PY_MEMBER("STD_OUTPUT_HANDLE", T_LONG, Console, std_output_handle_),
  PY_MEMBER("STD_ERROR_HANDLE", T_LONG, Console, std_error_handle_),
  PY_LAST_MEMBER
};

#define ADD_METHOD_ENTRY(Name, name)                                           \
  {#Name, Console::name, METH_VARARGS, #Name},
PyMethodDef Console::methods[kConsoleFunctionCount + 1] = {
  FOR_EACH_CONSOLE_FUNCTION(ADD_METHOD_ENTRY)
  {NULL, NULL, 0, NULL}
};
#undef ADD_METHOD_ENTRY
