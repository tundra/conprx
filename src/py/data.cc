// Copyright 2014 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "data.hh"
#include "pyth-inl.hh"

using namespace condrv;

// --- A n s i   s t r i n g ---

// Creates a new ansi buffer based either on a string or a length.
PyObject *AnsiBuffer::create(PyTypeObject *type, PyObject *args, PyObject *kwds) {
  PyObject *arg = NULL;
  if (!PyArg_ParseTuple(args, "O", &arg))
    return NULL;

  uint8_t *data = NULL;
  size_t data_size = 0;
  bool is_const = false;
  if (PyString_Check(arg)) {
    // AnsiBuffer(str)
    data_size = PyString_GET_SIZE(arg) + 1;
    is_const = true;
    data = new uint8_t[data_size];
    // Copy the whole input string, including the null terminator.
    memcpy(data, PyString_AsString(arg), data_size);
  } else if (PyInt_Check(arg)) {
    // AnsiBuffer(int)
    data_size = PyInt_AsLong(arg);
    data = new uint8_t[data_size];
    memset(data, 0, data_size);
  } else {
    PyErr_SetString(PyExc_TypeError, "Unexpected argument to AnsiBuffer()");
    return NULL;
  }

  AnsiBuffer *self = AnsiBuffer::type.cast(type->tp_alloc(type, 0));
  self->data_ = data;
  self->is_const_ = is_const;
  self->data_size_ = data_size;
  return self;
}

PyObject *AnsiBuffer::to_representation(PyObject *object) {
  // The representation explicitly includes the null terminator for null-
  // terminated strings.
  AnsiBuffer *self = AnsiBuffer::type.cast(object);
  const char *c_str = self->as_c_str();
  PyObject *data_str = PyString_FromStringAndSize(c_str, self->data_size_);
  PyObject *format = PyString_FromString("A\"%s\"");
  PyObject *args = Py_BuildValue("O", data_str);
  PyObject *result = PyString_Format(format, args);
  return result;
}

PyObject *AnsiBuffer::to_string(PyObject *object) {
  // The string conversion stops at the first null character if the string is
  // null-terminated.
  AnsiBuffer *self = AnsiBuffer::type.cast(object);
  const char *c_str = self->as_c_str();
  size_t data_size = self->data_size_ - 1;
  size_t length = (c_str[data_size] == '\0') ? strlen(c_str) : data_size - 1;
  return PyString_FromStringAndSize(c_str, length);
}

Py_ssize_t AnsiBuffer::length(PyObject *object) {
  AnsiBuffer *self = AnsiBuffer::type.cast(object);
  return self->data_size_;
}

PyType<AnsiBuffer> AnsiBuffer::type;
