//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "io/iop.hh"
#include "rpc.hh"
#include "sync/pipe.hh"
#include "sync/process.hh"
#include "async/promise-inl.hh"
#include "test.hh"

BEGIN_C_INCLUDES
#include "utils/string-inl.h"
#include "utils/strbuf.h"
END_C_INCLUDES

using namespace tclib;
using namespace plankton;

class DriverConnection;

class DriverProxy {
public:
  DriverProxy(DriverConnection *connection)
    : is_used_(false)
    , connection_(connection) { }
  Variant echo(Variant value);
  Variant get_std_handle(Variant n_std_handle);
private:
  bool is_used_;
  Variant send_unary(Variant selector, Variant arg);
  Variant send(rpc::OutgoingRequest *req);
  DriverConnection *connection_;
  rpc::IncomingResponse response_;
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

  void set_trace(bool value) { trace_ = value; }

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

Variant DriverProxy::get_std_handle(Variant n_std_handle) {
  return send_unary("get_std_handle", n_std_handle);
}

Variant DriverProxy::send_unary(Variant selector, Variant arg) {
  rpc::OutgoingRequest req(Variant::null(), selector, 1, &arg);
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
    }
  }
  return resp;
}

bool DriverConnection::join() {
  if (!channel()->close())
    return false;
  ProcessWaitIop wait(&process_, o0());
  return wait.execute();
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

  ASSERT_TRUE(driver.join());
}

TEST(driver, get_std_handle) {
  DriverConnection driver;
  ASSERT_TRUE(driver.start());
  ASSERT_TRUE(driver.connect());

  DriverProxy gsh10 = driver.new_call();
  ASSERT_TRUE(gsh10.get_std_handle(-10).is_integer());

  DriverProxy gsh11 = driver.new_call();
  ASSERT_TRUE(gsh11.get_std_handle(-11).is_integer());

  DriverProxy gsh12 = driver.new_call();
  ASSERT_EQ(-1, gsh12.get_std_handle(1000).integer_value());

  ASSERT_TRUE(driver.join());
}
