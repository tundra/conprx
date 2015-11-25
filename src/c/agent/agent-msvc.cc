//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "utils/types.hh"

using namespace conprx;

// Windows-specific console agent code.
class WindowsConsoleAgent : public ConsoleAgent {
public:
  // Returns true iff the current process is hardcoded blacklisted. Returns true
  // if this process should not be patched under any circumstances, no matter
  // what options are set.
  static bool is_process_hard_blacklisted();

  // Called when the dll process attach notification is received.
  static bool dll_process_attach();

private:
  // A list of executable names we refuse to patch.
  static const size_t kBlacklistSize = 4;
  static cstr_t kBlacklist[kBlacklistSize];
};

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

bool WindowsConsoleAgent::dll_process_attach() {
  // The blacklist is one way to protect against hosing the system completely
  // if there's a bug, and a more fundamental one than the options since we
  // use the blacklist to ensure that you can change the options using the
  // registry editor, for instance. Hence the blacklist check should be short
  // and sweet with few places for bugs to hide. And nothing nontrivial should
  // happen before it.
  if (is_process_hard_blacklisted())
    return true;

  // Then apply the options. This is the next step in ensuring that you can
  // interact with the system even if there are problems with the agent so
  // again as little nontrivial behavior that could harbor bugs should happen
  // before it.
  Options &options = Options::get();
  if (options.verbose_logging())
    Log::get().set_debug_log_enabled(true);
  if (!options.is_enabled())
    return true;

  MessageSink messages;

  // Okay at this point we can start with the actual work since we've made it
  // safely through the guards above.
  LoggingConsole *logger = new LoggingConsole(NULL);
  Console *original = NULL;
  if (!install(options, *logger, &original, &messages))
    return false;
  logger->set_delegate(original);

  return true;
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
  }
  return true;
}
