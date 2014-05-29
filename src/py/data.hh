// Copyright 2014 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Datatypes used with the console driver.

#ifndef _DATA
#define _DATA

#include "pyth.hh"

namespace condrv {

// An (8-bit) ansi buffer, mutable or const.
//
// An ansi buffer is sort of like a string in that it holds 8-bit characters
// and is usually used for strings, but it is unlike a string in that it is
// explicitly null-terminated (so when it holds a null-terminated string the
// terminator is counted in the length) and can be mutable. Think of it as a
// buffer that happens, for convenience, to be easily convertible to a string.
class AnsiBuffer : public PyObject {
public:
  // Initializes an already existing instance.
  void init(uint8_t *data, size_t data_size);

  // __new__
  static PyObject *create(PyTypeObject *type, PyObject *args, PyObject *kwds);

  // __dealloc__
  static void dispose(PyObject *object);

  // __str__
  static PyObject *to_string(PyObject *object);

  // __repr__
  static PyObject *to_representation(PyObject *object);

  // __len__
  static Py_ssize_t length(PyObject *object);

  // Returns this buffer's content viewed as an ansi c string.
  ansi_c_str_t as_c_str() { return reinterpret_cast<ansi_c_str_t>(data_); }

  // Python type object.
  static PyType<AnsiBuffer> type;

private:
  // The underlying character data.
  uint8_t *data_;

  // The number of bytes in the given data array.
  size_t data_size_;
};

// An (16-bit) wide character buffer, mutable or const.
class WideBuffer : public PyObject {
public:
  // Initializes an already existing instance.
  void init(uint16_t *data, size_t data_size);

  // __new__
  static PyObject *create(PyTypeObject *type, PyObject *args, PyObject *kwds);

  // __dealloc__
  static void dispose(PyObject *object);

  // __str__
  static PyObject *to_string(PyObject *object);

  // __repr__
  static PyObject *to_representation(PyObject *object);

  // __len__
  static Py_ssize_t length(PyObject *object);

  // __item__
  static PyObject *get_item(PyObject *object, Py_ssize_t index);

  // Returns the index of the first \0 in the given string.
  static size_t wstrlen(wide_c_str_t str);

  // Returns this buffer's content viewed as an ansi c string.
  wide_c_str_t as_c_str() { return reinterpret_cast<wide_c_str_t>(data_); }

  // Python type object.
  static PyType<WideBuffer> type;

private:
  // The underlying character data.
  uint16_t *data_;

  // The number of shorts in the given data array.
  size_t data_size_;
};

// Wrapper around a windows HANDLE. Handles cannot be created from python, only
// returned from api calls.
class Handle : public PyObject {
public:
  // Initializes an already existing instance.
  void init(handle_t handle);

  // Creates and initializes a new instance.
  static Handle *create(handle_t handle);

  // __repr__
  static PyObject *to_representation(PyObject *object);

  // Returns the handle wrapped by this object.
  handle_t handle() { return handle_; }

  // Python type object.
  static PyType<Handle> type;

private:
  handle_t handle_;
};

// A pointer to a dword. Used for output parameters.
class DwordRef : public PyObject {
public:
  void init();

  // __new__
  static PyObject *create(PyTypeObject *type, PyObject *args, PyObject *kwds);

  // __repr__
  static PyObject *to_representation(PyObject *object);

  // Returns a pointer to this ref's value.
  dword_t *ref() { return &value_; }

  static PyType<DwordRef> type;
  static PyMemberDef members[2];

private:
  dword_t value_;
};

} // namespace condrv

#endif // _DATA
