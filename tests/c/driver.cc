//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "driver.hh"

BEGIN_C_INCLUDES
#include "utils/log.h"
END_C_INCLUDES

using namespace plankton;

class ConsoleDriver {
public:
  static int main(int argc, const char **argv);
};

int ConsoleDriver::main(int argc, const char **argv) {
  CommandLineReader reader;
  CommandLine *cmdline = reader.parse(argc - 1, argv + 1);
  if (!cmdline->is_valid()) {
    SyntaxError *error = cmdline->error();
    ERROR("Error parsing command-line at %c", error->offender());
    return 1;
  }
  return 0;
}

int main(int argc, const char *argv[]) {
  return ConsoleDriver::main(argc, argv);
}
