//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent.hh"
#include "binpatch.hh"
#include "conapi.hh"
#include "utils/log.hh"

Console *ConsoleAgent::delegate = NULL;

bool ConsoleAgent::install(Console &installed_delegate) {
  if (delegate != NULL) {
    LOG_ERROR("Console agent has already been installed");
    return false;
  }
  delegate = &installed_delegate;
  address_t write_console_a = get_console_function_address(TEXT("WriteConsoleA"));
  PatchRequest patch(write_console_a, FUNCAST(write_console_a_bridge), 0);
  MemoryManager &memman = MemoryManager::get();
  if (!memman.ensure_initialized())
    return false;
  if (!patch.is_tentatively_possible()) {
    LOG_ERROR("Patch is impossible");
    return false;
  }
  if (!patch.apply(memman)) {
    LOG_ERROR("Failed to patch");
    return false;
  }
  return true;
}

#ifdef IS_MSVC
#include "agent-msvc.cc"
#else
#include "agent-posix.cc"
#endif
