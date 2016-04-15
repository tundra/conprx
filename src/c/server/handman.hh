//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _CONPRX_SERVER_HANDMAN
#define _CONPRX_SERVER_HANDMAN

#include "utils/types.hh"
#include "share/protocol.hh"

namespace conprx {

class HandleInfo {
public:
  HandleInfo() : mode_(0) { }
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
  HandleInfo *get_or_create_info(Handle handle, bool create_if_missing);

private:

  platform_hash_map<address_arith_t, HandleInfo> handles_;
};

} // namespace conprx

#endif // _CONPRX_SERVER_HANDMAN
