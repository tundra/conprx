//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "host.hh"
#include "launch.hh"

BEGIN_C_INCLUDES
#include "utils/string-inl.h"
#include "utils/log.h"
END_C_INCLUDES

using namespace tclib;
using namespace conprx;

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

  Launcher launcher;
  if (!launcher.start(command, new_argc, new_argv, library))
    return 1;

  int exit_code = 0;
  if (!launcher.join(&exit_code))
    return 1;

  return exit_code;
}
