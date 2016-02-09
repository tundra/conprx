//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "driver.hh"
#include "socket.hh"
#include "sync/pipe.hh"
#include "rpc.hh"
#include "agent/conapi.hh"

BEGIN_C_INCLUDES
#include "utils/log.h"
#include "utils/string-inl.h"
END_C_INCLUDES

using namespace plankton;
using namespace tclib;

#define FOR_EACH_DRIVER_FUNCTION(F)                                            \
  F(echo)

class ConsoleDriver : public rpc::Service {
public:
  ConsoleDriver(conprx::Console *console);
#define __DECL_DRIVER_BY_NAME__(name)                                          \
  void name(rpc::RequestArguments args, ResponseCallback callback);
  FOR_EACH_DRIVER_FUNCTION(__DECL_DRIVER_BY_NAME__)
#define __DECL_DRIVER_FUNCTION__(Name, name, MINOR, SIG) __DECL_DRIVER_BY_NAME__(name)
  FOR_EACH_CONAPI_FUNCTION(__DECL_DRIVER_FUNCTION__)
#undef __DECL_DRIVER_FUNCTION__
#undef __DECL_DRIVER_BY_NAME__

  conprx::Console *console() { return console_; }

private:
  conprx::Console *console_;
};

ConsoleDriver::ConsoleDriver(conprx::Console *console)
  : console_(console) {
#define __REG_DRIVER_BY_NAME__(name)                                           \
  register_method(#name, new_callback(&ConsoleDriver::name, this));
  FOR_EACH_DRIVER_FUNCTION(__REG_DRIVER_BY_NAME__)
#define __REG_DRIVER_FUNCTION__(Name, name, MINOR, SIG)                        \
  mfDr MINOR (__REG_DRIVER_BY_NAME__(name), )
  FOR_EACH_CONAPI_FUNCTION(__REG_DRIVER_FUNCTION__)
#undef __REG_DRIVER_FUNCTION__
#undef __REG_DRIVER_BY_NAME
}

void ConsoleDriver::echo(rpc::RequestArguments args, ResponseCallback callback) {
  callback(rpc::OutgoingResponse::success(args[0]));
}

void ConsoleDriver::get_std_handle(rpc::RequestArguments value, ResponseCallback callback) {
  dword_t n_std_handle = static_cast<dword_t>(value[0].integer_value());
  handle_t handle = console()->get_std_handle(n_std_handle);
  callback(rpc::OutgoingResponse::success(reinterpret_cast<int64_t>(handle)));
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
    LOG_ERROR("Error parsing command-line at %i (char '%c')", error->offset(),
        error->offender());
    return false;
  }
  const char *channel = cmdline->option("channel", Variant::null()).string_chars();
  if (channel == NULL) {
    LOG_ERROR("No channel name specified");
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
  conprx::Console *console = conprx::Console::native();
  ConsoleDriver driver(console);
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
