//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

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

  // Returns true iff the current process is hardcoded blacklisted. Returns true
  // if this process should not be patched under any circumstances, no matter
  // what options are set.
  static bool is_process_hard_blacklisted();

  // Called when the dll process attach notification is received.
  static bool dll_process_attach();

  static bool dll_process_detach();

  virtual fat_bool_t install_agent_platform();

  fat_bool_t connect(blob_t data_in, blob_t data_out, int *last_error_out);

  static WindowsConsoleAgent *get() { return instance_; }

private:
  def_ref_t<InStream> agent_in_;
  InStream *agent_in() { return *agent_in_; }
  def_ref_t<OutStream> agent_out_;
  OutStream *agent_out() { return *agent_out_; }

  // A list of executable names we refuse to patch.
  static const size_t kBlacklistSize = 4;
  static cstr_t kBlacklist[kBlacklistSize];

  static WindowsConsoleAgent *instance_;
};

WindowsConsoleAgent *WindowsConsoleAgent::instance_ = NULL;

cstr_t WindowsConsoleAgent::kBlacklist[kBlacklistSize] = {
    // These are internal windows services that become unhappy if we try to
    // patch them. Also it serves no purpose.
    TEXT("conhost.exe"),
    TEXT("svchost.exe"),
    TEXT("WerFault.exe"),
    // Leave the registry editor untouched since, in the worst case, you may
    // need it to disable the agent if it becomes broken.
    TEXT("regedit.exe")
};

WindowsConsoleAgent::WindowsConsoleAgent() { }

bool WindowsConsoleAgent::dll_process_attach() {
  instance_ = new WindowsConsoleAgent();
  return true;
}

bool WindowsConsoleAgent::dll_process_detach() {
  delete instance_;
  instance_ = NULL;
  return true;
}

fat_bool_t WindowsConsoleAgent::install_agent_platform() {
  return F_TRUE;
}

bool WindowsConsoleAgent::is_process_hard_blacklisted() {
  // It is not safe to log at this point since this is run in *every* process
  // the system has, including processes that are too fundamental to support
  // the stuff we need for logging.
  char_t buffer[1024];
  // Passing NULL means the current process. Now we know.
  size_t length = GetModuleFileName(NULL, buffer, 1024);
  if (length == 0)
    // If we can't determine the executable's name we better not try patching
    // it.
    return true;
  // Scan through the blacklist to see if this process matches anything. I
  // imaging this will have to get smarter, possibly even support some amount
  // of external configuration and/or wildcard matching. Laters.
  Vector<char_t> executable(buffer, length);
  for (size_t i = 0; i < kBlacklistSize; i++) {
    Vector<char_t> entry(const_cast<char_t*>(kBlacklist[i]), lstrlen(kBlacklist[i]));
    if (executable.has_suffix(entry))
      return true;
  }
  return false;
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

  return install_agent(agent_in(), agent_out());
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
      : CONNECT_FAILED_RESULT(result.file_id(), result.line(), last_error);
}
