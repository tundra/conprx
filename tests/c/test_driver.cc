//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "async/promise-inl.hh"
#include "driver.hh"
#include "io/iop.hh"
#include "marshal-inl.hh"
#include "rpc.hh"
#include "sync/pipe.hh"
#include "sync/process.hh"
#include "test.hh"

BEGIN_C_INCLUDES
#include "utils/string-inl.h"
#include "utils/strbuf.h"
END_C_INCLUDES

using namespace tclib;
using namespace plankton;
using namespace conprx;

class DriverConnection;

class DriverProxy {
public:
  DriverProxy(DriverConnection *connection)
    : is_used_(false)
    , connection_(connection) { }
  Variant echo(Variant value);
  Variant is_handle(Variant value);
  Handle get_std_handle(int64_t n_std_handle);
  Variant write_console_a(Handle console_output, const char *string,
      int64_t chars_to_write);
  String get_console_title_a(int64_t bufsize);
  Variant set_console_title_a(const char *string);
  Factory *factory() { return &arena_; }
private:
  bool is_used_;
  Variant send_unary(Variant selector, Variant arg);
  Variant send_ternary(Variant selector, Variant arg0, Variant arg1, Variant arg2);
  Variant send(rpc::OutgoingRequest *req);
  Handle unwrap_handle(Variant value);
  DriverConnection *connection_;
  rpc::IncomingResponse response_;
  Arena arena_;
};

// Manages a driver instance.
//
// TODO: factor out into a header once the api has stabilized.
class DriverConnection {
public:
  DriverConnection();

  // Start up the driver executable.
  bool start();

  // Connect to the running driver.
  bool connect();

  // Wait for the driver to terminate.
  bool join();

  ONLY_ALLOW_DEVUTILS(void set_trace(bool value) { trace_ = value; })

  // Returns a new proxy that can be used to perform a single call to the
  // driver. The value returned by the call is only valid as long as the proxy
  // is.
  DriverProxy new_call() { return DriverProxy(this); }

  rpc::IncomingResponse send(rpc::OutgoingRequest *req);

private:
  NativeProcess process_;
  def_ref_t<ServerChannel> channel_;
  ServerChannel *channel() { return *channel_;}
  def_ref_t<rpc::StreamServiceConnector> connector_;
  rpc::StreamServiceConnector *connector() { return *connector_; }
  bool trace_;
  static utf8_t executable_path();
};

DriverConnection::DriverConnection()
  : trace_(false) {
  channel_ = ServerChannel::create();
}

Variant DriverProxy::echo(Variant value) {
  return send_unary("echo", value);
}

Variant DriverProxy::is_handle(Variant value) {
  return send_unary("is_handle", value);
}

Handle DriverProxy::get_std_handle(int64_t n_std_handle) {
  return unwrap_handle(send_unary("get_std_handle", n_std_handle));
}

Variant DriverProxy::write_console_a(Handle console_output, const char *string,
    int64_t chars_to_write) {
  return send_ternary("write_console_a", factory()->new_native(&console_output),
      string, chars_to_write);
}

String DriverProxy::get_console_title_a(int64_t bufsize) {
  return send_unary("get_console_title_a", bufsize);
}

Variant DriverProxy::set_console_title_a(const char *string) {
  return send_unary("set_console_title_a", string);
}

Handle DriverProxy::unwrap_handle(Variant value) {
  Handle *result = value.native_as(Handle::seed_type());
  return (result == NULL) ? Handle::invalid() : *result;
}

Variant DriverProxy::send_unary(Variant selector, Variant arg) {
  rpc::OutgoingRequest req(Variant::null(), selector, 1, &arg);
  return send(&req);
}

Variant DriverProxy::send_ternary(Variant selector, Variant arg0, Variant arg1,
    Variant arg2) {
  Variant argv[3] = {arg0, arg1, arg2};
  rpc::OutgoingRequest req(Variant::null(), selector, 3, argv);
  return send(&req);
}

Variant DriverProxy::send(rpc::OutgoingRequest *req) {
  ASSERT_FALSE(is_used_);
  is_used_ = true;
  response_ = connection_->send(req);
  return response_->peek_value(Variant::null());
}

// Utility for building command-line arguments.
class CommandLineBuilder {
public:
  CommandLineBuilder();
  void add_option(Variant key, Variant value);
  utf8_t flush();
private:
  Arena *arena() { return &arena_; }
  Arena arena_;
  Map map_;
  TextWriter writer_;
};

CommandLineBuilder::CommandLineBuilder()
  : map_(arena()->new_map())
  , writer_(FLAT_SYNTAX) {
}

utf8_t CommandLineBuilder::flush() {
  writer_.write(map_);
  return new_string(*writer_, writer_.length());
}

void CommandLineBuilder::add_option(Variant key, Variant value) {
  map_.set(key, value);
}

bool DriverConnection::start() {
  if (!channel()->create(NativePipe::pfDefault))
    return false;
  utf8_t exec = executable_path();
  CommandLineBuilder builder;
  builder.add_option("channel", channel()->name().chars);
  utf8_t args = builder.flush();
  return process_.start(exec, 1, &args);
}

bool DriverConnection::connect() {
  if (!channel()->open())
    return false;
  connector_ = new (kDefaultAlloc) rpc::StreamServiceConnector(channel()->in(),
      channel()->out());
  connector_->set_default_type_registry(ConsoleProxy::registry());
  if (!connector()->init(empty_callback()))
    return false;
  return true;
}

rpc::IncomingResponse DriverConnection::send(rpc::OutgoingRequest *req) {
  if (trace_) {
    TextWriter subjw, selw, argsw;
    subjw.write(req->subject());
    selw.write(req->selector());
    argsw.write(req->arguments());
    LOG_INFO("%s->%s%s", *subjw, *selw, *argsw);
  }
  rpc::IncomingResponse resp = connector()->socket()->send_request(req);
  bool keep_going = true;
  while (keep_going && !resp->is_settled())
    keep_going = connector()->input()->process_next_instruction(NULL);
  if (trace_) {
    if (resp->is_fulfilled()) {
      TextWriter resw;
      resw.write(resp->peek_value(Variant::null()));
      LOG_INFO("<- %s", *resw);
    } else if (resp->is_rejected()) {
      TextWriter resw;
      resw.write(resp->peek_error(Variant::null()));
      LOG_INFO("<! %s", *resw);
    }
  }
  return resp;
}

bool DriverConnection::join() {
  if (!channel()->close())
    return false;
  ProcessWaitIop wait(&process_, o0());
  if (!wait.execute())
    return false;
  return process_.exit_code().peek_value(1) == 0;
}

utf8_t DriverConnection::executable_path() {
  const char *result = getenv("DRIVER");
  ASSERT_TRUE(result != NULL);
  return new_c_string(result);
}

TEST(driver, simple) {
  DriverConnection driver;
  ASSERT_TRUE(driver.start());
  ASSERT_TRUE(driver.connect());

  DriverProxy call0 = driver.new_call();
  ASSERT_EQ(5436, call0.echo(5436).integer_value());

  Arena arena;
  DriverProxy call1 = driver.new_call();
  Array arg1 = arena.new_array(0);
  arg1.add(8);
  arg1.add("foo");
  arg1.add(Variant::yes());
  Array res1 = call1.echo(arg1);
  ASSERT_EQ(8, res1[0].integer_value());

  DriverProxy call2 = driver.new_call();
  Handle out(54234);
  Handle *in = call2.echo(call2.factory()->new_native(&out)).native_as(Handle::seed_type());
  ASSERT_TRUE(in != NULL);
  ASSERT_EQ(54234, in->id());

  DriverProxy call3 = driver.new_call();
  ASSERT_TRUE(call3.is_handle(call3.factory()->new_native(&out)) == Variant::yes());

  DriverProxy call4 = driver.new_call();
  ASSERT_TRUE(call4.is_handle(100) == Variant::no());

  ASSERT_TRUE(driver.join());
}

TEST(driver, get_std_handle) {
  DriverConnection driver;
  ASSERT_TRUE(driver.start());
  ASSERT_TRUE(driver.connect());

  DriverProxy gsh10 = driver.new_call();
  ASSERT_TRUE(gsh10.get_std_handle(-10).is_valid());

  DriverProxy gsh11 = driver.new_call();
  ASSERT_TRUE(gsh11.get_std_handle(-11).is_valid());

  DriverProxy gsh12 = driver.new_call();
  ASSERT_TRUE(gsh12.get_std_handle(-12).is_valid());

  DriverProxy gsh1000 = driver.new_call();
  ASSERT_FALSE(gsh1000.get_std_handle(1000).is_valid());

  ASSERT_TRUE(driver.join());
}

TEST(driver, get_console_title) {
  DriverConnection driver;
  driver.set_trace(true);
  ASSERT_TRUE(driver.start());
  ASSERT_TRUE(driver.connect());

  DriverProxy gettit0 = driver.new_call();
  String tit0 = gettit0.get_console_title_a(1024);
  ASSERT_TRUE(tit0.is_string());

  DriverProxy settit0 = driver.new_call();
  ASSERT_TRUE(settit0.set_console_title_a("Looky here!") == Variant::yes());

  DriverProxy gettit1 = driver.new_call();
  String tit1 = gettit1.get_console_title_a(1024);
  ASSERT_C_STREQ("Looky here!", tit1.string_chars());

  ASSERT_TRUE(driver.join());
}
