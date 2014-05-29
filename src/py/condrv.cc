// Copyright 2014 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "console.hh"
#include "data.hh"
#include "pyth-inl.hh"

using namespace condrv;

// Called by python during module load.
PyMODINIT_FUNC initcondrv() {
  PyObject* module = Py_InitModule3("condrv", NULL, "Console driver");

  if (!AnsiBuffer::type
      .set_name("condrv.AnsiBuffer")
      .set_new().set_dealloc().set_str().set_repr().set_len()
      .complete())
    return;
  AnsiBuffer::type.bind(module, "AnsiBuffer");

  if (!WideBuffer::type
      .set_name("condrv.WideBuffer")
      .set_new().set_dealloc().set_str().set_repr().set_len().set_item()
      .complete())
    return;
  WideBuffer::type.bind(module, "WideBuffer");

  if (!Handle::type
      .set_name("condrv.Handle")
      .set_repr()
      .complete())
    return;
  Handle::type.bind(module, "Handle");

  if (!DwordRef::type
      .set_name("condrv.DwordRef")
      .set_new().set_repr().set_members()
      .complete())
    return;
  DwordRef::type.bind(module, "DwordRef");

  PyType<Console> *console = Console::type();
  if (!console)
    return;
  console->bind(module, "Console");
}
