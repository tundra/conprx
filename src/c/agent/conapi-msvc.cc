//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Console api implementation specific to the windows platform.

using namespace conprx;

class WindowsConsole : public Console {
#define __DECLARE_CONAPI_METHOD__(Name, name, FLAGS, SIG)                      \
  virtual SIG(GET_SIG_RET) name SIG(GET_SIG_PARAMS);
  FOR_EACH_CONAPI_FUNCTION(__DECLARE_CONAPI_METHOD__)
#undef __DECLARE_CONAPI_METHOD__
};

handle_t WindowsConsole::get_std_handle(dword_t handle) {
  return GetStdHandle(handle);
}

Console *Console::native() {
  static Console *instance = NULL;
  if (instance == NULL)
    instance = new WindowsConsole();
  return instance;
}
