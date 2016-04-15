//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "server/handman.hh"

BEGIN_C_INCLUDES
#include "utils/log.h"
END_C_INCLUDES

using namespace conprx;

void HandleManager::register_std_handle(standard_handle_t type, Handle handle) {
  HandleInfo *info = get_or_create_info(handle, true);
  if (type == kStdInputHandle) {
    info->set_mode(1);
  } else {
    info->set_mode(2);
  }
}

void HandleManager::set_handle_mode(Handle handle, uint32_t mode) {
  HandleInfo *info = get_or_create_info(handle, true);
  info->set_mode(mode);
}

HandleInfo *HandleManager::get_or_create_info(Handle handle, bool create_if_missing) {
  address_arith_t key = reinterpret_cast<address_arith_t>(handle.ptr());
  platform_hash_map<address_arith_t, HandleInfo>::iterator iter = handles_.find(key);
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
