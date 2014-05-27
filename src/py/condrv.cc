// Copyright 2014 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "condrv-inl.hh"

using namespace condrv;

// --- A n s i   s t r i n g ---

// Creates a new ansi string based either on a string or a buffer length.
PyObject *AnsiString::create(PyTypeObject *type, PyObject *args, PyObject *kwds) {
  PyObject *arg = NULL;
  if (!PyArg_ParseTuple(args, "O", &arg))
    return NULL;

  uint8_t *data = NULL;
  size_t data_size = 0;
  bool is_const = false;
  if (PyString_Check(arg)) {
    data_size = PyString_GET_SIZE(arg) + 1;
    is_const = true;
    data = new uint8_t[data_size];
    memcpy(data, PyString_AsString(arg), data_size);
  } else if (PyInt_Check(arg)) {
    data_size = PyInt_AsLong(arg);
    data = new uint8_t[data_size];
    memset(data, 0, data_size);
  } else {
    return NULL;
  }

  PyObject *object = type->tp_alloc(type, 0);
  AnsiString *self = static_cast<AnsiString*>(object);
  self->data_ = data;
  self->is_const_ = is_const;
  self->data_size_ = data_size;
  return object;
}

PyObject *AnsiString::to_string(PyObject *object) {
  AnsiString *self = static_cast<AnsiString*>(object);
  const char *c_str = reinterpret_cast<char*>(self->data_);
  PyObject *data_str = PyString_FromStringAndSize(c_str, self->data_size_ - 1);
  PyObject *format = PyString_FromString("A\"%s\"");
  PyObject *args = Py_BuildValue("O", data_str);
  PyObject *result = PyString_Format(format, args);
  return result;
}

Py_ssize_t AnsiString::length(PyObject *object) {
  AnsiString *self = static_cast<AnsiString*>(object);
  return self->data_size_;
}

PyMethodDef AnsiString::methods[2] = {
  {NULL, NULL, 0, NULL},
  {NULL, NULL, 0, NULL}
};

PyType<AnsiString> AnsiString::type;

// --- M o d u l e ---

// Called by python during module load.
PyMODINIT_FUNC initcondrv() {
  if (!AnsiString::type
      .set_name("condrv.AnsiString")
      .has_new().has_str().has_len().has_methods()
      .complete()) {
    return;
  }

  PyObject* module = Py_InitModule3("condrv", NULL,
      "Example module that creates an extension type.");

  Py_INCREF(&AnsiString::type);
  PyModule_AddObject(module, "AnsiString", (PyObject*) &AnsiString::type);
}
