//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Logging. Logging is important since failures can be severe, impacting all
/// the user's executables, and because the agent disables eagerly and if it
/// does it's important to be able to debug why.

#ifndef _LOG
#define _LOG

// Log a message.
void log(const char *file, int line, const char *fmt, ...);

#define ERROR(FMT, ...) log(__FILE__, __LINE__, FMT, ##__VA_ARGS__)

#endif // _LOG
