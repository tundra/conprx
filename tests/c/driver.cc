//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "driver.hh"
#include "sync/pipe.hh"

BEGIN_C_INCLUDES
#include "utils/log.h"
#include "utils/string-inl.h"
END_C_INCLUDES

using namespace plankton;
using namespace tclib;

class ConsoleDriver {
public:
  static int main(int argc, const char **argv);
};

int ConsoleDriver::main(int argc, const char **argv) {
  CommandLineReader reader;
  CommandLine *cmdline = reader.parse(argc - 1, argv + 1);
  if (!cmdline->is_valid()) {
    SyntaxError *error = cmdline->error();
    ERROR("Error parsing command-line at %i (char '%c')", error->offset(),
        error->offender());
    return 1;
  }
  const char *name = cmdline->option("channel", Variant::null()).string_chars();
  if (name == NULL) {
    ERROR("No channel name specified");
    return 1;
  }
  def_ref_t<ClientChannel> channel = ClientChannel::create();
  if (!channel->open(new_c_string(name)))
    return 1;
  char buf[1024];
  ReadIop read(channel->in(), buf, 1024);
  if (!read.execute())
    return 1;
  return 0;
}

int main(int argc, const char *argv[]) {
  return ConsoleDriver::main(argc, argv);
}
