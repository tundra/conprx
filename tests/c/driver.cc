//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "driver.hh"

#include "agent/agent.hh"
#include "agent/conconn.hh"
#include "agent/confront.hh"
#include "marshal-inl.hh"
#include "rpc.hh"
#include "socket.hh"
#include "sync/pipe.hh"

BEGIN_C_INCLUDES
#include "utils/lifetime.h"
#include "utils/log.h"
#include "utils/string-inl.h"
END_C_INCLUDES

using namespace conprx;
using namespace plankton;
using namespace tclib;

// A service wrapping a console frontend.
class ConsoleFrontendService : public rpc::Service {
public:
  ConsoleFrontendService(ConsoleFrontend *console);

#define __DECL_DRIVER_BY_NAME__(name, SIG, ARGS)                               \
  void name(rpc::RequestData &data, ResponseCallback callback);
  FOR_EACH_REMOTE_MESSAGE(__DECL_DRIVER_BY_NAME__)
#define __DECL_DRIVER_FUNCTION__(Name, name, MINOR, SIG, PSIG) __DECL_DRIVER_BY_NAME__(name, , )
  FOR_EACH_CONAPI_FUNCTION(__DECL_DRIVER_FUNCTION__)
#undef __DECL_DRIVER_FUNCTION__
#undef __DECL_DRIVER_BY_NAME__

  // Returns the frontend we delegate everything to.
  ConsoleFrontend *frontend() { return frontend_; }

private:
  // Captures the last error and wraps it in a console error.
  Native new_console_error(Factory *factory);

  static Native wrap_handle(handle_t handle, Factory *factory);
  static dword_t to_dword(Variant value);

  ConsoleFrontend *frontend_;
};

ConsoleFrontendService::ConsoleFrontendService(ConsoleFrontend *console)
  : frontend_(console) {
#define __REG_DRIVER_BY_NAME__(name, SIG, ARGS)                                \
  register_method(#name, new_callback(&ConsoleFrontendService::name, this));
  FOR_EACH_REMOTE_MESSAGE(__REG_DRIVER_BY_NAME__)
#define __REG_DRIVER_FUNCTION__(Name, name, MINOR, SIG, PSIG)                  \
  __REG_DRIVER_BY_NAME__(name, , )
  FOR_EACH_CONAPI_FUNCTION(__REG_DRIVER_FUNCTION__)
#undef __REG_DRIVER_FUNCTION__
#undef __REG_DRIVER_BY_NAME
}

void ConsoleFrontendService::echo(rpc::RequestData &data, ResponseCallback callback) {
  callback(rpc::OutgoingResponse::success(data[0]));
}

void ConsoleFrontendService::is_handle(rpc::RequestData &data, ResponseCallback callback) {
  Handle *handle = data[0].native_as<Handle>();
  callback(rpc::OutgoingResponse::success(Variant::boolean(handle != NULL)));
}

void ConsoleFrontendService::raise_error(rpc::RequestData &data, ResponseCallback callback) {
  int64_t last_error = data[0].integer_value();
  Factory *factory = data.factory();
  ConsoleError *error = new (factory) ConsoleError(last_error);
  callback(rpc::OutgoingResponse::failure(factory->new_native(error)));
}

void ConsoleFrontendService::poke_backend(rpc::RequestData &data, ResponseCallback callback) {
  int64_t value = data[0].integer_value();
  int64_t result = frontend()->poke_backend(value);
  callback(rpc::OutgoingResponse::success(result));
}

void ConsoleFrontendService::get_std_handle(rpc::RequestData &data, ResponseCallback callback) {
  dword_t n_std_handle = to_dword(data[0]);
  handle_t handle = frontend()->get_std_handle(n_std_handle);
  callback(rpc::OutgoingResponse::success(wrap_handle(handle, data.factory())));
}

void ConsoleFrontendService::write_console_a(rpc::RequestData &data, ResponseCallback callback) {
  handle_t output = data[0].native_as<Handle>()->ptr();
  const char *buffer = data[1].string_chars();
  dword_t chars_to_write = to_dword(data[2]);
  dword_t chars_written = 0;
  if (frontend()->write_console_a(output, buffer, chars_to_write, &chars_written,
      NULL)) {
    callback(rpc::OutgoingResponse::success(chars_written));
  } else {
    callback(rpc::OutgoingResponse::failure(new_console_error(data.factory())));
  }
}

void ConsoleFrontendService::get_console_title_a(rpc::RequestData &data, ResponseCallback callback) {
  dword_t chars_to_read = to_dword(data[0]);
  ansi_char_t *buf = new ansi_char_t[chars_to_read];
  dword_t len = frontend()->get_console_title_a(buf, chars_to_read);
  if (len == 0) {
    callback(rpc::OutgoingResponse::failure(new_console_error(data.factory())));
  } else {
    String result = data.factory()->new_string(buf, static_cast<uint32_t>(len));
    callback(rpc::OutgoingResponse::success(result));
  }
}

void ConsoleFrontendService::set_console_title_a(rpc::RequestData &data, ResponseCallback callback) {
  const char *new_title = data[0].string_chars();
  if (frontend()->set_console_title_a(new_title)) {
    callback(rpc::OutgoingResponse::success(Variant::yes()));
  } else {
    callback(rpc::OutgoingResponse::failure(new_console_error(data.factory())));
  }
}

void ConsoleFrontendService::get_console_cp(rpc::RequestData &data, ResponseCallback callback) {
  uint_t cp = frontend()->get_console_cp();
  // GetConsoleCP doesn't specify how it indicates errors but it seems to be
  // by returning 0 so that's what we do here.
  callback((cp == 0)
      ? rpc::OutgoingResponse::failure(new_console_error(data.factory()))
      : rpc::OutgoingResponse::success(cp));
}

Native ConsoleFrontendService::wrap_handle(handle_t raw_handle, Factory *factory) {
  Handle *handle = new (factory) Handle(raw_handle);
  return factory->new_native(handle);
}

Native ConsoleFrontendService::new_console_error(Factory *factory) {
  dword_t last_error = frontend()->get_last_error();
  ConsoleError *error = new (factory) ConsoleError(last_error);
  return factory->new_native(error);
}

dword_t ConsoleFrontendService::to_dword(Variant value) {
  CHECK_TRUE("not integer", value.is_integer());
  int64_t int_val = value.integer_value();
  long long_val = static_cast<long>(int_val);
  CHECK_EQ("dword out of bounds", long_val, int_val);
  return static_cast<dword_t>(long_val);
}

// An implementation of the console agent that doesn't do any actual patching.
// Used for testing only.
class FakeConsoleAgent : public ConsoleAgent {
public:
  FakeConsoleAgent() { }
  virtual void default_destroy() { default_delete_concrete(this); }
  virtual fat_bool_t install_agent_platform() { return F_TRUE; }
  virtual fat_bool_t uninstall_agent_platform() { return F_TRUE; }
};

// A log that ignores everything.
class SilentLog : public Log {
public:
  virtual bool record(log_entry_t *entry) { return true; }
};

// Holds the state for the console driver.
class ConsoleDriverMain {
public:
  ConsoleDriverMain();

  // Parse command-line arguments and store their values in the fields.
  bool parse_args(int argc, const char **argv);

  // Open the connection to the server.
  bool open_connection();

  // Runs the driver service.
  bool run();

  bool install_fake_agent();

  // Main entry-point.
  bool inner_main(int argc, const char **argv);

  // Sets up allocators, crash handling, etc, before calling the inner main.
  static int outer_main(int argc, const char **argv);

private:
  CommandLineReader reader_;
  bool silence_log_;
  SilentLog silent_log_;

  // The channel through which the client communicates with this driver.
  utf8_t channel_name_;
  def_ref_t<ClientChannel> channel_;
  ClientChannel *channel() { return *channel_; }

  DriverFrontendType frontend_type_;

  // If we're faking an agent this is the channel the fake agent will be using.
  // This is only for testing.
  utf8_t fake_agent_channel_name_;
  def_ref_t<ClientChannel> fake_agent_channel_;
  // Returns the fake agent channel or NULL if there is none.
  ClientChannel *fake_agent_channel() { return *fake_agent_channel_; }
  bool use_fake_agent() { return !string_is_empty(fake_agent_channel_name_); }
  def_ref_t<ConsoleAgent> fake_agent_;
  ConsoleAgent *fake_agent() { return *fake_agent_; }
};

ConsoleDriverMain::ConsoleDriverMain()
  : silence_log_(false)
  , channel_name_(string_empty())
  , frontend_type_(dfDummy)
  , fake_agent_channel_name_(string_empty()) { }

bool ConsoleDriverMain::parse_args(int argc, const char **argv) {
  CommandLine *cmdline = reader_.parse(argc - 1, argv + 1);
  if (!cmdline->is_valid()) {
    SyntaxError *error = cmdline->error();
    LOG_ERROR("Error parsing command-line at %i (char '%c')", error->offset(),
        error->offender());
    return false;
  }

  silence_log_ = cmdline->option("silence-log").bool_value();

  const char *channel = cmdline->option("channel").string_chars();
  if (channel == NULL) {
    LOG_ERROR("No channel name specified");
    return false;
  }
  channel_name_ = new_c_string(channel);

  const char *fake_agent_channel = cmdline->option("fake-agent-channel").string_chars();
  if (fake_agent_channel != NULL)
    fake_agent_channel_name_ = new_c_string(fake_agent_channel);

  Variant frontend_type = cmdline->option("frontend-type");
  if (frontend_type == Variant::string("native"))
    frontend_type_ = dfNative;
  else if (frontend_type == Variant::string("dummy"))
    frontend_type_ = dfDummy;
  else if (frontend_type == Variant::string("simulating"))
    frontend_type_ = dfSimulating;
  return true;
}

bool ConsoleDriverMain::open_connection() {
  if (silence_log_)
    silent_log_.ensure_installed();
  if (use_fake_agent()) {
    fake_agent_channel_ = ClientChannel::create();
    if (!fake_agent_channel()->open(fake_agent_channel_name_))
      return false;
    fake_agent_ = new (kDefaultAlloc) FakeConsoleAgent();
    if (!install_fake_agent())
      return false;
  }
  channel_ = ClientChannel::create();
  if (!channel()->open(channel_name_))
    return false;
  return true;
}

bool ConsoleDriverMain::install_fake_agent() {
  if (!use_fake_agent())
    // There is no fake agent so this trivially succeeds.
    return true;
  return fake_agent()->install_agent(fake_agent_channel()->in(),
      fake_agent_channel()->out());
}

bool ConsoleDriverMain::run() {
  // Hook up the console service to the main channel.
  rpc::StreamServiceConnector connector(channel()->in(), channel()->out());
  connector.set_default_type_registry(ConsoleProxy::registry());
  def_ref_t<ConsoleFrontend> frontend;
  def_ref_t<ConsoleConnector> conconn;
  switch (frontend_type_) {
    case dfNative:
      CHECK_TRUE("native frontend not supported", kIsMsvc);
      // The else-part shouldn't ever be run but we need the new_native part to
      // not be present on non-msvc platforms because it hasn't been implemented
      // there.
      frontend = IF_MSVC(ConsoleFrontend::new_native(), pass_def_ref_t<ConsoleFrontend>(NULL));
      break;
    case dfDummy:
      frontend = ConsoleFrontend::new_dummy();
      break;
    case dfSimulating:
      CHECK_TRUE("fake agent required", use_fake_agent());
      conconn = PrpcConsoleConnector::create(fake_agent()->owner()->socket(),
          fake_agent()->owner()->input());
      frontend = ConsoleFrontend::new_simulating(*conconn);
      break;
  }
  ConsoleFrontendService driver(*frontend);
  if (!connector.init(driver.handler()))
    return false;
  bool result = connector.process_all_messages();
  channel()->out()->close();
  if (use_fake_agent())
    F_TRY(fake_agent()->uninstall_agent());
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
