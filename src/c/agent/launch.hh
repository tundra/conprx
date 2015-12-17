//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _CONPRX_AGENT_LAUNCH
#define _CONPRX_AGENT_LAUNCH

#include "sync/process.hh"
#include "sync/pipe.hh"
#include "sync/thread.hh"

namespace conprx {

class Launcher {
public:
  Launcher();
  ~Launcher();
  bool start(utf8_t command, size_t argc, utf8_t *argv, utf8_t agent_dll);
  bool join(int *exit_code);
  bool start_monitor();
private:
  void *run_monitor();

  void handle_child_message(utf8_t message);

  tclib::NativeProcess process_;
  tclib::NativePipe log_;
  tclib::NativeThread monitor_;
};

} // namespace conprx

#endif // _CONPRX_AGENT_LAUNCH
