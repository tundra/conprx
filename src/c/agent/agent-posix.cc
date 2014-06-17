//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "utils/types.hh"

address_t ConsoleAgent::get_console_function_address(cstr_t name) {
  return NULL;
}

Options &Options::get() {
  static Options *instance = NULL;
  if (instance == NULL)
    instance = new Options();
  return *instance;
}
