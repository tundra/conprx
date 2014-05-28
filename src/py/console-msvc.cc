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
  if (!AnsiBuffer::type.is_instance(console_title_obj)) {
    PyErr_SetString(PyExc_TypeError, "Unexpected buffer to GetConsoleTitleA");
    return NULL;
  }
  AnsiBuffer *console_title = static_cast<AnsiBuffer*>(console_title_obj);
  long result = GetConsoleTitleA((LPSTR) console_title->data(), size);
  return PyInt_FromLong(result);
}

// BOOL WINAPI SetConsoleTitle(
//   _In_  LPCTSTR lpConsoleTitle
// );
PyObject *Console::set_console_title_a(PyObject *self, PyObject *args) {
  PyObject *console_title_obj = NULL;
  if (!PyArg_ParseTuple(args, "O", &console_title_obj))
    return NULL;
  if (!AnsiBuffer::type.is_instance(console_title_obj)) {
    PyErr_SetString(PyExc_TypeError, "Unexpected buffer to SetConsoleTitleA");
    return NULL;
  }
  AnsiBuffer *console_title = static_cast<AnsiBuffer*>(console_title_obj);
  bool result = SetConsoleTitleA((LPSTR) console_title->data());
  return PyBool_FromLong(result);
}

PyType<Console> *Console::type() {
  static PyType<Console> *instance = NULL;
  if (instance == NULL) {
    PyType<Console> *type = new PyType<Console>();
    type->set_name("condrv.Console");
    type->set_methods();
    type->set_new();
    if (!type->complete())
      return NULL;
    instance = type;
  }
  return instance;
}

PyObject *Console::create(PyTypeObject *type, PyObject *args, PyObject *kwds) {
  return type->tp_alloc(type, 0);
}

#define ADD_METHOD_ENTRY(Name, name)                                           \
  {#Name, Console::name, METH_VARARGS, #Name},
PyMethodDef Console::methods[kConsoleFunctionCount + 1] = {
  FOR_EACH_CONSOLE_FUNCTION(ADD_METHOD_ENTRY)
  {NULL, NULL, 0, NULL}
};
#undef ADD_METHOD_ENTRY
