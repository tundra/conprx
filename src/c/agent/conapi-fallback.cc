//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Console api implementation specific to the windows platform.

using namespace conprx;

// The fallback console is a console implementation that works on all platforms.
// The implementation is just placeholders but they should be functional enough
// that they can be used for testing.
class FallbackConsole : public Console {
public:
#define __DECLARE_CONAPI_METHOD__(Name, name, FLAGS, SIG)                      \
  virtual SIG(GET_SIG_RET) name SIG(GET_SIG_PARAMS);
  FOR_EACH_CONAPI_FUNCTION(__DECLARE_CONAPI_METHOD__)
#undef __DECLARE_CONAPI_METHOD__
};

handle_t FallbackConsole::get_std_handle(dword_t n_std_handle) {
  dword_t result = (-12 <= n_std_handle && n_std_handle <= -10)
      ? n_std_handle + 18
      : -1;
  return reinterpret_cast<handle_t>(result);
}

Console *Console::native() {
  static Console *instance = NULL;
  if (instance == NULL)
    instance = new FallbackConsole();
  return instance;
}
