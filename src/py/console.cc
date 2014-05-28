// Copyright 2014 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "console.hh"
#include "pyth-inl.hh"

using namespace condrv;

#ifdef IS_GCC
#  include "console-posix.cc"
#else
#  include "console-msvc.cc"
#endif
