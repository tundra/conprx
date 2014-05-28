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
  // __new__
  static PyObject *create(PyTypeObject *type, PyObject *args, PyObject *kwds);

  // __str__
  static PyObject *to_string(PyObject *object);

  // __repr__
  static PyObject *to_representation(PyObject *object);

  // __len__
  static Py_ssize_t length(PyObject *object);

  // Returns the raw underlying data buffer.
  uint8_t *data() { return data_; }

  // Python helpers.
  static PyType<AnsiBuffer> type;

private:
  // Can this string be mutated or is it constant?
  bool is_const_;

  // The underlying character data.
  uint8_t *data_;

  // The number of bytes in the given data array.
  size_t data_size_;
};

} // namespace condrv

#endif // _DATA
