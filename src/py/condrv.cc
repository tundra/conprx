// Copyright 2014 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "console.hh"
#include "data.hh"
#include "pyth-inl.hh"

using namespace condrv;

// Called by python during module load.
PyMODINIT_FUNC initcondrv() {
  if (!AnsiBuffer::type
      .set_name("condrv.AnsiBuffer")
      .set_new().set_str().set_repr().set_len()
      .complete())
    return;

  PyType<Console> *console = Console::type();
  if (!console)
    return;

  PyObject* module = Py_InitModule3("condrv", NULL, "Console driver");

  Python::incref(AnsiBuffer::type);
  PyModule_AddObject(module, "AnsiBuffer", reinterpret_cast<PyObject*>(&AnsiBuffer::type));
  Python::incref(*console);
  PyModule_AddObject(module, "Console", reinterpret_cast<PyObject*>(console));
}
