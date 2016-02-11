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
#include "agent/conapi.hh"

BEGIN_C_INCLUDES
#include "utils/string-inl.h"
#include "utils/strbuf.h"
END_C_INCLUDES

using namespace tclib;
using namespace plankton;
using namespace conprx;

class DriverConnection;

class DriverRequest {
public:
  DriverRequest(DriverConnection *connection)
    : is_used_(false)
    , connection_(connection) { }

#define __DECL_PROXY_METHOD__(name, SIG, ARGS)                                 \
  Variant name SIG;
  FOR_EACH_REMOTE_MESSAGE(__DECL_PROXY_METHOD__)
#undef __DECL_PROXY_METHOD__

#define __DECL_PROXY_METHOD__(Name, name, FLAGS, SIG, PSIG)                    \
  Variant name PSIG(GET_SIG_PARAMS);
  FOR_EACH_CONAPI_FUNCTION(__DECL_PROXY_METHOD__)
#undef __DECL_PROXY_METHOD__

  Factory *factory() { return &arena_; }

  // Returns the resulting value of this request, assuming it succeeded. If it
  // is not yet resolved or failed this will fail.
  const Variant &operator*();

  // Returns the error resulting from this request, assuming it failed and
  // failing if it didn't.
  const Variant &error();

  // Returns a pointer to the value of this request, assuming it succeeded. If
  // not, same as *.
  const Variant *operator->() { return &this->operator*(); }

  bool is_rejected() { return response_->is_rejected(); }

private:
  Variant send(Variant selector, Variant arg0);
  Variant send(Variant selector, Variant arg0, Variant arg1, Variant arg2);
  Variant send_request(rpc::OutgoingRequest *req);

  bool is_used_;
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

  // Returns a new request object that can be used to perform a single call to
  // the driver. The value returned by the call is only valid as long as the
  // request is.
  DriverRequest new_request() { return DriverRequest(this); }

  // Shorthands for requests that don't need the request object before sending
  // the message.
#define __DECL_PROXY_METHOD__(name, SIG, ARGS)                                 \
  DriverRequest name SIG {                                                     \
    DriverRequest req(this);                                                   \
    req.name ARGS;                                                             \
    return req;                                                                \
  }
  FOR_EACH_REMOTE_MESSAGE(__DECL_PROXY_METHOD__)
#undef __DECL_PROXY_METHOD__

#define __DECL_PROXY_METHOD__(Name, name, FLAGS, SIG, PSIG)                    \
  DriverRequest name PSIG(GET_SIG_PARAMS) {                                    \
    DriverRequest req(this);                                                   \
    req.name PSIG(GET_SIG_ARGS);                                               \
    return req;                                                                \
  }
  FOR_EACH_CONAPI_FUNCTION(__DECL_PROXY_METHOD__)
#undef __DECL_PROXY_METHOD__

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

Variant DriverRequest::echo(Variant value) {
  return send("echo", value);
}

Variant DriverRequest::is_handle(Variant value) {
  return send("is_handle", value);
}

Variant DriverRequest::raise_error(int64_t last_error) {
  return send("raise_error", last_error);
}

Variant DriverRequest::get_std_handle(int64_t n_std_handle) {
  return send("get_std_handle", n_std_handle);
}

Variant DriverRequest::write_console_a(Handle console_output, const char *string,
    int64_t chars_to_write) {
  return send("write_console_a", factory()->new_native(&console_output),
      string, chars_to_write);
}

Variant DriverRequest::get_console_title_a(int64_t bufsize) {
  return send("get_console_title_a", bufsize);
}

Variant DriverRequest::set_console_title_a(const char *string) {
  return send("set_console_title_a", string);
}

const Variant &DriverRequest::operator*() {
  ASSERT_TRUE(response_->is_settled());
  ASSERT_TRUE(response_->is_fulfilled());
  return response_->peek_value(Variant::null());
}

const Variant &DriverRequest::error() {
  ASSERT_TRUE(response_->is_settled());
  ASSERT_TRUE(response_->is_rejected());
  return response_->peek_error(Variant::null());
}

Variant DriverRequest::send(Variant selector, Variant arg) {
  rpc::OutgoingRequest req(Variant::null(), selector, 1, &arg);
  return send_request(&req);
}

Variant DriverRequest::send(Variant selector, Variant arg0, Variant arg1,
    Variant arg2) {
  Variant argv[3] = {arg0, arg1, arg2};
  rpc::OutgoingRequest req(Variant::null(), selector, 3, argv);
  return send_request(&req);
}

Variant DriverRequest::send_request(rpc::OutgoingRequest *req) {
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

  DriverRequest echo0 = driver.echo(5436);
  ASSERT_EQ(5436, echo0->integer_value());

  DriverRequest call1 = driver.new_request();
  Array arg1 = call1.factory()->new_array(0);
  arg1.add(8);
  arg1.add("foo");
  arg1.add(Variant::yes());
  Array res1 = call1.echo(arg1);
  ASSERT_EQ(8, res1[0].integer_value());

  DriverRequest call2 = driver.new_request();
  Handle out(54234);
  Handle *in = call2.echo(call2.factory()->new_native(&out)).native_as(Handle::seed_type());
  ASSERT_TRUE(in != NULL);
  ASSERT_EQ(54234, in->id());

  DriverRequest call3 = driver.new_request();
  ASSERT_TRUE(call3.is_handle(call3.factory()->new_native(&out)) == Variant::yes());

  DriverRequest call4 = driver.is_handle(100);
  ASSERT_TRUE(*call4 == Variant::no());

  ASSERT_TRUE(driver.join());
}

TEST(driver, get_std_handle) {
  DriverConnection driver;
  ASSERT_TRUE(driver.start());
  ASSERT_TRUE(driver.connect());

  DriverRequest gsh10 = driver.get_std_handle(-10);
  ASSERT_TRUE(gsh10->native_as<Handle>()->is_valid());

  DriverRequest gsh11 = driver.get_std_handle(-11);
  ASSERT_TRUE(gsh11->native_as<Handle>()->is_valid());

  DriverRequest gsh12 = driver.get_std_handle(-12);
  ASSERT_TRUE(gsh12->native_as<Handle>()->is_valid());

  DriverRequest gsh1000 = driver.get_std_handle(1000);
  ASSERT_FALSE(gsh1000->native_as<Handle>()->is_valid());

  ASSERT_TRUE(driver.join());
}

TEST(driver, get_console_title) {
  DriverConnection driver;
  ASSERT_TRUE(driver.start());
  ASSERT_TRUE(driver.connect());

  DriverRequest tit0 = driver.get_console_title_a(1024);
  ASSERT_TRUE(tit0->is_string());

  DriverRequest settit0 = driver.set_console_title_a("Looky here!");
  ASSERT_TRUE(*settit0 == Variant::yes());

  DriverRequest tit1 = driver.get_console_title_a(1024);
  ASSERT_C_STREQ("Looky here!", tit1->string_chars());

  ASSERT_TRUE(driver.join());
}

TEST(driver, errors) {
  DriverConnection driver;
  ASSERT_TRUE(driver.start());
  ASSERT_TRUE(driver.connect());

  DriverRequest err = driver.raise_error(1090);
  ASSERT_TRUE(err.is_rejected());
  ASSERT_EQ(1090, err.error().native_as<ConsoleError>()->last_error());

  ASSERT_TRUE(driver.join());
}
