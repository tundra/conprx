//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "subprocess.hh"
#include "utils/log.hh"

class WindowsSubprocess : public Subprocess {
public:
  WindowsSubprocess(const char *cmd, char *const *argv);

  virtual bool start();
  virtual bool join();

  // Returns a handle for the child process.
  handle_t process() { return child_info_.hProcess; }

  // Returns a handle for the main thread in the child process.
  handle_t thread() { return child_info_.hThread; }

  // Creates the child process without starting it running.
  bool start_suspended_child();

  // Starts the child process running.
  bool unsuspend_child();

private:
  PROCESS_INFORMATION child_info_;
};

WindowsSubprocess::WindowsSubprocess(const char *cmd, char *const *argv)
  : Subprocess(cmd, argv) {
  ZeroMemory(&child_info_, sizeof(child_info_));
}

bool WindowsSubprocess::start() {
  if (!start_suspended_child())
    return false;
  if (!unsuspend_child())
    return false;
  return true;
}

bool WindowsSubprocess::start_suspended_child() {
  STARTUPINFO startup_info;
  ZeroMemory(&startup_info, sizeof(startup_info));
  startup_info.cb = sizeof(startup_info);

  // Try creating the process in a suspended state.
  dword_t creation_flags = CREATE_SUSPENDED;
  bool result = CreateProcess(
      cmd_,             // lpApplicationName
      NULL,             // lpCommandLine
      NULL,             // lpProcessAttributes
      NULL,             // lpThreadAttributes
      true,             // bInheritHandles
      creation_flags,   // dwCreationFlags
      NULL,             // lpEnvironment
      NULL,             // lpCurrentDirectory
      &startup_info,    // lpStartupInfo
      &child_info_);    // lpProcessInformation

  if (!result)
    LOG_ERROR("Failed to create child process '%s'", cmd_);

  return result;
}

bool WindowsSubprocess::unsuspend_child() {
  bool result = ResumeThread(thread());
  if (!result)
    LOG_ERROR("Failed to resume thread '%s'", cmd_);
  return result;
}

bool WindowsSubprocess::join() {
  WaitForSingleObject(process(), INFINITE);
  return true;
}

Subprocess *Subprocess::create(const char *cmd, char *const *argv) {
  return new WindowsSubprocess(cmd, argv);
}
