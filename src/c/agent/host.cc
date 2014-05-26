//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Shared console api implementation.

#include "host.hh"
#include "subprocess.hh"

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: host <library> <command>\n");
    return 1;
  }
  char *library = argv[1];
  char *command = argv[2];
  char *const args[2] = {command, NULL};
  Subprocess *process = Subprocess::create(library, command, args);
  if (!process->start())
    printf("Failed to start process.");
  if (!process->join())
    printf("Failed to join process.");
  delete process;
}
