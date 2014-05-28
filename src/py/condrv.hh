// Copyright 2014 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Console driver for python.

#ifndef _CONDRV
#define _CONDRV

// The python docs say to include Python.h before anything else but we need
// stuff from here to control how to import it so this one gets to go first.
#include "utils/types.hh"

// Okay now we can import Python.h.
#ifdef IS_MSVC
#  pragma warning(push, 0)
#    include <Python.h>
#  pragma warning(pop)
#else
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wall"
#    include <Python.h>
#  pragma GCC diagnostic pop
#endif

namespace condrv {

// Wrapper that simplifies building of python types somewhat.
template <typename T>
struct PyType : public PyTypeObject {
public:
  PyType();

  // Sets the name of this type.
  PyType<T> &set_name(const char *name);

  // Sets the new-function of this type to T::create.
  PyType<T> &set_new();

  // Sets the str-function of this type to T::to_string.
  PyType<T> &set_str();

  // Sets the repr-function of this type to T::to_representation.
  PyType<T> &set_repr();

  // Sets the methods of this type to T::py_methods.
  PyType<T> &set_methods();

  // Sets the length function of this type to T::length;
  PyType<T> &set_len();

  // Completes the construction of this type, returning true iff the completion
  // succeeded.
  bool complete();

private:
  // The sequence methods to use if this type has any.
  PySequenceMethods sequence_methods_;
};

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

#endif // _CONDRV
