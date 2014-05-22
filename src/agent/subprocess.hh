//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Abstraction for starting subprocesses. Used by the proxy host.

#ifndef _SUBPROCESS
#define _SUBPROCESS

#include "types.hh"

class Subprocess {
public:
  Subprocess(const char *cmd, char *const *argv);

  virtual ~Subprocess();

  // Creates and returns a new subprocess instance for running the given
  // command.
  static Subprocess *create(const char *cmd, char *const *argv);

  // Attempts to start this subprocess.
  virtual bool start() = 0;

  // Wait for the running subprocess to terminate.
  virtual bool join() = 0;

protected:
  // The executable command.
  const char *cmd_;

  // The NULL-terminated array of arguments.
  char *const *argv_;
};

#endif // _SUBPROCESS
