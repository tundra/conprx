//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "driver.hh"
#include "socket.hh"
#include "sync/pipe.hh"
#include "rpc.hh"

BEGIN_C_INCLUDES
#include "utils/log.h"
#include "utils/string-inl.h"
END_C_INCLUDES

using namespace plankton;
using namespace tclib;

class ConsoleDriver : public rpc::Service {
public:
  ConsoleDriver();
  void echo(Variant value, ResponseCallback callback);
};

ConsoleDriver::ConsoleDriver() {
  register_method("echo", new_callback(&ConsoleDriver::echo, this));
}

void ConsoleDriver::echo(Variant value, ResponseCallback callback) {
  callback(rpc::OutgoingResponse::success(value));
}

class ConsoleDriverMain {
public:
  ConsoleDriverMain();

  // Parse command-line arguments and store their values in the fields.
  bool parse_args(int argc, const char **argv);

  // Open the connection to the server.
  bool open_connection();

  // Runs the driver service.
  bool run();

  // Main entry-point.
  int main(int argc, const char **argv);

private:
  CommandLineReader reader_;
  utf8_t channel_name_;
  def_ref_t<ClientChannel> channel_;
  ClientChannel *channel() { return *channel_; }
};

ConsoleDriverMain::ConsoleDriverMain()
  : channel_name_(string_empty()) { }

bool ConsoleDriverMain::parse_args(int argc, const char **argv) {
  CommandLine *cmdline = reader_.parse(argc - 1, argv + 1);
  if (!cmdline->is_valid()) {
    SyntaxError *error = cmdline->error();
    ERROR("Error parsing command-line at %i (char '%c')", error->offset(),
        error->offender());
    return false;
  }
  const char *channel = cmdline->option("channel", Variant::null()).string_chars();
  if (channel == NULL) {
    ERROR("No channel name specified");
    return false;
  }
  channel_name_ = new_c_string(channel);
  return true;
}

bool ConsoleDriverMain::open_connection() {
  channel_ = ClientChannel::create();
  if (!channel()->open(channel_name_))
    return false;
  return true;
}

bool ConsoleDriverMain::run() {
  rpc::StreamServiceConnector connector(channel()->in(), channel()->out());
  ConsoleDriver driver;
  if (!connector.init(driver.handler()))
    return false;
  bool result = connector.process_all_messages();
  channel()->out()->close();
  return result;
}

int ConsoleDriverMain::main(int argc, const char **argv) {
  if (parse_args(argc, argv) && open_connection() && run())
    return 0;
  return 1;
}

int main(int argc, const char *argv[]) {
  ConsoleDriverMain main;
  return main.main(argc, argv);
}
