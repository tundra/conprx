//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _CONPRX_DRIVER_HH
#define _CONPRX_DRIVER_HH

#include "c/stdc.h"

#include "plankton-inl.hh"
#include "share/protocol.hh"

BEGIN_C_INCLUDES
#include "utils/string.h"
END_C_INCLUDES

namespace conprx {

// These are the extra messages understood by the console driver that don't
// correspond directly to console functions.
#define FOR_EACH_REMOTE_MESSAGE(F)                                             \
  F(echo,               (Variant value),        (value))                       \
  F(is_handle,          (Variant value),        (value))                       \
  F(raise_error,        (NtStatus last_error),  (last_error))                  \
  F(poke_backend,       (Variant value),        (value))                       \
  F(create_child,       (Variant args),         (args))

enum DriverFrontendType {
  // The native frontend on this platform; only works on windows obviously.
  dfNative,
  // Dummy frontend that keeps everything in memory.
  dfDummy,
  // Simulating frontend that interacts through the backend. Requires the fake
  // agent to be present to simulate through.
  dfSimulating
};

// An object that indicates a problem interacting with the console api.
class ConsoleError {
public:
  ConsoleError() : last_error_(NtStatus::success()) { }
  ConsoleError(NtStatus last_error) : last_error_(last_error) { }

  static plankton::SeedType<ConsoleError> *seed_type() { return &kSeedType; }

  // The last_error status that was active when this error occurred.
  NtStatus last_error() { return last_error_; }

  // The last_error's status code.
  uint32_t code() { return last_error().code(); }

private:
  template <typename T> friend class plankton::DefaultSeedType;

  static ConsoleError *new_instance(plankton::Variant header, plankton::Factory *factory);
  plankton::Variant to_seed(plankton::Factory *factory);
  void init(plankton::Seed payload, plankton::Factory *factory);
  static plankton::DefaultSeedType<ConsoleError> kSeedType;
  NtStatus last_error_;
};

} // namespace conprx

#endif // _CONPRX_DRIVER_HH
