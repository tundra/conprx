//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _CONPRX_SERVER_HANDMAN
#define _CONPRX_SERVER_HANDMAN

#include "utils/types.hh"
#include "share/protocol.hh"

namespace conprx {

// Server-side information about a handle. It's a shadow because it's not
// authoritative -- that resides in the native windows handle system -- but it
// should reflect that info.
class HandleShadow {
public:
  HandleShadow() : mode_(0) { }
  uint32_t mode() { return mode_; }
  void set_mode(uint32_t value) { mode_ = value; }
private:
  uint32_t mode_;
};

// A handle manager keeps track of the active handles in a process.
class HandleManager {
public:
  // Notifies this manager that the given handle is that process' representation
  // of the indicated standard handle type.
  void register_std_handle(standard_handle_t type, Handle handle);

  void set_handle_mode(Handle handle, uint32_t mode);

  // Returns a pointer to the info struct that describes the given handle. If
  // no mapping is present and create_if_missing is false NULL is returned,
  // otherwise a new handle info is created and bounds as the mapping for the
  // given handle. The result is only valid until the next call to this method
  // that creates a handle info so watch out.
  HandleShadow *get_or_create_shadow(Handle handle, bool create_if_missing);

private:

  platform_hash_map<address_arith_t, HandleShadow> handles_;
};

} // namespace conprx

#endif // _CONPRX_SERVER_HANDMAN
