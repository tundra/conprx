//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Logging. Logging is important since failures can be severe, impacting all
/// the user's executables, and because the agent disables eagerly and if it
/// does it's important to be able to debug why.

#ifndef _LOG
#define _LOG

#include "stdc.h"

// State associated with logging.
class Log {
public:
  // The different log levels. We have to use these short awkward names because
  // windows.h defines the names they should have had.
  enum Type {
    ERR = 0x0001,
    WRN = 0x0002,
    INF = 0x0004
  };

  virtual ~Log();

  // Record a log event.
  virtual void record(Type type, const char *file, int line, const char *fmt, ...) = 0;

public:
  // Returns the singleton log instance, creating it if necessary.
  static Log &get();
};

// Log an error.
#define LOG_ERROR(FMT, ...) Log::get().record(Log::ERR, __FILE__, __LINE__, FMT, ##__VA_ARGS__)

// Log a warning.
#define LOG_WARNING(FMT, ...) Log::get().record(Log::WRN, __FILE__, __LINE__, FMT, ##__VA_ARGS__)

// Log information.
#define LOG_INFO(FMT, ...) Log::get().record(Log::INF, __FILE__, __LINE__, FMT, ##__VA_ARGS__)

#endif // _LOG
