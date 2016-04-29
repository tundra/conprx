//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "driver-manager.hh"
#include "utils/string.hh"

#include "test.hh"


BEGIN_C_INCLUDES
#include "utils/strbuf.h"
#include "utils/string-inl.h"
#include "utils/trybool.h"
END_C_INCLUDES

using namespace conprx;
using namespace plankton;
using namespace tclib;

DriverManager::DriverManager()
  : has_started_agent_monitor_(false)
  , silence_log_(false)
  , trace_(false)
  , agent_path_(string_empty())
  , agent_type_(atNone)
  , frontend_type_(dfDummy)
  , backend_(NULL) {
  channel_ = ServerChannel::create();
}

void DriverManager::set_agent_type(AgentType type) {
  if (!kSupportsRealAgent)
    CHECK_FALSE("real agent not supported", type == atReal);
  agent_type_ = type;
}

void DriverManager::set_frontend_type(DriverFrontendType type) {
  if (!kIsMsvc)
    CHECK_FALSE("native frontend not supported", type == dfNative);
  if (agent_type_ != atFake)
    CHECK_FALSE("simulating frontend requires fake agent", type == dfSimulating);
  frontend_type_ = type;
}

Variant DriverRequest::echo(Variant value) {
  return send("echo", value);
}

Variant DriverRequest::is_handle(Variant value) {
  return send("is_handle", value);
}

Variant DriverRequest::raise_error(NtStatus last_error) {
  return send("raise_error", last_error.to_nt());
}

Variant DriverRequest::poke_backend(Variant value) {
  return send("poke_backend", value);
}

Variant DriverRequest::get_std_handle(int64_t n_std_handle) {
  return send("get_std_handle", n_std_handle);
}

Variant DriverRequest::from_blob(tclib::Blob blob) {
  return Variant::blob(blob.start(), static_cast<uint32_t>(blob.size()));
}


Variant DriverRequest::write_console_a(Handle output, tclib::Blob data) {
  NativeVariant output_var(&output);
  return send("write_console_a", output_var, from_blob(data));
}

Variant DriverRequest::write_console_w(Handle output, tclib::Blob data) {
  NativeVariant output_var(&output);
  return send("write_console_w", output_var, from_blob(data));
}

Variant DriverRequest::read_console_a(Handle input, uint32_t bufsize) {
  NativeVariant input_var(&input);
  return send("read_console_a", input_var, bufsize);
}

Variant DriverRequest::read_console_w(Handle input, uint32_t bufsize,
    ReadConsoleControl *control) {
  NativeVariant input_var(&input);
  NativeVariant control_var(control == NULL ? NULL : control->raw());
  Array pair = send("read_console_w", input_var, bufsize, control_var);
  if (control != NULL) {
    console_readconsole_control_t *control_out = pair[1].native_as<console_readconsole_control_t>();
    *control = ReadConsoleControl(control_out);
  }
  return pair[0];
}

Variant DriverRequest::get_console_title_a(int64_t bufsize) {
  return send("get_console_title_a", bufsize);
}

Variant DriverRequest::set_console_title_a(ansi_cstr_t string) {
  tclib::Blob blob = StringUtils::as_blob(string, true);
  return send("set_console_title_a", from_blob(blob));
}

Variant DriverRequest::get_console_title_w(int64_t bufsize) {
  return send("get_console_title_w", bufsize);
}

Variant DriverRequest::set_console_title_w(wide_cstr_t string) {
  tclib::Blob blob = StringUtils::as_blob(string, true);
  return send("set_console_title_w", from_blob(blob));
}

Variant DriverRequest::get_console_cp() {
  return send("get_console_cp");
}

Variant DriverRequest::set_console_cp(uint32_t value) {
  return send("set_console_cp", value);
}

Variant DriverRequest::get_console_output_cp() {
  return send("get_console_output_cp");
}

Variant DriverRequest::set_console_output_cp(uint32_t value) {
  return send("set_console_output_cp", value);
}

Variant DriverRequest::get_console_mode(Handle handle) {
  NativeVariant handle_var(&handle);
  return send("get_console_mode", handle_var);
}

Variant DriverRequest::set_console_mode(Handle handle, uint32_t mode) {
  NativeVariant handle_var(&handle);
  return send("set_console_mode", handle_var, mode);
}

Variant DriverRequest::get_console_screen_buffer_info(Handle handle) {
  NativeVariant handle_var(&handle);
  return send("get_console_screen_buffer_info", handle_var);
}

Variant DriverRequest::get_console_screen_buffer_info_ex(Handle handle) {
  NativeVariant handle_var(&handle);
  return send("get_console_screen_buffer_info_ex", handle_var);
}

const Variant &DriverRequest::operator*() {
  ASSERT_TRUE(response_->is_settled());
  if (response_->is_rejected()) {
    TextWriter writer;
    writer.write(response_->peek_error(Variant::null()));
    FATAL("Unexpected response failure: %s", *writer);
  }
  return result_;
}

const Variant &DriverRequest::error() {
  ASSERT_TRUE(response_->is_settled());
  ASSERT_TRUE(response_->is_rejected());
  return response_->peek_error(Variant::null());
}

Variant DriverRequest::send(Variant selector) {
  OutgoingRequest req(Variant::null(), selector);
  return send_request(&req);
}

Variant DriverRequest::send(Variant selector, Variant arg) {
  OutgoingRequest req(Variant::null(), selector, 1, &arg);
  return send_request(&req);
}

Variant DriverRequest::send(Variant selector, Variant arg0, Variant arg1) {
  Variant argv[2] = {arg0, arg1};
  OutgoingRequest req(Variant::null(), selector, 2, argv);
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

fat_bool_t DriverManager::start() {
  CommandLineBuilder builder;
  F_TRY(channel()->allocate());
  builder.add_option("channel", channel()->name().chars);
  switch (agent_type()) {
    case atNone:
      launcher_ = new (kDefaultAlloc) NoAgentLauncher();
      break;
    case atReal:
      launcher_ = new (kDefaultAlloc) InjectingLauncher(agent_path());
      break;
    case atFake: {
      FakeAgentLauncher *launcher = new (kDefaultAlloc) FakeAgentLauncher();
      F_TRY(launcher->allocate());
      builder.add_option("fake-agent-channel",
          launcher->agent_channel()->name().chars);
      launcher_ = launcher;
      break;
    }
    default:
      return F_FALSE;
  }
  if (backend_ != NULL)
    launcher()->set_backend(backend_);
  agent_monitor_.set_callback(new_callback(&Launcher::monitor_agent, launcher()));
  const char *frontend_type_name = NULL;
  switch (frontend_type()) {
    case dfNative:
      frontend_type_name = "native";
      break;
    case dfDummy:
      frontend_type_name = "dummy";
      break;
    case dfSimulating:
      frontend_type_name = "simulating";
      break;
  }
  builder.add_option("frontend-type", frontend_type_name);
  if (silence_log_)
    builder.add_option("silence-log", Variant::yes());
  builder.add_option("port-delta", 90000);
  F_TRY(launcher()->initialize());
  utf8_t args = builder.flush();
  utf8_t exec = executable_path();
  F_TRY(launcher()->start(exec, 1, &args));
  if (trace_)
    tracer_.install(launcher()->socket());
  if (!use_agent())
    return F_TRUE;
  launcher()->agent_monitor_done()->raise();
  has_started_agent_monitor_ = true;
  return F_BOOL(agent_monitor_.start());
}

fat_bool_t DriverManager::connect() {
  F_TRY(channel()->open());
  connector_ = new (kDefaultAlloc) StreamServiceConnector(channel()->in(),
      channel()->out());
  registry_.add_fallback(ConsoleTypes::registry());
  registry_.register_type<ConsoleError>();
  connector_->set_default_type_registry(&registry_);
  return connector()->init(empty_callback());
}

IncomingResponse DriverManager::send(rpc::OutgoingRequest *req) {
  if (trace_) {
    TextWriter selw, argsw;
    selw.write(req->selector());
    argsw.write(req->arguments());
    LOG_INFO("DR |> %s %s", *selw, *argsw);
  }
  IncomingResponse resp = connector()->socket()->send_request(req);
  bool keep_going = true;
  while (keep_going && !resp->is_settled())
    keep_going = connector()->input()->process_next_instruction(NULL);
  if (trace_) {
    if (resp->is_fulfilled()) {
      TextWriter resw;
      resw.write(resp->peek_value(Variant::null()));
      LOG_INFO("DR <| %s", *resw);
    } else if (resp->is_rejected()) {
      TextWriter resw;
      resw.write(resp->peek_error(Variant::null()));
      LOG_INFO("DR <! %s", *resw);
    }
  }
  return resp;
}

fat_bool_t DriverManager::join(int *exit_code_out) {
  F_TRY(channel()->close());
  if (use_agent()) {
    F_TRY(launcher()->close_agent());
    if (has_started_agent_monitor_) {
      opaque_t monitor_result = o0();
      F_TRY(agent_monitor_.join(&monitor_result));
      F_TRY(o2f(monitor_result));
    }
  }
  int exit_code = 0;
  F_TRY(launcher()->join(&exit_code));
  if (exit_code_out == NULL) {
    return F_BOOL(exit_code == 0);
  } else {
    *exit_code_out = exit_code;
    return F_TRUE;
  }
}

utf8_t DriverManager::executable_path() {
  const char *result = getenv("DRIVER");
  ASSERT_TRUE(result != NULL);
  return new_c_string(result);
}

utf8_t DriverManager::agent_path() {
  return string_is_empty(agent_path_) ? default_agent_path() : agent_path_;
}

utf8_t DriverManager::default_agent_path() {
  const char *result = getenv("AGENT");
  ASSERT_TRUE(result != NULL);
  return new_c_string(result);
}

FakeAgentLauncher::FakeAgentLauncher() {
  agent_channel_ = ServerChannel::create();
}

fat_bool_t FakeAgentLauncher::allocate() {
  F_TRY(agent_channel()->allocate());
  return F_TRUE;
}

fat_bool_t FakeAgentLauncher::start_connect_to_agent() {
  F_TRY(ensure_process_resumed());
  F_TRY(agent_channel()->open());
  F_TRY(attach_agent_service());
  return F_TRUE;
}

fat_bool_t NoAgentLauncher::start_connect_to_agent() {
  UNREACHABLE("NoAgentLauncher::start_connect_to_agent");
  return F_FALSE;
}
