//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Shared subprocess implementation.

#include "subprocess.hh"

Subprocess::Subprocess(const char *cmd, char *const *argv)
  : cmd_(cmd)
  , argv_(argv) { }

Subprocess::~Subprocess() { }

#ifdef IS_GCC
#include "subprocess-posix.cc"
#else
#include "subprocess-msvc.cc"
#endif
