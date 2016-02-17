//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "driver-manager.hh"

#include "test.hh"


BEGIN_C_INCLUDES
#include "utils/string-inl.h"
#include "utils/strbuf.h"
END_C_INCLUDES

using namespace conprx;
using namespace plankton;
using namespace tclib;

DriverManagerService::DriverManagerService(DriverManager *manager)
  : manager_(manager) {
  register_method("log", new_callback(&DriverManagerService::on_log, this));
  register_method("is_ready", new_callback(&DriverManagerService::on_is_ready, this));
}

void DriverManagerService::on_log(rpc::RequestData& data, ResponseCallback resp) {
  TextWriter writer;
  writer.write(data[0]);
  INFO("AGENT LOG: %s", *writer);
  resp(rpc::OutgoingResponse::success(Variant::null()));
}

void DriverManagerService::on_is_ready(rpc::RequestData& data, ResponseCallback resp) {
  manager()->mark_agent_ready();
  resp(rpc::OutgoingResponse::success(Variant::null()));
}

DriverManager::DriverManager()
  : agent_is_ready_(false)
  , trace_(false) {
  channel_ = ServerChannel::create();
}

bool DriverManager::enable_agent() {
  return enable_agent_fake();
}

bool DriverManager::enable_agent_fake() {
  fake_agent_channel_ = ServerChannel::create();
  return true;
}

void *DriverManager::run_agent_monitor() {
  address_arith_t result = fake_agent()->process_all_messages();
  return reinterpret_cast<void*>(result);
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
  OutgoingRequest req(Variant::null(), selector, 1, &arg);
  return send_request(&req);
}

Variant DriverRequest::send(Variant selector, Variant arg0, Variant arg1,
    Variant arg2) {
  Variant argv[3] = {arg0, arg1, arg2};
  OutgoingRequest req(Variant::null(), selector, 3, argv);
  return send_request(&req);
}

Variant DriverRequest::send_request(OutgoingRequest *req) {
  ASSERT_FALSE(is_used_);
  is_used_ = true;
  response_ = manager_->send(req);
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

bool DriverManager::start() {
  CommandLineBuilder builder;
  if (!channel()->allocate())
    return false;
  builder.add_option("channel", channel()->name().chars);
  if (has_fake_agent()) {
    if (!fake_agent_channel()->allocate())
      return false;
    builder.add_option("fake-agent-channel", fake_agent_channel()->name().chars);
  }
  utf8_t args = builder.flush();
  utf8_t exec = executable_path();
  return process_.start(exec, 1, &args);
}

bool DriverManager::connect() {
  if (has_fake_agent() && !fake_agent_channel()->open())
    return false;
  if (!channel()->open())
    return false;
  if (has_fake_agent()) {
    service_ = new (kDefaultAlloc) DriverManagerService(this);
    fake_agent_ = new (kDefaultAlloc) StreamServiceConnector(fake_agent_channel()->in(),
        fake_agent_channel()->out());
    if (!fake_agent()->init(service()->handler()))
      return false;
    agent_monitor_.set_callback(new_callback(&DriverManager::run_agent_monitor,
        this));
    if (!agent_monitor_.start())
      return false;
  }
  connector_ = new (kDefaultAlloc) StreamServiceConnector(channel()->in(),
      channel()->out());
  connector_->set_default_type_registry(ConsoleProxy::registry());
  if (!connector()->init(empty_callback()))
    return false;
  return true;
}

IncomingResponse DriverManager::send(rpc::OutgoingRequest *req) {
  if (trace_) {
    TextWriter subjw, selw, argsw;
    subjw.write(req->subject());
    selw.write(req->selector());
    argsw.write(req->arguments());
    LOG_INFO("%s->%s%s", *subjw, *selw, *argsw);
  }
  IncomingResponse resp = connector()->socket()->send_request(req);
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

bool DriverManager::join() {
  if (!channel()->close())
    return false;
  if (has_fake_agent()) {
    agent_monitor_.join();
    if (!fake_agent_channel()->close())
      return false;
  }
  ProcessWaitIop wait(&process_, o0());
  if (!wait.execute())
    return false;
  return process_.exit_code().peek_value(1) == 0;
}

utf8_t DriverManager::executable_path() {
  const char *result = getenv("DRIVER");
  ASSERT_TRUE(result != NULL);
  return new_c_string(result);
}
