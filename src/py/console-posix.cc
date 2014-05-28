// Copyright 2014 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

PyType<Console> *Console::type() {
  static PyType<Console> *instance = NULL;
  if (instance == NULL) {
    PyType<Console> *type = new PyType<Console>();
    type->set_name("condrv.Console");
    type->set_new();
    if (!type->complete())
      return NULL;
    instance = type;
  }
  return instance;
}

PyObject *Console::create(PyTypeObject *type, PyObject *args, PyObject *kwds) {
  // On posix there is no windows console so the console constructor just
  // returns none.
  Py_RETURN_NONE;
}
