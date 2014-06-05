//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "utils/types.hh"

using namespace conprx;

// Windows-specific console agent code.
class WindowsConsoleAgent : public ConsoleAgent {
public:
  // Returns true iff the current process is blacklisted. Returns true if this
  // process should not be patched under any circumstances.
  static bool is_process_blacklisted();

  // Called when the dll process attach notification is received.
  static bool dll_process_attach();

private:
  // A list of executable names we refuse to patch.
  static const size_t kBlacklistSize = 3;
  static c_str_t kBlacklist[kBlacklistSize];
};

c_str_t WindowsConsoleAgent::kBlacklist[kBlacklistSize] = {
    // These are internal windows services that become unhappy if we try to
    // patch them. Also it serves no purpose.
    TEXT("conhost.exe"),
    TEXT("svchost.exe"),
    // Leave the registry editor untouched since, in the worst case, you may
    // need it to disable the agent if it becomes broken.
    TEXT("regedit.exe")
};

bool WindowsConsoleAgent::dll_process_attach() {
  if (is_process_blacklisted())
    return true;
  LoggingConsole *logger = new LoggingConsole(NULL);
  Console *original = NULL;
  if (!install(*logger, &original))
    return false;
  logger->set_delegate(original);
  return true;
}

bool WindowsConsoleAgent::is_process_blacklisted() {
  c_char_t buffer[1024];
  // Passing NULL means the current process. Now we know.
  size_t length = GetModuleFileName(NULL, buffer, 1024);
  if (length == 0)
    // If we can't determine the executable's name we better not try patching
    // it.
    return true;
  // Scan through the blacklist to see if this process matches anything. I
  // imaging this will have to get smarter, possibly even support some amount
  // of external configuration and/or wildcard matching. Laters.
  Vector<c_char_t> executable(buffer, length);
  for (size_t i = 0; i < kBlacklistSize; i++) {
    Vector<c_char_t> entry(const_cast<c_char_t*>(kBlacklist[i]), lstrlen(kBlacklist[i]));
    if (executable.has_suffix(entry))
      return true;
  }
  return false;
}

address_t ConsoleAgent::get_console_function_address(c_str_t name) {
  module_t kernel32 = GetModuleHandle(TEXT("kernel32.dll"));
  return Code::upcast(GetProcAddress(kernel32, name));
}

// The registry key under which any options are stored. This doesn't have to
// exist but if it does we'll read from it.
#define OPTIONS_REGISTRY_KEY_NAME TEXT("Software\\Tundra\\Console Agent")

// Windows-specific implementation of the options. The reason to make this into
// a class is such that it has access to the protected fields in Options.
class WindowsOptions : public Options {
public:
  WindowsOptions();

  // Reads any options from the registry.
  void override_from_registry(hkey_t registry);

  // Attempts to read a value from the registry and if present sets the field
  // to its value.
  void override_bool_from_registry(bool *field, hkey_t registry, c_str_t name);

  // Reads any options from environment variables.
  void override_from_environment();

  // Attempts to read the given environment variable and if present sets the
  // field to its value, otherwise doesn't change it.
  void override_bool_from_environment(bool *field, c_str_t name);
};

WindowsOptions::WindowsOptions() {
  // First override with any global values from the registry.
  override_from_registry(HKEY_LOCAL_MACHINE);

  // Then override with any user specific values from the registry.
  override_from_registry(HKEY_CURRENT_USER);

  // Finally override with values from the environment.
  override_from_environment();
}

void WindowsOptions::override_from_registry(hkey_t registry) {
  // Try to open the registry key under which these values live. If it doesn't
  // exist there's no point in continuing.
  hkey_t hkey = 0;
  long ret = RegOpenKeyEx(registry, OPTIONS_REGISTRY_KEY_NAME, 0, KEY_READ, &hkey);
  if (ret != ERROR_SUCCESS)
    return;
#define __EMIT_REG_READ__(name, defawlt, Name, NAME)                           \
  override_bool_from_registry(&name##_, hkey, TEXT(Name));
  FOR_EACH_BOOL_OPTION(__EMIT_REG_READ__)
#undef __EMIT_REG_READ__
}

void WindowsOptions::override_bool_from_registry(bool *field, hkey_t hkey,
    c_str_t name) {
  // Read the relevant registry entry.
  dword_t type = 0;
  dword_t value = 0;
  dword_t size = sizeof(value);
  long ret = RegQueryValueEx(hkey, name, 0, &type, reinterpret_cast<byte_t*>(&value),
      &size);
  if (ret == ERROR_SUCCESS && type == REG_DWORD)
    // Only if it was present and had the right type do we use it.
    *field = !!value;
}

void WindowsOptions::override_from_environment() {
#define __EMIT_ENV_READ__(name, defawlt, Name, NAME)                           \
  override_bool_from_environment(&name##_, TEXT("CONSOLE_AGENT_" NAME));
  FOR_EACH_BOOL_OPTION(__EMIT_ENV_READ__)
#undef __EMIT_ENV_READ__
}

void WindowsOptions::override_bool_from_environment(bool *field, c_str_t name) {
  c_char_t buffer[256];
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
