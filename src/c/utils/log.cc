//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "log.hh"

Log::~Log() { }

#ifdef IS_MSVC
#include "log-msvc.cc"
#else
#include "log-fallback.cc"
#endif
