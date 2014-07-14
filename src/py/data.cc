// Copyright 2014 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "data.hh"
#include "pyth-inl.hh"
#include "utils/string.hh"

using namespace condrv;
using namespace conprx;

// --- A n s i   b u f f e r ---

void AnsiBuffer::init(uint8_t *data, size_t data_size) {
  this->data_ = data;
  this->data_size_ = data_size;
}

// Creates a new ansi buffer based either on a string or a length.
PyObject *AnsiBuffer::create(PyTypeObject *type, PyObject *args, PyObject *kwds) {
  PyObject *arg = NULL;
  if (!PyArg_ParseTuple(args, "O", &arg))
    return NULL;

  uint8_t *data = NULL;
  size_t data_size = 0;
  if (PyString_Check(arg)) {
    // AnsiBuffer(str)
    data_size = PyString_GET_SIZE(arg) + 1;
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
  self->init(data, data_size);
  return self;
}

void AnsiBuffer::dispose(PyObject *object) {
  AnsiBuffer *self = AnsiBuffer::type.cast(object);
  delete[] self->data_;
}

PyObject *AnsiBuffer::to_representation(PyObject *object) {
  // The representation explicitly includes the null terminator for null-
  // terminated strings.
  AnsiBuffer *self = AnsiBuffer::type.cast(object);
  ansi_str_t c_str = self->as_c_str();
  PyObject *data_str = PyString_FromStringAndSize(c_str, self->data_size_);
  PyObject *format = PyString_FromString("A'%s'");
  PyObject *args = Py_BuildValue("O", data_str);
  PyObject *result = PyString_Format(format, args);
  return result;
}

PyObject *AnsiBuffer::to_string(PyObject *object) {
  // The string conversion stops at the first null character if the string is
  // null-terminated.
  AnsiBuffer *self = AnsiBuffer::type.cast(object);
  ansi_str_t c_str = self->as_c_str();
  size_t data_size = self->data_size_ - 1;
  size_t length = (c_str[data_size] == '\0') ? strlen(c_str) : data_size - 1;
  return PyString_FromStringAndSize(c_str, length);
}

Py_ssize_t AnsiBuffer::length(PyObject *object) {
  AnsiBuffer *self = AnsiBuffer::type.cast(object);
  return self->data_size_;
}

PyType<AnsiBuffer> AnsiBuffer::type;


// --- W i d e   b u f f e r ---

void WideBuffer::init(uint16_t *data, size_t data_size) {
  this->data_ = data;
  this->data_size_ = data_size;
}

// Creates a new ansi buffer based either on a string or a length.
PyObject *WideBuffer::create(PyTypeObject *type, PyObject *args, PyObject *kwds) {
  PyObject *arg = NULL;
  if (!PyArg_ParseTuple(args, "O", &arg))
    return NULL;

  uint16_t *data = NULL;
  size_t data_size = 0;
  if (PyUnicode_Check(arg)) {
    // WideBuffer(unicode)
    // Encode the unicode string as utf16. The result is a plain string (not a
    // unicode!) that holds the encoded bytes.
    PyObject *utf16 = PyUnicode_AsUTF16String(arg);
    // The first (16-bit) character will always be a BOM so discount that.
    uint16_t *chars = reinterpret_cast<uint16_t*>(PyString_AS_STRING(utf16)) + 1;
    size_t char_count = (PyString_Size(utf16) / sizeof(uint16_t)) - 1;
    // Add room for the null terminator.
    data_size = char_count + 1;
    data = new uint16_t[data_size];
    // The string is not null-terminated so first copy the characters, then
    // null-terminate.
    memcpy(data, chars, sizeof(uint16_t) * char_count);
    data[char_count] = '\0';
  } else if (PyInt_Check(arg)) {
    // WideBuffer(int)
    data_size = PyInt_AsLong(arg);
    data = new uint16_t[data_size];
    memset(data, 0, sizeof(uint16_t) * data_size);
  } else {
    PyErr_SetString(PyExc_TypeError, "Unexpected argument to WideBuffer()");
    return NULL;
  }

  WideBuffer *self = WideBuffer::type.cast(type->tp_alloc(type, 0));
  self->init(data, data_size);
  return self;
}

void WideBuffer::dispose(PyObject *object) {
  WideBuffer *self = WideBuffer::type.cast(object);
  delete[] self->data_;
}

PyObject *WideBuffer::to_representation(PyObject *object) {
  // The representation explicitly includes the null terminator for null-
  // terminated strings.
  WideBuffer *self = WideBuffer::type.cast(object);
  wide_str_t w_str = self->as_c_str();
  // Decode the utf16 data in the buffer into a python unicode object.
  PyObject *data_str = PyUnicode_DecodeUTF16(reinterpret_cast<const char *>(w_str),
      self->data_size_ * sizeof(uint16_t), NULL, NULL);
  // Use the %r formatter directive which calls repr which converts non-ascii
  // characters to escapes, which is what we want.
  PyObject *format = PyString_FromString("U%r");
  PyObject *args = Py_BuildValue("O", data_str);
  return PyUnicode_Format(format, args);
}

PyObject *WideBuffer::to_string(PyObject *object) {
  // The string conversion stops at the first null character if the string is
  // null-terminated.
  WideBuffer *self = WideBuffer::type.cast(object);
  wide_str_t w_str = self->as_c_str();
  size_t last_char = self->data_size_ - 1;
  size_t length = (w_str[last_char] == '\0') ? String::wstrlen(w_str) : last_char;
  // This actually returns a unicode, not a string, but python seems to do the
  // right thing both when calling this through str() and unicode().
  return PyUnicode_DecodeUTF16(reinterpret_cast<const char *>(w_str),
      sizeof(uint16_t) * length, NULL, NULL);
}

Py_ssize_t WideBuffer::length(PyObject *object) {
  WideBuffer *self = WideBuffer::type.cast(object);
  return self->data_size_;
}

PyObject *WideBuffer::get_item(PyObject *object, Py_ssize_t index) {
  WideBuffer *self = WideBuffer::type.cast(object);
  if (static_cast<Py_ssize_t>(self->data_size_) <= index) {
    PyErr_SetString(PyExc_IndexError, "WideBuffer index out of bounds");
    return NULL;
  }
  return PyInt_FromLong(self->data_[index]);
}

PyType<WideBuffer> WideBuffer::type;


// --- H a n d l e ---

void Handle::init(handle_t handle) {
  this->handle_ = handle;
}

Handle *Handle::create(handle_t value) {
  PyObject *object = type.tp_alloc(&type, 0);
  Handle *result = type.cast(object);
  result->init(value);
  return result;
}

PyObject *Handle::to_representation(PyObject *object) {
  Handle *self = Handle::type.cast(object);
  PyObject *format = PyString_FromString("H[0x%x]");
  PyObject *args = Py_BuildValue("l", reinterpret_cast<long>(self->handle_));
  PyObject *result = PyString_Format(format, args);
  return result;
}

PyType<Handle> Handle::type;

// --- D w o r d   r e f ---

void DwordRef::init() {
  this->value_ = 0;
}

PyObject *DwordRef::create(PyTypeObject *type, PyObject *args, PyObject *kwds) {
  DwordRef *self = DwordRef::type.cast(type->tp_alloc(type, 0));
  self->init();
  return self;
}

PyObject *DwordRef::to_representation(PyObject *object) {
  DwordRef *self = DwordRef::type.cast(object);
  PyObject *format = PyString_FromString("&%i");
  PyObject *args = Py_BuildValue("l", static_cast<long>(self->value_));
  PyObject *result = PyString_Format(format, args);
  return result;
}

PyType<DwordRef> DwordRef::type;
PyMemberDef DwordRef::members[2] = {
  PY_MEMBER("value", T_LONG, DwordRef, value_),
  PY_LAST_MEMBER
};

// --- C o n s o l e   c u r s o r   i n f o ---

PyObject *ConsoleCursorInfo::create(PyTypeObject *type, PyObject *args, PyObject *kwds) {
  long size = 0;
  long visible = false;
  if (!PyArg_ParseTuple(args, "ll", &size, &visible))
    return NULL;

  ConsoleCursorInfo *self = ConsoleCursorInfo::type.cast(type->tp_alloc(type, 0));
  self->init(size, !!visible);
  return self;
}

void ConsoleCursorInfo::init(long size, char visible) {
  size_ = size;
  visible_ = visible;
}

PyType<ConsoleCursorInfo> ConsoleCursorInfo::type;

PyMemberDef ConsoleCursorInfo::members[3] = {
  PY_MEMBER("size", T_LONG, ConsoleCursorInfo, size_),
  PY_MEMBER("visible", T_BOOL, ConsoleCursorInfo, visible_),
  PY_LAST_MEMBER
};

// --- C o o r d ---

PyObject *Coord::create(PyTypeObject *type, PyObject *args, PyObject *kwds) {
  long x = 0;
  long y = 0;
  if (!PyArg_ParseTuple(args, "ll", &x, &y))
    return NULL;

  Coord *self = Coord::type.cast(type->tp_alloc(type, 0));
  self->init(x, y);
  return self;
}

void Coord::init(long x, long y) {
  x_ = x;
  y_ = y;
}

PyType<Coord> Coord::type;

PyMemberDef Coord::members[3] = {
  PY_MEMBER("x", T_LONG, Coord, x_),
  PY_MEMBER("y", T_LONG, Coord, y_),
  PY_LAST_MEMBER
};

// --- S m a l l   r e c t ---

PyObject *SmallRect::create(PyTypeObject *type, PyObject *args, PyObject *kwds) {
  long left = 0;
  long top = 0;
  long right = 0;
  long bottom = 0;
  if (!PyArg_ParseTuple(args, "llll", &left, &top, &right, &bottom))
    return NULL;

  SmallRect *self = SmallRect::type.cast(type->tp_alloc(type, 0));
  self->init(left, top, right, bottom);
  return self;
}

void SmallRect::init(long left, long top, long right, long bottom) {
  left_ = left;
  top_ = top;
  right_ = right;
  bottom_ = bottom;
}

PyType<SmallRect> SmallRect::type;

PyMemberDef SmallRect::members[5] = {
  PY_MEMBER("left", T_LONG, SmallRect, left_),
  PY_MEMBER("top", T_LONG, SmallRect, top_),
  PY_MEMBER("right", T_LONG, SmallRect, right_),
  PY_MEMBER("bottom", T_LONG, SmallRect, bottom_),
  PY_LAST_MEMBER
};
