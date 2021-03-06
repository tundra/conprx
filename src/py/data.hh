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
  ansi_str_t as_c_str() { return reinterpret_cast<ansi_str_t>(data_); }

  // Python type object.
  static PyType<AnsiBuffer> type;

private:
  // The underlying character data.
  uint8_t *data_;

  // The number of bytes in the given data array.
  size_t data_size_;
};

// An (16-bit) wide character buffer, mutable or const. It is wide in the
// windows sense, UTF-16, no in the ill-defined wchar_t sense which means
// different things in different contexts.
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

  // Returns this buffer's content viewed as an ansi c string.
  wide_str_t as_c_str() { return reinterpret_cast<wide_str_t>(data_); }

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

// CONSOLE_CURSOR_INFO
class ConsoleCursorInfo : public PyObject {
public:
  void init(long size, char visible);

  // __new__
  static PyObject *create(PyTypeObject *type, PyObject *args, PyObject *kwds);

  static PyType<ConsoleCursorInfo> type;
  static PyMemberDef members[3];

private:
  // Python makes assumptions about these types, that's why they're not dword_t
  // and bool_t.
  long size_;
  char visible_;
};

// COORD
class Coord : public PyObject {
public:
  void init(long x, long y);

  // __new__
  static PyObject *create(PyTypeObject *type, PyObject *args, PyObject *kwds);

  static PyType<Coord> type;
  static PyMemberDef members[3];

private:
  long x_;
  long y_;
};

// SMALL_RECT
class SmallRect : public PyObject {
public:
  void init(long left, long top, long right, long bottom);

  // __new__
  static PyObject *create(PyTypeObject *type, PyObject *args, PyObject *kwds);

  static PyType<SmallRect> type;
  static PyMemberDef members[5];

private:
  long left_;
  long top_;
  long right_;
  long bottom_;
};

} // namespace condrv

#endif // _DATA
