//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "driver.hh"

#include "agent/conapi.hh"
#include "marshal-inl.hh"
#include "rpc.hh"
#include "socket.hh"
#include "sync/pipe.hh"

BEGIN_C_INCLUDES
#include "utils/log.h"
#include "utils/string-inl.h"
#include "utils/lifetime.h"
END_C_INCLUDES

using namespace plankton;
using namespace tclib;

class ConsoleDriver : public rpc::Service {
public:
  ConsoleDriver(conprx::Console *console);
#define __DECL_DRIVER_BY_NAME__(name, SIG, ARGS)                               \
  void name(rpc::RequestData &data, ResponseCallback callback);
  FOR_EACH_REMOTE_MESSAGE(__DECL_DRIVER_BY_NAME__)
#define __DECL_DRIVER_FUNCTION__(Name, name, MINOR, SIG, PSIG) __DECL_DRIVER_BY_NAME__(name, , )
  FOR_EACH_CONAPI_FUNCTION(__DECL_DRIVER_FUNCTION__)
#undef __DECL_DRIVER_FUNCTION__
#undef __DECL_DRIVER_BY_NAME__

  conprx::Console *console() { return console_; }

private:
  Native new_console_error(Factory *factory);

  static Native wrap_handle(handle_t handle, Factory *factory);
  static dword_t to_dword(Variant value);

  conprx::Console *console_;
};

ConsoleDriver::ConsoleDriver(conprx::Console *console)
  : console_(console) {
#define __REG_DRIVER_BY_NAME__(name, SIG, ARGS)                                \
  register_method(#name, new_callback(&ConsoleDriver::name, this));
  FOR_EACH_REMOTE_MESSAGE(__REG_DRIVER_BY_NAME__)
#define __REG_DRIVER_FUNCTION__(Name, name, MINOR, SIG, PSIG)                  \
  __REG_DRIVER_BY_NAME__(name, , )
  FOR_EACH_CONAPI_FUNCTION(__REG_DRIVER_FUNCTION__)
#undef __REG_DRIVER_FUNCTION__
#undef __REG_DRIVER_BY_NAME
}

void ConsoleDriver::echo(rpc::RequestData &data, ResponseCallback callback) {
  callback(rpc::OutgoingResponse::success(data[0]));
}

void ConsoleDriver::is_handle(rpc::RequestData &data, ResponseCallback callback) {
  conprx::Handle *handle = data[0].native_as(conprx::Handle::seed_type());
  callback(rpc::OutgoingResponse::success(Variant::boolean(handle != NULL)));
}

void ConsoleDriver::raise_error(rpc::RequestData &data, ResponseCallback callback) {
  int64_t last_error = data[0].integer_value();
  Factory *factory = data.factory();
  conprx::ConsoleError *error = new (factory) conprx::ConsoleError(last_error);
  callback(rpc::OutgoingResponse::failure(factory->new_native(error)));
}

void ConsoleDriver::get_std_handle(rpc::RequestData &data, ResponseCallback callback) {
  dword_t n_std_handle = to_dword(data[0]);
  handle_t handle = console()->get_std_handle(n_std_handle);
  callback(rpc::OutgoingResponse::success(wrap_handle(handle, data.factory())));
}

void ConsoleDriver::write_console_a(rpc::RequestData &data, ResponseCallback callback) {
  handle_t output = data[0].native_as(conprx::Handle::seed_type())->ptr();
  const char *buffer = data[1].string_chars();
  dword_t chars_to_write = to_dword(data[2]);
  dword_t chars_written = 0;
  if (console()->write_console_a(output, buffer, chars_to_write, &chars_written,
      NULL)) {
    callback(rpc::OutgoingResponse::success(chars_written));
  } else {
    callback(rpc::OutgoingResponse::failure(new_console_error(data.factory())));
  }
}

void ConsoleDriver::get_console_title_a(rpc::RequestData &data, ResponseCallback callback) {
  dword_t chars_to_read = to_dword(data[0]);
  ansi_char_t *buf = new ansi_char_t[chars_to_read];
  dword_t len = console()->get_console_title_a(buf, chars_to_read);
  if (len == 0) {
    callback(rpc::OutgoingResponse::failure(new_console_error(data.factory())));
  } else {
    String result = data.factory()->new_string(buf, static_cast<uint32_t>(len));
    callback(rpc::OutgoingResponse::success(result));
  }
}

void ConsoleDriver::set_console_title_a(rpc::RequestData &data, ResponseCallback callback) {
  const char *new_title = data[0].string_chars();
  if (console()->set_console_title_a(new_title)) {
    callback(rpc::OutgoingResponse::success(Variant::yes()));
  } else {
    callback(rpc::OutgoingResponse::failure(new_console_error(data.factory())));
  }
}

Native ConsoleDriver::wrap_handle(handle_t raw_handle, Factory *factory) {
  conprx::Handle *handle = new (factory) conprx::Handle(raw_handle);
  return factory->new_native(handle);
}

Native ConsoleDriver::new_console_error(Factory *factory) {
  dword_t last_error = console()->get_last_error();
  conprx::ConsoleError *error = new (factory) conprx::ConsoleError(last_error);
  return factory->new_native(error);
}

dword_t ConsoleDriver::to_dword(Variant value) {
  CHECK_TRUE("not integer", value.is_integer());
  int64_t int_val = value.integer_value();
  long long_val = static_cast<long>(int_val);
  CHECK_EQ("dword out of bounds", long_val, int_val);
  return static_cast<dword_t>(long_val);
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
  bool inner_main(int argc, const char **argv);

  // Sets up allocators, crash handling, etc, before calling the inner main.
  static int outer_main(int argc, const char **argv);

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
  connector.set_default_type_registry(conprx::ConsoleProxy::registry());
  def_ref_t<conprx::Console> console = conprx::Console::new_native();
  ConsoleDriver driver(*console);
  if (!connector.init(driver.handler()))
    return false;
  bool result = connector.process_all_messages();
  channel()->out()->close();
  return result;
}

bool ConsoleDriverMain::inner_main(int argc, const char **argv) {
  return parse_args(argc, argv) && open_connection() && run();
}

int ConsoleDriverMain::outer_main(int argc, const char **argv) {
  limited_allocator_t limiter;
  limited_allocator_install(&limiter, 100 * 1024 * 1024);
  fingerprinting_allocator_t fingerprinter;
  fingerprinting_allocator_install(&fingerprinter);
  install_crash_handler();

  bool result;
  {
    ConsoleDriverMain main;
    result = main.inner_main(argc, argv);
  }
  if (!result)
    return 1;

  if (fingerprinting_allocator_uninstall(&fingerprinter)
      && limited_allocator_uninstall(&limiter)) {
    return 0;
  } else {
    return 1;
  }
}

int main(int argc, const char *argv[]) {
  return ConsoleDriverMain::outer_main(argc, argv);
}
