//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _CONPRX_DRIVER_HH
#define _CONPRX_DRIVER_HH

#include "c/stdc.h"

#include "plankton-inl.hh"

namespace conprx {

// These are the extra messages understood by the console driver that don't
// correspond directly to console functions.
#define FOR_EACH_REMOTE_MESSAGE(F)                                             \
  F(echo,               (Variant value),        (value))                       \
  F(is_handle,          (Variant value),        (value))                       \
  F(raise_error,        (int64_t last_error),   (last_error))

// Utilities that don't fit anywhere else.
class ConsoleProxy {
public:
  // Returns a singleton type registry that holds all the types relevant to the
  // console proxy.
  static plankton::TypeRegistry *registry();
};

// A wrapper around a native handle. A handle can either be valid and have a
// non-negative id or invalid.
class Handle {
public:
  // Initializes an invalid handle.
  Handle() : id_(kInvalidHandleValue) { }

  // Initializes a handle with the given id.
  explicit Handle(int64_t id) : id_(id) { }

  // Initializes a handle with an id corresponding to the given pointer.
  explicit Handle(void *raw) : id_(reinterpret_cast<int64_t>(raw)) { }

  // Is this a valid handle?
  bool is_valid() { return id_ != kInvalidHandleValue; }

  // The seed type for handles.
  static plankton::SeedType<Handle> *seed_type() { return &kSeedType; }

  // Returns an invalid handle.
  static Handle invalid() { return Handle(); }

  int64_t id() { return id_; }

  void *ptr() { return reinterpret_cast<void*>(id_); }

private:
  template <typename T> friend class plankton::DefaultSeedType;

  plankton::Variant to_seed(plankton::Factory *factory);
  static Handle *new_instance(plankton::Variant header, plankton::Factory *factory);
  void init(plankton::Seed payload, plankton::Factory *factory);

  // This must match the invalid handle value from windows.
  static const int64_t kInvalidHandleValue = -1;

  static plankton::DefaultSeedType<Handle> kSeedType;
  int64_t id_;
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
