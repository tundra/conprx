// Copyright 2014 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "console.hh"
#include "data.hh"

using namespace condrv;

// BOOL WINAPI GetConsoleMode(
//   _In_   HANDLE hConsoleHandle,
//   _Out_  LPDWORD lpMode
// );
PyObject *Console::get_console_mode(PyObject *self, PyObject *args) {
  PyObject *console_handle_obj = NULL;
  PyObject *mode_obj = NULL;
  if (!PyArg_ParseTuple(args, "OO", &console_handle_obj, &mode_obj))
    return NULL;
  Handle *console_handle = Handle::type.cast(console_handle_obj);
  DwordRef *mode = DwordRef::type.cast(mode_obj);
  if (!console_handle || !mode)
    return NULL;
  bool result = GetConsoleMode(console_handle->handle(), mode->ref());
  return PyBool_FromLong(result);
}

// BOOL WINAPI GetConsoleCursorInfo(
//   _In_   HANDLE hConsoleOutput,
//   _Out_  PCONSOLE_CURSOR_INFO lpConsoleCursorInfo
// );
PyObject *Console::get_console_cursor_info(PyObject *self, PyObject *args) {
  PyObject *console_output_obj = NULL;
  PyObject *console_cursor_info_obj = NULL;
  if (!PyArg_ParseTuple(args, "OO", &console_output_obj, &console_cursor_info_obj))
    return NULL;
  Handle *console_output = Handle::type.cast(console_output_obj);
  ConsoleCursorInfo *console_cursor_info = ConsoleCursorInfo::type.cast(console_cursor_info_obj);
  if (!console_output || !console_cursor_info)
    return NULL;
  CONSOLE_CURSOR_INFO info;
  ZeroMemory(&info, sizeof(info));
  bool result = GetConsoleCursorInfo(console_output->handle(), &info);
  console_cursor_info->init(info.dwSize, info.bVisible);
  return PyBool_FromLong(result);
}

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
  if (!console_title)
    return NULL;
  long result = GetConsoleTitleA(console_title->as_c_str(), size);
  return PyInt_FromLong(result);
}

PyObject *Console::get_console_title_w(PyObject *self, PyObject *args) {
  PyObject *console_title_obj = NULL;
  long size = 0;
  if (!PyArg_ParseTuple(args, "Ol", &console_title_obj, &size))
    return NULL;
  WideBuffer *console_title = WideBuffer::type.cast(console_title_obj);
  if (!console_title)
    return NULL;
  long result = GetConsoleTitleW(console_title->as_c_str(), size);
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

PyObject *Console::is_console_screen_buffer(PyObject *self, PyObject *args) {
  PyObject *value_obj = NULL;
  if (!PyArg_ParseTuple(args, "O", &value_obj))
    return NULL;
  Handle *value = Handle::type.cast(value_obj);
  CONSOLE_SCREEN_BUFFER_INFO info;
  bool result = GetConsoleScreenBufferInfo(value->handle(), &info);
  return PyBool_FromLong(result);
}

// BOOL WINAPI SetConsoleTitle(
//   _In_  LPCTSTR lpConsoleTitle
// );
PyObject *Console::set_console_title_a(PyObject *self, PyObject *args) {
  PyObject *console_title_obj = NULL;
  if (!PyArg_ParseTuple(args, "O", &console_title_obj))
    return NULL;
  AnsiBuffer *console_title = AnsiBuffer::type.cast(console_title_obj);
  if (!console_title)
    return NULL;
  bool result = SetConsoleTitleA(console_title->as_c_str());
  return PyBool_FromLong(result);
}

PyObject *Console::set_console_title_w(PyObject *self, PyObject *args) {
  PyObject *console_title_obj = NULL;
  if (!PyArg_ParseTuple(args, "O", &console_title_obj))
    return NULL;
  WideBuffer *console_title = WideBuffer::type.cast(console_title_obj);
  if (!console_title)
    return NULL;
  bool result = SetConsoleTitleW(console_title->as_c_str());
  return PyBool_FromLong(result);
}

// BOOL WINAPI WriteConsole(
//   _In_        HANDLE hConsoleOutput,
//   _In_        const VOID *lpBuffer,
//   _In_        DWORD nNumberOfCharsToWrite,
//   _Out_       LPDWORD lpNumberOfCharsWritten,
//   _Reserved_  LPVOID lpReserved
// );
PyObject *Console::write_console_a(PyObject *self, PyObject *args) {
  PyObject *console_output_obj = NULL;
  PyObject *buffer_obj = NULL;
  long number_of_chars_to_write = 0;
  PyObject *number_of_chars_written_obj = NULL;
  PyObject *reserved = NULL;
  if (!PyArg_ParseTuple(args, "OOlOO", &console_output_obj, &buffer_obj,
      &number_of_chars_to_write, &number_of_chars_written_obj, &reserved))
    return NULL;
  Handle *console_output = Handle::type.cast(console_output_obj);
  AnsiBuffer *buffer = AnsiBuffer::type.cast(buffer_obj);
  DwordRef *number_of_chars_written = DwordRef::type.cast(number_of_chars_written_obj);
  if (!console_output || !buffer || !number_of_chars_written)
    return NULL;
  bool result = WriteConsoleA(console_output->handle(), buffer->as_c_str(),
      number_of_chars_to_write, number_of_chars_written->ref(), NULL);
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
