//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _CONPRX_DRIVER_HH
#define _CONPRX_DRIVER_HH

#include "c/stdc.h"

#include "plankton-inl.hh"

namespace conprx {

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

  // Returns a seed representing this handle, allocated using the given factory.
  plankton::Variant to_seed(plankton::Factory *factory);

private:
  static Handle *new_instance(plankton::Variant header, plankton::Factory *factory);
  void init(plankton::Seed payload, plankton::Factory *factory);

  // This must match the invalid handle value from windows.
  static const int64_t kInvalidHandleValue = -1;

  static plankton::SeedType<Handle> kSeedType;
  int64_t id_;
};

} // namespace conprx

#endif // _CONPRX_DRIVER_HH
