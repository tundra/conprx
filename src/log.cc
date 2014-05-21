//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "log.hh"

#include <stdarg.h>
#include <stdio.h>

#define kMaxMessageSize 256

void log(const char *file, int line, const char *fmt, ...) {
  char message[kMaxMessageSize];
  va_list args;
  va_start(args, fmt);
  vsnprintf(message, kMaxMessageSize, fmt, args);
  va_end(args);
  fprintf(stderr, "%s:%i: %s\n", file, line, message);
}
