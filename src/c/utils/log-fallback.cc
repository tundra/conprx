//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include <stdio.h>

// Fallback log that only uses standard C functions that should be available on
// all platforms.
class FallbackLog : public Log {
public:
  virtual void vrecord(Type type, const char *file, int line, const char *fmt, va_list args);
};

#define kMaxMessageSize 256

void FallbackLog::vrecord(Type type, const char *file, int line, const char *fmt, va_list args) {
  char message[kMaxMessageSize];
  vsnprintf(message, kMaxMessageSize, fmt, args);
  fprintf(stderr, "[%i] %s:%i: %s\n", type, file, line, message);
}

Log &Log::get() {
  static FallbackLog *instance = NULL;
  if (instance == NULL)
    instance = new FallbackLog();
  return *instance;
}
