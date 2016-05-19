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

static OutStream *open_trace_stream() {
  const char *trace_flag = getenv("TRACE_CONPRX_HOST");
  if ((trace_flag == NULL) || (strcmp(trace_flag, "1") != 0))
    return NULL;
  const char *trace_file = getenv("TRACE_CONPRX_DEST");
  if ((trace_file == NULL) || (strcmp(trace_file, "-") == 0))
    return FileSystem::native()->std_out();
  return FileSystem::native()->open(new_c_string(trace_file), OPEN_FILE_MODE_WRITE).out();
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

  def_ref_t<ConsoleFrontend> own_frontend = ConsoleFrontend::new_native();
  def_ref_t<ConsolePlatform> own_platform = ConsolePlatform::new_native();
  def_ref_t<WinTty> wty = WinTty::new_adapted(*own_frontend, *own_platform);
  BasicConsoleBackend backend;
  backend.set_wty(*wty);
  backend.set_title("Console host");
  InjectingLauncher launcher(library);
  launcher.set_backend(&backend);

  F_TRY(launcher.initialize());
  F_TRY(launcher.start(command, new_argc, new_argv));

  plankton::rpc::TracingMessageSocketObserver observer("HOST");
  OutStream *trace_stream = open_trace_stream();
  if (trace_stream != NULL) {
    observer.set_out(trace_stream);
    observer.install(launcher.attachment()->socket());
  }

  F_TRY(launcher.attachment()->process_messages());
  F_TRY(launcher.join(exit_code_out));

  if (trace_stream != NULL)
    trace_stream->close();

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
