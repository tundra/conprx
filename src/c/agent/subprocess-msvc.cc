//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "subprocess.hh"
#include "utils/log.hh"

class WindowsSubprocess : public Subprocess {
public:
  virtual ~WindowsSubprocess();
  WindowsSubprocess(const char *library, const char *command,
      char *const *arguments);

  virtual bool start();
  virtual bool join();

  // Returns a handle for the child process.
  handle_t child_process() { return child_info_.hProcess; }

  // Returns a handle for the main thread in the child process.
  handle_t child_main_thread() { return child_info_.hThread; }

  // Creates the child process without starting it running.
  bool start_suspended_child();

  // Injects the thread that loads the DLL into the suspended child.
  bool inject_dll_thread();

  // Starts the child process running.
  bool unsuspend_child();

private:
  PROCESS_INFORMATION child_info_;
  char *cmdline_;
};

WindowsSubprocess::WindowsSubprocess(const char *library, const char *command,
    char *const *arguments)
  : Subprocess(library, command, arguments)
  , cmdline_(NULL) {
  ZeroMemory(&child_info_, sizeof(child_info_));

  // Scan through the arguments to determine the length of the command line
  // string.
  size_t cmdline_size = 0;
  for (char *const *ptr = arguments_; *ptr != NULL; ptr++)
    cmdline_size += strlen(*ptr) + 1;

  // Build the command line string using sprintf. Unless sprintf calculates the
  // length differently from strlen this should be sure to stay within the
  // bounds even though we're using sprintf, not snprintf (which doesn't exist
  // on windows).
  cmdline_ = new char[cmdline_size];
  size_t offset = 0;
  for (char *const *ptr = arguments_; *ptr != NULL; ptr++) {
    offset += sprintf(cmdline_ + offset, "%s ", *ptr);
  }
  cmdline_[offset] = '\0';
}

WindowsSubprocess::~WindowsSubprocess() {
  delete[] cmdline_;
}

bool WindowsSubprocess::start() {
  if (!start_suspended_child())
    return false;
  if (!inject_dll_thread())
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
      NULL,             // lpApplicationName
      cmdline_,         // lpCommandLine
      NULL,             // lpProcessAttributes
      NULL,             // lpThreadAttributes
      true,             // bInheritHandles
      creation_flags,   // dwCreationFlags
      NULL,             // lpEnvironment
      NULL,             // lpCurrentDirectory
      &startup_info,    // lpStartupInfo
      &child_info_);    // lpProcessInformation

  if (!result)
    LOG_ERROR("CreateProcess(%s) failed: %i", command_, GetLastError());

  return result;
}

bool WindowsSubprocess::inject_dll_thread() {
  // Allocate a block of memory in the child process to hold the DLL name.
  size_t dll_name_size = strlen(library_) + 1;
  void *remote_dll_name = VirtualAllocEx(child_process(), NULL, dll_name_size,
      MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  if (remote_dll_name == NULL) {
    LOG_ERROR("Failed to allocate memory for DLL name in child process");
    return false;
  }

  // Write the name into the child process.
  win_size_t bytes_written = 0;
  bool written = WriteProcessMemory(child_process(), remote_dll_name,
      (void*) library_, dll_name_size, &bytes_written);
  if (!written) {
    LOG_ERROR("Failed to write DLL name into child process");
    return false;
  }
  if (bytes_written != dll_name_size) {
    LOG_ERROR("Write of DLL name into child process was incomplete.");
    return false;
  }

  // Grab the address of LoadLibrary. What we'll actually get is the address
  // within this process but kernel32.dll is always located in the same place
  // so the address will also work in the child process.
  module_t kernel32 = GetModuleHandle(TEXT("kernel32.dll"));
  void *load_library_a = GetProcAddress(kernel32, TEXT("LoadLibraryA"));
  if (load_library_a == NULL) {
    LOG_ERROR("Failed to resolve LoadLibraryA");
    return false;
  }

  // Create a thread in the child process that loads the DLL.
  handle_t loader_thread = CreateRemoteThread(child_process(), NULL, 0,
      (LPTHREAD_START_ROUTINE) load_library_a, remote_dll_name, NULL, NULL);
  if (loader_thread == NULL) {
    LOG_ERROR("Failed to create remote DLL loader thread");
    return false;
  }

  // Start the loader thread running.
  dword_t retval = ResumeThread(loader_thread);
  if (retval == -1) {
    LOG_ERROR("Failed to start loader thread (%i)", GetLastError());
    return false;
  }

  // Wait the the loader thread to complete.
  WaitForSingleObject(loader_thread, INFINITE);

  return true;
}

bool WindowsSubprocess::unsuspend_child() {
  dword_t retval = ResumeThread(child_main_thread());
  if (retval == -1) {
    LOG_ERROR("Failed to resume thread '%s'", command_);
    return false;
  }
  return true;
}

bool WindowsSubprocess::join() {
  WaitForSingleObject(child_process(), INFINITE);
  return true;
}

Subprocess *Subprocess::create(const char *library, const char *command,
    char *const *arguments) {
  return new WindowsSubprocess(library, command, arguments);
}
