//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "host.hh"
#include "utils/log.hh"
#include "subprocess.hh"

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr, "Usage: host <library> <command ...>\n");
    return 1;
  }
  char *library = argv[1];
  char *command = argv[2];
  char *const *args = argv + 2;
  fprintf(stderr, "--- host: starting process ---\n");
  Subprocess *process = Subprocess::create(library, command, args);
  if (!process->start())
    LOG_ERROR("Failed to start process.");
  if (!process->join())
    LOG_ERROR("Failed to join process.");
  delete process;
  fprintf(stderr, "--- host: process done ---\n");
}
