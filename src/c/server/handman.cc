//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "server/handman.hh"

BEGIN_C_INCLUDES
#include "utils/log.h"
END_C_INCLUDES

using namespace conprx;

void HandleManager::register_std_handle(standard_handle_t type, Handle handle) {
  get_or_create_shadow(handle, true);
  // TODO: set the handle's initial mode.
}

void HandleManager::set_handle_mode(Handle handle, uint32_t mode) {
  HandleShadow *shadow = get_or_create_shadow(handle, true);
  shadow->set_mode(mode);
}

HandleShadow *HandleManager::get_or_create_shadow(Handle handle, bool create_if_missing) {
  address_arith_t key = reinterpret_cast<address_arith_t>(handle.ptr());
  platform_hash_map<address_arith_t, HandleShadow>::iterator iter = handles_.find(key);
  if (iter == handles_.end()) {
    if (create_if_missing) {
      return &handles_[key];
    } else {
      return NULL;
    }
  } else {
    return &iter->second;
  }
}
