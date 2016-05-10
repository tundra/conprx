//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Shared console api implementation.

#include "confront.hh"

#include "utils/string.hh"
#include "plankton-inl.hh"

BEGIN_C_INCLUDES
#include "utils/log.h"
END_C_INCLUDES

using namespace conprx;
using namespace plankton;
using namespace tclib;

#include "confront-dummy.cc"

#include "confront-simul.cc"

pass_def_ref_t<NativeProcess> ConsolePlatform::create_native_process(utf8_t executable,
    size_t argc, utf8_t *argv) {
  NativeProcess *result = new (kDefaultAlloc) NativeProcess();
  F_LOG_FALSE(result->start(executable, argc, argv));
  return result;
}

#ifdef IS_MSVC
#  include "confront-msvc.cc"
#else
#  include "confront-posix.cc"
#endif
