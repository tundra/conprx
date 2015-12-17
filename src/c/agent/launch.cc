//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent/agent.hh"
#include "agent/launch.hh"
#include "async/promise-inl.hh"

BEGIN_C_INCLUDES
#include "utils/log.h"
#include "utils/string-inl.h"
END_C_INCLUDES

using namespace tclib;
using namespace conprx;

Launcher::Launcher() {
  process_.set_flags(pfStartSuspendedOnWindows);
}

Launcher::~Launcher() {
}

bool Launcher::start(utf8_t command, size_t argc, utf8_t *argv, utf8_t agent_dll) {
  if (!log_.open(NativePipe::pfDefault))
    return false;

  // Start monitoring immediately before launching.
  if (!start_monitor())
    return false;

  HEST("host: starting process %s", command.chars);
  if (!process_.start(command, argc, argv)) {
    LOG_ERROR("Failed to start %s.", command.chars);
    return false;
  }
  connect_data_t data;
  data.magic = ConsoleAgent::kConnectDataMagic;
  data.parent_process_id = IF_MSVC(GetCurrentProcessId(), 0);
  data.parent_logout_handle = log_.out()->to_raw_handle();
  utf8_t connect_name = new_c_string("ConprxAgentConnect");
  blob_t blob_in = blob_new(&data, sizeof(data));
  if (!process_.inject_library(agent_dll, connect_name, blob_in, blob_empty())) {
    LOG_ERROR("Failed to inject %s.", agent_dll.chars);
    return false;
  }
  if (!process_.resume()) {
    LOG_ERROR("Failed to resume %s.", command.chars);
    return false;
  }
  return true;
}

bool Launcher::join(int *exit_code_out) {
  ProcessWaitIop wait(&process_, o0());
  if (!wait.execute()) {
    LOG_ERROR("Failed to wait for process.");
    return false;
  }
  fprintf(stderr, "--- host: process done ---\n");
  *exit_code_out = process_.exit_code().peek_value(1);
  return true;
}

bool Launcher::start_monitor() {
  monitor_.set_callback(new_callback(&Launcher::run_monitor, this));
  return monitor_.start();
}

void *Launcher::run_monitor() {
  uint32_t size = 0;
  ReadIop size_read(log_.in(), &size, sizeof(size));
  while (true) {
    bool at_eof = true;
    if (size_read.execute() && (size_read.bytes_read() == sizeof(size))) {
      char *buf = new char[size + 1];
      ReadIop data_read(log_.in(), buf, size);
      data_read.execute();
      buf[size] = '\0';
      handle_child_message(new_string(buf, size));
      delete[] buf;
      at_eof = data_read.at_eof();
    }
    if (at_eof) {
      return NULL;
    } else {
      size_read.recycle();
    }
  }
}

void Launcher::handle_child_message(utf8_t message) {
  WARN("Child message: %s", message.chars);
}
