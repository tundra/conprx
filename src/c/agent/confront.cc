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

#include "confront-dummy.cc"

#include "confront-simul.cc"

#ifdef IS_MSVC
#  include "confront-msvc.cc"
#endif
