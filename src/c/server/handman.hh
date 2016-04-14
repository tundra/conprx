//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _CONPRX_SERVER_HANDMAN
#define _CONPRX_SERVER_HANDMAN

#include "utils/types.hh"
#include "share/protocol.hh"

namespace conprx {

// A handle manager keeps track of the active handles in a process.
class HandleManager {
public:
  // Notifies this manager that the given handle is that process' representation
  // of the indicated standard handle type.
  void register_std_handle(standard_handle_t type, Handle handle);
};

} // namespace conprx

#endif // _CONPRX_SERVER_HANDMAN
