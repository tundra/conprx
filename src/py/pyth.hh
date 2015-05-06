// Copyright 2014 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// The python header, included suitably to compile with a high warning setting.

#ifndef _PYTH
#define _PYTH

// We can't use the macros from types.hh because python.h insists on being
// included first and sometimes fails to include if it isn't. What a surprise:
// python sucks at the api level too.

#define Py_UNICODE_SIZE 4

#ifdef _MSC_VER
#  pragma warning(push, 0)
#    include <Python.h>
#    include <structmember.h>
#  pragma warning(pop)
#else
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wall"
#    include <Python.h>
#    include <structmember.h>
#  pragma GCC diagnostic pop
#endif

#include "utils/types.hh"

namespace condrv {

// Utilities for working with python.
class Python {
public:
  // Increments the ref count of the given object. Use this instead of the
  // Py_INCREF macro to avoid a strict aliasing warning.
  template <typename T>
  static T &incref(T &obj);
};

// Wrapper that simplifies building of python types somewhat.
template <typename T>
class PyType : public PyTypeObject {
public:
  PyType();

  // Sets the name of this type.
  PyType<T> &set_name(const char *name);

  // Sets the new-function of this type to T::create.
  PyType<T> &set_new();

  // Sets the dealloc-function of this type to T::dispose.
  PyType<T> &set_dealloc();

  // Sets the str-function of this type to T::to_string.
  PyType<T> &set_str();

  // Sets the repr-function of this type to T::to_representation.
  PyType<T> &set_repr();

  // Sets the methods of this type to T::methods.
  PyType<T> &set_methods();

  // Sets the methods of this type to T::members.
  PyType<T> &set_members();

  // Sets the length function of this type to T::length.
  PyType<T> &set_len();

  // Sets the item getter of this type to T::get_item.
  PyType<T> &set_item();

  // Binds this type to a name in the given module.
  PyType<T> &bind(PyObject *module, const char *name);

  // Completes the construction of this type, returning true iff the completion
  // succeeded.
  bool complete();

  // Returns true iff the given object is an instance of this type.
  bool is_instance(PyObject *obj);

  // Returns the given object viewed as this type if it is an instance,
  // otherwise NULL.
  T *cast(PyObject *obj);

private:
  // The sequence methods to use if this type has any.
  PySequenceMethods sequence_methods_;
};

// Shorthand for declaring members.
#define PY_MEMBER(NAME, TYPE, HOLDER, FIELD) \
  {const_cast<char*>(NAME), TYPE, offsetof(HOLDER, FIELD), 0, NULL}

// Last entry in a list of members.
#define PY_LAST_MEMBER {NULL, 0, 0, 0, NULL}

} // condrv

#endif // _PYTH
