//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Posix implementation of subprocesses.

#include <unistd.h>
#include <sys/wait.h>

#include "utils/log.hh"

class PosixSubprocess : public Subprocess {
public:
  PosixSubprocess(const char *library, const char *command,
      char *const *arguments)
    : Subprocess(library, command, arguments)
    , child_pid_(-1) { }

  virtual bool start();
  virtual bool join();
private:
  pid_t child_pid_;
};

bool PosixSubprocess::start() {
  pid_t pid = fork();
  if (pid == -1) {
    LOG_ERROR("Failed to fork subprocess.");
    return false;
  }
  if (pid == 0) {
    execv(command_, arguments_);
  } else {
    child_pid_ = pid;
  }
  return true;
}

bool PosixSubprocess::join() {
  int status = 0;
  if (waitpid(child_pid_, &status, 0) == 0)
    return false;
  return true;
}

Subprocess *Subprocess::create(const char *library, const char *command,
    char *const *arguments) {
  return new PosixSubprocess(library, command, arguments);
}
