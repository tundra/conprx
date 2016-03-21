//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _CONPRX_DRIVER_HH
#define _CONPRX_DRIVER_HH

#include "c/stdc.h"

#include "plankton-inl.hh"

BEGIN_C_INCLUDES
#include "utils/string.h"
END_C_INCLUDES

namespace conprx {

// These are the extra messages understood by the console driver that don't
// correspond directly to console functions.
#define FOR_EACH_REMOTE_MESSAGE(F)                                             \
  F(echo,               (Variant value),        (value))                       \
  F(is_handle,          (Variant value),        (value))                       \
  F(raise_error,        (int64_t last_error),   (last_error))                  \
  F(poke_backend,       (Variant value),        (value))

enum DriverFrontendType {
  // The native frontend on this platform; only works on windows obviously.
  dfNative,
  // Dummy frontend that keeps everything in memory.
  dfDummy,
  // Simulating frontend that interacts through the backend. Requires the fake
  // agent to be present to simulate through.
  dfSimulating
};

// Utilities that don't fit anywhere else.
class ConsoleProxy {
public:
  // Returns a singleton type registry that holds all the types relevant to the
  // console proxy.
  static plankton::TypeRegistry *registry();
};

// An object that indicates a problem interacting with the console api.
class ConsoleError {
public:
  ConsoleError() : last_error_(0) { }
  ConsoleError(int64_t last_error) : last_error_(last_error) { }

  static plankton::SeedType<ConsoleError> *seed_type() { return &kSeedType; }

  int64_t last_error() { return last_error_; }

private:
  template <typename T> friend class plankton::DefaultSeedType;

  static ConsoleError *new_instance(plankton::Variant header, plankton::Factory *factory);
  plankton::Variant to_seed(plankton::Factory *factory);
  void init(plankton::Seed payload, plankton::Factory *factory);
  static plankton::DefaultSeedType<ConsoleError> kSeedType;
  int64_t last_error_;
};

} // namespace conprx

#endif // _CONPRX_DRIVER_HH
