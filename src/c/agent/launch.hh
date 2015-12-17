//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _CONPRX_AGENT_LAUNCH
#define _CONPRX_AGENT_LAUNCH

#include "sync/process.hh"
#include "sync/pipe.hh"
#include "sync/thread.hh"

namespace conprx {

// Encapsulates launching a child process and injecting the agent dll.
class Launcher {
public:
  Launcher();
  ~Launcher();

  // Start the process running.
  bool start(utf8_t command, size_t argc, utf8_t *argv, utf8_t agent_dll);

  // Wait for the process to exit, storing the exit code in the out param.
  bool join(int *exit_code_out);

private:
  // Starts up the child output monitor.
  bool start_monitor();

  // Entry-point for the monitor.
  void *run_monitor();

  // Called with every new message received by the monitor.
  void handle_child_message(utf8_t message);

  tclib::NativeProcess process_;
  tclib::NativePipe log_;
  tclib::NativeThread monitor_;
};

} // namespace conprx

#endif // _CONPRX_AGENT_LAUNCH
