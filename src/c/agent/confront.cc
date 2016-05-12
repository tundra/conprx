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

fat_bool_t ConsolePlatform::create_native_process(utf8_t executable, size_t argc,
    utf8_t *argv, pass_def_ref_t<NativeProcess> *process_out) {
  pass_def_ref_t<NativeProcess> process = new (kDefaultAlloc) NativeProcess();
  fat_bool_t started = process.peek()->start(executable, argc, argv);
  if (!started) {
    def_ref_t<NativeProcess> arrival = process;
    return F_FALSE;
  }
  *process_out = process;
  return F_TRUE;
}

#ifdef IS_MSVC
#  include "confront-msvc.cc"
#else
#  include "confront-posix.cc"
#endif
