//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Logging. Logging is important since failures can be severe, impacting all
/// the user's executables, and because the agent disables eagerly and if it
/// does it's important to be able to debug why.

#ifndef _LOG
#define _LOG

#include "c/stdc.h"

#include <stdarg.h>

class LogBackend;

// State associated with logging.
class Log {
public:
  // The different log levels. We have to use these short awkward names because
  // windows.h defines the names they should have had.
  enum Type {
    ERR = 0x0001,
    WRN = 0x0002,
    INF = 0x0004,
    DBG = 0x0008
  };

  Log(LogBackend *primary, LogBackend *secondary);

  // Record a log event.
  void record(Type type, const char *file, int line, const char *fmt, ...);

  // Returns a debug log event.
  void record_debug(const char *file, int line, const char *fmt, ...);

  // Record a debug log event in the fallback log.
  void record_debug_fallback(const char *file, int line, const char *fmt, ...);

  // Sets whether debug log output is enabled or ignored.
  void set_debug_log_enabled(bool value);

public:
  // Ignore debug log messages?
  bool enable_debug_log_;

  // The current primary backend.
  LogBackend *primary_;
  LogBackend &primary() { return *primary_; }

  // The current secondary log. The secondary log is intended to always be
  // available such that it can be used even when, for whatever reason, the
  // primary isn't working or isn't working yet.
  LogBackend *secondary_;
  LogBackend &secondary() { return *secondary_; }

  // Returns the singleton log instance, creating it if necessary.
  static Log &get();
};

// A log backend is the place log output ends up. It's a separate abstraction
// because it's convenient to be able to switch between different backends
// without affecting the state that controls how logging works.
class LogBackend {
public:
  virtual ~LogBackend();

  // Record a log event.
  virtual void vrecord(Log::Type type, const char *file, int line, const char *fmt,
      va_list args) = 0;

  // Returns this platform's default log backend.
  static LogBackend *get_default();
};

// Log backend that dumps everything to stderr. Since the agent messes with
// stderr you typically rarely want to use this.
class StderrLogBackend : public LogBackend {
public:
  virtual void vrecord(Log::Type type, const char *file, int line,
      const char *fmt, va_list args);
};

// Log an error.
#define LOG_ERROR(FMT, ...) Log::get().record(Log::ERR, __FILE__, __LINE__, FMT, ##__VA_ARGS__)

// Log a warning.
#define LOG_WARNING(FMT, ...) Log::get().record(Log::WRN, __FILE__, __LINE__, FMT, ##__VA_ARGS__)

// Log information.
#define LOG_INFO(FMT, ...) Log::get().record(Log::INF, __FILE__, __LINE__, FMT, ##__VA_ARGS__)

// Conditionally log debug information.
#define LOG_DEBUG(FMT, ...) Log::get().record_debug( __FILE__, __LINE__, FMT, ##__VA_ARGS__)

// Logs a message on the fallback log. Useful when the primary log may not
// yet be operational.
#define FLOG_DEBUG(FMT, ...) Log::get().record_debug_fallback(__FILE__, __LINE__, FMT, ##__VA_ARGS__)

#endif // _LOG
