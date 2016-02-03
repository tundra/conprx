//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "io/iop.hh"
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

class Driver {
public:
  Driver();
  bool start();
  bool connect();
  bool join();

private:
  NativeProcess process_;
  def_ref_t<ServerChannel> channel_;

  ServerChannel *channel() { return *channel_;}

  static utf8_t executable_path();
};

Driver::Driver() {
  channel_ = ServerChannel::create();
}

class CommandLineBuilder {
public:
  CommandLineBuilder();
  void add_option(Variant key, Variant value);
  Arena *arena() { return &arena_; }
  utf8_t flush();
private:
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

bool Driver::start() {
  if (!channel()->create(NativePipe::pfDefault))
    return false;
  utf8_t exec = executable_path();
  CommandLineBuilder builder;
  builder.add_option("channel", channel()->name().chars);
  utf8_t args = builder.flush();
  return process_.start(exec, 1, &args);
}

bool Driver::connect() {
  if (!channel()->open())
    return false;
  WriteIop write(channel()->out(), "Hello!", 6);
  if (!write.execute())
    return false;
  if (!channel()->out()->flush())
    return false;
  return true;
}

bool Driver::join() {
  if (!channel()->close())
    return false;
  ProcessWaitIop wait(&process_, o0());
  return wait.execute();
}

utf8_t Driver::executable_path() {
  const char *result = getenv("DRIVER");
  ASSERT_TRUE(result != NULL);
  return new_c_string(result);
}

TEST(driver, simple) {
  Driver driver;
  ASSERT_TRUE(driver.start());
  ASSERT_TRUE(driver.connect());
  ASSERT_TRUE(driver.join());
}
