//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Abstraction for starting subprocesses. Used by the proxy host.

#ifndef _SUBPROCESS
#define _SUBPROCESS

#include "utils/types.hh"

class Subprocess {
public:
  Subprocess(const char *library, const char *cpmmand, char *const *arguments);

  virtual ~Subprocess();

  // Creates and returns a new subprocess instance for running the given
  // command with the given shared library injected into it.
  static Subprocess *create(const char *library, const char *command,
      char *const *arguments);

  // Attempts to start this subprocess.
  virtual bool start() = 0;

  // Wait for the running subprocess to terminate.
  virtual bool join() = 0;

protected:
  // The library to inject when running the command.
  const char *library_;

  // The executable command.
  const char *command_;

  // The NULL-terminated array of arguments.
  char *const *arguments_;
};

#endif // _SUBPROCESS
