//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

fat_bool_t ConsoleDriverMain::maybe_alloc_console() {
  if (!alloc_console_)
    return F_TRUE;
  if (!FreeConsole())
    return F_FALSE;
  // This allocates a new console and hides the window. It actually causes the
  // window to flicker which is bad but it's only for testing so it should do.
  if (!AllocConsole())
    return F_FALSE;
  hwnd_t console_window = GetConsoleWindow();
  if (console_window == NULL)
    return F_FALSE;
  if (!ShowWindow(console_window, SW_HIDE))
    return F_FALSE;
  return F_TRUE;
}

fat_bool_t ConsoleDriverMain::maybe_free_console() {
  if (!alloc_console_)
    return F_TRUE;
  return F_BOOL(FreeConsole());
}
