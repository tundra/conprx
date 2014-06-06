//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include <stdio.h>

LogBackend *LogBackend::get_default() {
  static StderrLogBackend *instance = NULL;
  if (instance == NULL)
    instance = new StderrLogBackend();
  return instance;
}
