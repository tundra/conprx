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
  DriverProxy(DriverConnection *connection) : connection_(connection) { }
  Variant echo(Variant value);
private:
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

  DriverProxy new_proxy() { return DriverProxy(this); }

  rpc::IncomingResponse send(rpc::OutgoingRequest *req);

private:
  NativeProcess process_;
  def_ref_t<ServerChannel> channel_;
  ServerChannel *channel() { return *channel_;}
  def_ref_t<rpc::StreamServiceConnector> connector_;
  rpc::StreamServiceConnector *connector() { return *connector_; }
  static utf8_t executable_path();
};

DriverConnection::DriverConnection() {
  channel_ = ServerChannel::create();
}

Variant DriverProxy::echo(Variant value) {
  rpc::OutgoingRequest req(Variant::null(), "echo", 1, &value);
  response_ = connection_->send(&req);
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
  rpc::IncomingResponse resp = connector()->socket()->send_request(req);
  bool keep_going = true;
  while (keep_going && !resp->is_settled())
    keep_going = connector()->input()->process_next_instruction(NULL);
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

  DriverProxy proxy = driver.new_proxy();
  ASSERT_EQ(5436, proxy.echo(5436).integer_value());

  ASSERT_TRUE(driver.join());
}
