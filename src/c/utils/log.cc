//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "log.hh"

Log::Log(LogBackend *primary, LogBackend *secondary)
  : enable_debug_log_(false)
  , primary_(primary)
  , secondary_(secondary) { }

LogBackend::~LogBackend() { }

#define kMaxLogMessageSize 1024

void StderrLogBackend::vrecord(Log::Type type, const char *file, int line,
    const char *fmt, va_list args) {
  char message[kMaxLogMessageSize];
  vsnprintf(message, kMaxLogMessageSize, fmt, args);
  fprintf(stderr, "[%i] %s:%i: %s\n", type, file, line, message);
  fflush(stderr);
}

void Log::set_debug_log_enabled(bool value) {
  enable_debug_log_ = value;
}

void Log::record(Type type, const char *file, int line, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  primary().vrecord(type, file, line, fmt, args);
  va_end(args);
}

void Log::record_debug(const char *file, int line, const char *fmt, ...) {
  if (!enable_debug_log_)
    return;
  va_list args;
  va_start(args, fmt);
  primary().vrecord(DBG, file, line, fmt, args);
  va_end(args);
}

void Log::record_debug_fallback(const char *file, int line, const char *fmt, ...) {
  if (!enable_debug_log_)
    return;
  va_list args;
  va_start(args, fmt);
  secondary().vrecord(DBG, file, line, fmt, args);
  va_end(args);
}


Log &Log::get() {
  static Log *instance = NULL;
  if (instance == NULL) {
    LogBackend *primary = LogBackend::get_default();
    LogBackend *secondary = new StderrLogBackend();
    instance = new Log(primary, secondary);
  }
  return *instance;
}

#ifdef IS_MSVC
#include "log-msvc.cc"
#else
#include "log-fallback.cc"
#endif
