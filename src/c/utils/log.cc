//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "log.hh"

Log::Log()
  : enable_debug_log_(false) { }

Log::~Log() { }

void Log::set_debug_log_enabled(bool value) {
  enable_debug_log_ = value;
}

void Log::record(Type type, const char *file, int line, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vrecord(type, file, line, fmt, args);
  va_end(args);
}

void Log::record_debug(const char *file, int line, const char *fmt, ...) {
  if (!enable_debug_log_)
    return;
  va_list args;
  va_start(args, fmt);
  vrecord(DBG, file, line, fmt, args);
  va_end(args);
}

#ifdef IS_MSVC
#include "log-msvc.cc"
#else
#include "log-fallback.cc"
#endif
