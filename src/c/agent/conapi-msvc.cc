//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Console api implementation specific to the windows platform.

class WinCon : public Console {
public:
  virtual bool write_console_a(handle_t console_output, const void *buffer,
    dword_t number_of_chars_to_write, dword_t *number_of_chars_written,
    void *reserved);
};

bool WinCon::write_console_a(handle_t console_output, const void *buffer,
    dword_t number_of_chars_to_write, dword_t *number_of_chars_written,
    void *reserved) {
  return WriteConsoleA(console_output, buffer, number_of_chars_to_write,
    number_of_chars_written, reserved);
}

Console &Console::wincon() {
  static WinCon *instance = NULL;
  if (instance == NULL)
    instance = new WinCon();
  return *instance;
}
