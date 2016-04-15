//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include <execinfo.h>

using namespace conprx;

fat_bool_t Interceptor::capture_stacktrace(Vector<void*> buffer) {
  int frames = static_cast<int>(buffer.length());
  int captured = backtrace(buffer.start(), frames);
  return F_BOOL(captured == frames);
}
