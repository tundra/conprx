//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "host.hh"
#include "launch.hh"

BEGIN_C_INCLUDES
#include "utils/string-inl.h"
#include "utils/log.h"
END_C_INCLUDES

using namespace tclib;
using namespace conprx;

// Rudimentary window implementation. Used just for the host for now but should
// be made into a proper implementation in its own right.
class HostWindow : public ConsoleWindow {
public:
  virtual coord_t size() { return coord_new(80, 25); }
  virtual coord_t cursor_position() { return coord_new(0, 0); }
  virtual word_t attributes() { return 0; }
  virtual small_rect_t position() { return small_rect_new(0, 0, 80, 25); }
  virtual coord_t maximum_window_size() { return size(); }
  virtual response_t<uint32_t> write(tclib::Blob blob, bool is_unicode,
      bool is_error);
};

response_t<uint32_t> HostWindow::write(tclib::Blob blob, bool is_unicode,
    bool is_error) {
  if (is_unicode) {
    return response_t<uint32_t>::error(CONPRX_ERROR_NOT_IMPLEMENTED);
  } else {
    FILE *stream = is_error ? stderr : stdout;
    fprintf(stream, static_cast<char*>(blob.start()), blob.size());
    fflush(stream);
    return response_t<uint32_t>::of(static_cast<uint32_t>(blob.size()));
  }
}

static bool should_trace() {
  const char *envval = getenv("TRACE_CONPRX_HOST");
  return (envval != NULL) && (strcmp(envval, "1") == 0);
}

fat_bool_t fat_main(int argc, char *argv[], int *exit_code_out) {
  if (argc < 3) {
    fprintf(stderr, "Usage: host <library> <command ...>\n");
    return F_FALSE;
  }
  utf8_t library = new_c_string(argv[1]);
  utf8_t command = new_c_string(argv[2]);

  int skip_count = 3;
  int new_argc = argc - skip_count;
  utf8_t *new_argv = new utf8_t[new_argc];
  for (int i = 0; i < new_argc; i++)
    new_argv[i] = new_c_string(argv[skip_count + i]);

  BasicConsoleBackend backend;
  HostWindow window;
  backend.set_window(&window);
  InjectingLauncher launcher(library);
  launcher.set_backend(&backend);

  F_TRY(launcher.initialize());
  F_TRY(launcher.start(command, new_argc, new_argv));

  plankton::rpc::TracingMessageSocketObserver observer;
  if (should_trace())
    observer.install(launcher.socket());

  F_TRY(launcher.process_messages());
  F_TRY(launcher.join(exit_code_out));

  return F_TRUE;
}

int main(int argc, char *argv[]) {
  int exit_code = 0;
  fat_bool_t result = fat_main(argc, argv, &exit_code);
  if (result) {
    return exit_code;
  } else {
    LOG_ERROR("Failed to launch process at " kFatBoolFileLine,
        fat_bool_file(result), fat_bool_line(result));
    return (exit_code == 0) ? 1 : exit_code;
  }
}
