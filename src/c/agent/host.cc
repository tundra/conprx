//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "host.hh"
#include "sync/process.hh"
#include "async/promise-inl.hh"

BEGIN_C_INCLUDES
#include "utils/string-inl.h"
#include "utils/log.h"
END_C_INCLUDES

using namespace tclib;

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr, "Usage: host <library> <command ...>\n");
    return 1;
  }
  utf8_t library = new_c_string(argv[1]);
  utf8_t command = new_c_string(argv[2]);

  int skip_count = 3;
  int new_argc = argc - skip_count;
  utf8_t *new_argv = new utf8_t[new_argc];
  for (int i = 0; i < new_argc; i++)
    new_argv[i] = new_c_string(argv[skip_count + i]);

  HEST("host: starting process %s", command.chars);
  NativeProcess process;
  process.set_flags(pfStartSuspendedOnWindows);
  if (!process.start(command, new_argc, new_argv))
    LOG_ERROR("Failed to start %s.", command.chars);
  if (!process.inject_library(library))
    LOG_ERROR("Failed to inject %s.", library.chars);
  if (!process.resume())
    LOG_ERROR("Failed to resume %s.", library.chars);
  ProcessWaitIop wait(&process, o0());
  if (!wait.execute())
    LOG_ERROR("Failed to wait for %s.", command.chars);
  fprintf(stderr, "--- host: process done ---\n");
  return process.exit_code().peek_value(1);
}
