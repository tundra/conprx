//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include <stdarg.h>
#include <stdio.h>

// Fallback log that only uses standard C functions that should be available on
// all platforms.
class FallbackLog : public Log {
public:
  virtual void record(Type type, const char *file, int line, const char *fmt, ...);
};

#define kMaxMessageSize 256

void FallbackLog::record(Type type, const char *file, int line, const char *fmt, ...) {
  char message[kMaxMessageSize];
  va_list args;
  va_start(args, fmt);
  vsnprintf(message, kMaxMessageSize, fmt, args);
  va_end(args);
  fprintf(stderr, "[%i] %s:%i: %s\n", type, file, line, message);
}

Log &Log::get() {
  static FallbackLog *instance = NULL;
  if (instance == NULL)
    instance = new FallbackLog();
  return *instance;
}
