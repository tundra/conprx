//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent/conconn.hh"
#include "io/stream.hh"
#include "socket.hh"
#include "sync/injectee.hh"
#include "utils/log.hh"
#include "utils/types.hh"

BEGIN_C_INCLUDES
#include "utils/strbuf.h"
END_C_INCLUDES

using namespace conprx;
using namespace tclib;

// Windows-specific console agent code.
class WindowsConsoleAgent : public ConsoleAgent {
public:
  WindowsConsoleAgent();
  virtual void default_destroy() { default_delete_concrete(this); }

  // Called when the dll process attach notification is received.
  static bool dll_process_attach();

  static bool dll_process_detach();

  virtual fat_bool_t install_agent_platform();

  virtual fat_bool_t uninstall_agent_platform();

  fat_bool_t connect(blob_t data_in, blob_t data_out, int *last_error_out);

  static WindowsConsoleAgent *get() { return instance_; }

  virtual ConsoleConnector *connector() { return *connector_; }

private:
  def_ref_t<ConsoleConnector> connector_;

  lpc::Interceptor interceptor_;
  lpc::Interceptor *interceptor() { return &interceptor_; }

  def_ref_t<InStream> agent_in_;
  InStream *agent_in() { return *agent_in_; }
  def_ref_t<OutStream> agent_out_;
  OutStream *agent_out() { return *agent_out_; }

  static WindowsConsoleAgent *instance() { return instance_; }
  static WindowsConsoleAgent *instance_;
};

WindowsConsoleAgent *WindowsConsoleAgent::instance_ = NULL;

WindowsConsoleAgent::WindowsConsoleAgent()
  : interceptor_(new_callback(&ConsoleAgent::on_message, static_cast<ConsoleAgent*>(this))) {
}

bool WindowsConsoleAgent::dll_process_attach() {
  instance_ = new WindowsConsoleAgent();
  return true;
}

bool WindowsConsoleAgent::dll_process_detach() {
  if (instance_ == NULL)
    return true;
  fat_bool_t uninstalled = instance()->uninstall_agent();
  if (!uninstalled)
    LOG_WARN("Failed to uninstall agent (%0x04x:%i)", fat_bool_file(uninstalled),
        fat_bool_line(uninstalled));
  delete instance_;
  instance_ = NULL;
  return true;
}

fat_bool_t WindowsConsoleAgent::install_agent_platform() {
  return interceptor()->install();
}

fat_bool_t WindowsConsoleAgent::uninstall_agent_platform() {
  return interceptor()->uninstall();
}

fat_bool_t WindowsConsoleAgent::connect(blob_t data_in, blob_t data_out,
    int *last_error_out) {
  // Validate that the input data looks sane.
  if (data_in.size < sizeof(connect_data_t))
    return F_FALSE;
  connect_data_t *connect_data = static_cast<connect_data_t*>(data_in.start);
  if (connect_data->magic != kConnectDataMagic)
    return F_FALSE;

  // Create a connection to the parent process so we can pass back error and
  // status messages.
  handle_t parent_process = OpenProcess(PROCESS_DUP_HANDLE, false,
      connect_data->parent_process_id);
  if (parent_process == NULL) {
    *last_error_out = GetLastError();
    return F_FALSE;
  }

  handle_t agent_in_handle = INVALID_HANDLE_VALUE;
  if (!DuplicateHandle(parent_process, connect_data->agent_in_handle,
      GetCurrentProcess(), &agent_in_handle, GENERIC_READ, false, 0)) {
    *last_error_out = GetLastError();
    return F_FALSE;
  }
  agent_in_ = tclib::InOutStream::from_raw_handle(agent_in_handle);

  handle_t agent_out_handle = INVALID_HANDLE_VALUE;
  if (!DuplicateHandle(parent_process, connect_data->agent_out_handle,
      GetCurrentProcess(), &agent_out_handle, GENERIC_WRITE, false, 0)) {
    *last_error_out = GetLastError();
    return F_FALSE;
  }
  agent_out_ = tclib::InOutStream::from_raw_handle(agent_out_handle);

  F_TRY(install_agent(agent_in(), agent_out()));

  connector_ = PrpcConsoleConnector::create(owner()->socket(), owner()->input());
  return F_TRUE;
}

address_t ConsoleAgent::get_console_function_address(cstr_t name) {
  module_t kernel32 = GetModuleHandle(TEXT("kernel32.dll"));
  return Code::upcast(GetProcAddress(kernel32, name));
}

// Windows-specific implementation of the options. The reason to make this into
// a class is such that it has access to the protected fields in Options.
class WindowsOptions : public Options {
public:
  WindowsOptions();

  // Reads any options from environment variables.
  void override_from_environment();

  // Attempts to read the given environment variable and if present sets the
  // field to its value, otherwise doesn't change it.
  void override_bool_from_environment(bool *field, cstr_t name);
};

WindowsOptions::WindowsOptions() {
  // Override with values from the environment.
  override_from_environment();
}

void WindowsOptions::override_from_environment() {
#define __EMIT_ENV_READ__(name, defawlt, Name, NAME)                           \
  override_bool_from_environment(&name##_, TEXT("CONSOLE_AGENT_" NAME));
  FOR_EACH_BOOL_OPTION(__EMIT_ENV_READ__)
#undef __EMIT_ENV_READ__
}

void WindowsOptions::override_bool_from_environment(bool *field, cstr_t name) {
  char_t buffer[256];
  if (GetEnvironmentVariable(name, buffer, 256) == 0)
    // There was no environment variable with that name.
    return;
  if (strcmp(buffer, "1") == 0) {
    *field = true;
  } else if (strcmp(buffer, "0") == 0) {
    *field = false;
  }
}

Options &Options::get() {
  static WindowsOptions *options = NULL;
  if (options == NULL)
    options = new WindowsOptions();
  return *options;
}

bool APIENTRY DllMain(module_t module, dword_t reason, void *) {
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      return WindowsConsoleAgent::dll_process_attach();
    case DLL_PROCESS_DETACH:
      return WindowsConsoleAgent::dll_process_detach();
  }
  return true;
}

CONNECTOR_IMPL(ConprxAgentConnect, data_in, data_out) {
  int last_error = 0;
  fat_bool_t result = WindowsConsoleAgent::get()->connect(data_in, data_out,
      &last_error);
  return result
      ? CONNECT_SUCCEEDED_RESULT()
      : CONNECT_FAILED_RESULT(fat_bool_file(result), fat_bool_line(result),
          last_error);
}
