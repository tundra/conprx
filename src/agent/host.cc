//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Shared console api implementation.

#include "host.hh"
#include "subprocess.hh"

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: host <command>\n");
    return 1;
  }
  char *const args[2] = {argv[0], NULL};
  Subprocess *process = Subprocess::create(argv[1], args);
  if (!process->start())
    printf("Failed to start process.");
  if (!process->join())
    printf("Failed to join process.");
  delete process;
}
