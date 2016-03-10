//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "driver-manager.hh"
#include "test.hh"

BEGIN_C_INCLUDES
#include "utils/string-inl.h"
#include "utils/strbuf.h"
END_C_INCLUDES

using namespace tclib;
using namespace plankton;
using namespace conprx;

MULTITEST(agent, simple, bool, ("fake", true), ("real", false)) {
  bool use_fake = Flavor;
  if (!use_fake && !DriverManager::kSupportsRealAgent)
    SKIP_TEST(kIsMsvc ? "debug codegen" : "msvc only");
  DriverManager driver;
  driver.set_agent_type(use_fake ? DriverManager::atFake : DriverManager::atReal);
  ASSERT_F_TRUE(driver.start());
  ASSERT_F_TRUE(driver.connect());

  DriverRequest echo0 = driver.echo(5436);
  ASSERT_EQ(5436, echo0->integer_value());

  ASSERT_F_TRUE(driver.join(NULL));
}

TEST(agent, inject_fail) {
  if (!DriverManager::kSupportsRealAgent)
    SKIP_TEST("requires real agent");
  DriverManager driver;
  driver.set_agent_type(DriverManager::atReal);
  driver.set_agent_path(new_c_string("flip-flap-foobeliboo"));
  driver.set_silence_log(true);
  log_o *old_log = silence_global_log();
  ASSERT_FALSE(driver.start());
  set_global_log(old_log);
  ASSERT_F_TRUE(driver->ensure_process_resumed());
  int exit_code = 0;
  ASSERT_F_TRUE(driver.join(&exit_code));
  ASSERT_EQ(1, exit_code);
}

class PokeCounter : public DummyConsoleBackend {
public:
  PokeCounter()  : poke_count(0) { }
  virtual Response<int64_t> on_poke(int64_t value);
  size_t poke_count;
};

Response<int64_t> PokeCounter::on_poke(int64_t value) {
  poke_count++;
  return Response<int64_t>::of(value + 257);
}

TEST(agent, simulate_roundtrip) {
  DriverManager driver;
  driver.set_agent_type(DriverManager::atFake);
  driver.set_frontend_type(dfSimulating);
  PokeCounter counter;
  driver.set_backend(&counter);
  ASSERT_F_TRUE(driver.start());
  ASSERT_F_TRUE(driver.connect());

  ASSERT_EQ(0, counter.poke_count);
  DriverRequest poke0 = driver.poke_backend(4212);
  ASSERT_EQ(4212 + 257, poke0->integer_value());
  ASSERT_EQ(1, counter.poke_count);
  DriverRequest poke1 = driver.poke_backend(5457);
  ASSERT_EQ(5457 + 257, poke1->integer_value());
  ASSERT_EQ(2, counter.poke_count);

  ASSERT_F_TRUE(driver.join(NULL));
}

class CodePageBackend : public DummyConsoleBackend {
public:
  virtual Response<uint32_t> get_console_cp(bool is_output);
  virtual Response<bool_t> set_console_cp(uint32_t value, bool is_output);
  Response<uint32_t> input;
  Response<uint32_t> output;
};

Response<uint32_t> CodePageBackend::get_console_cp(bool is_output) {
  return is_output ? output : input;
}

Response<bool_t> CodePageBackend::set_console_cp(uint32_t value, bool is_output) {
  (is_output ? output : input) = Response<uint32_t>::of(value);
  return Response<bool_t>::of(true);
}


#define SKIP_IF_UNSUPPORTED(USE_REAL) do {                                     \
  if ((USE_REAL) && !DriverManager::kSupportsRealAgent)                        \
    SKIP_TEST("requires real agent");                                          \
} while (false)

static void configure_driver(DriverManager *driver, bool use_real) {
  if (use_real) {
    driver->set_agent_type(DriverManager::atReal);
    driver->set_frontend_type(dfNative);
  } else {
    driver->set_agent_type(DriverManager::atFake);
    driver->set_frontend_type(dfSimulating);
  }
}

MULTITEST(agent, native_get_cp, bool, ("real", true), ("simul", false)) {
  bool use_real = Flavor;
  SKIP_IF_UNSUPPORTED(use_real);
  CodePageBackend backend;
  DriverManager driver;
  driver.set_backend(&backend);
  configure_driver(&driver, use_real);
  ASSERT_F_TRUE(driver.start());
  ASSERT_F_TRUE(driver.connect());

  backend.input = Response<uint32_t>::of(82723);
  DriverRequest cp0 = driver.get_console_cp();
  ASSERT_EQ(82723, cp0->integer_value());

  backend.input = Response<uint32_t>::of(32728);
  DriverRequest cp1 = driver.get_console_cp();
  ASSERT_EQ(32728, cp1->integer_value());

  // For some reason positive error codes aren't propagated but negative ones
  // work.
  backend.input = Response<uint32_t>::error(-123);
  DriverRequest cp2 = driver.get_console_cp();
  ASSERT_EQ(-123, static_cast<int32_t>(cp2.error().native_as<ConsoleError>()->last_error()));

  ASSERT_F_TRUE(driver.join(NULL));
}

MULTITEST(agent, native_set_cp, bool, ("real", true), ("simul", false)) {
  bool use_real = Flavor;
  SKIP_IF_UNSUPPORTED(use_real);
  CodePageBackend backend;
  DriverManager driver;
  driver.set_backend(&backend);
  configure_driver(&driver, use_real);
  ASSERT_F_TRUE(driver.start());
  ASSERT_F_TRUE(driver.connect());

  backend.input = Response<uint32_t>::of(82723);
  DriverRequest cp0 = driver.get_console_cp();
  ASSERT_EQ(82723, cp0->integer_value());

  DriverRequest cp1 = driver.set_console_cp(54643);
  ASSERT_TRUE(cp1->bool_value());
  ASSERT_EQ(54643, backend.input.value());

  ASSERT_F_TRUE(driver.join(NULL));
}

MULTITEST(agent, native_set_output_cp, bool, ("real", true), ("simul", false)) {
  bool use_real = Flavor;
  SKIP_IF_UNSUPPORTED(use_real);
  CodePageBackend backend;
  DriverManager driver;
  driver.set_backend(&backend);
  configure_driver(&driver, use_real);
  ASSERT_F_TRUE(driver.start());
  ASSERT_F_TRUE(driver.connect());

  backend.output = Response<uint32_t>::of(534);
  backend.input = Response<uint32_t>::of(653);
  DriverRequest cp0 = driver.get_console_output_cp();
  ASSERT_EQ(534, cp0->integer_value());
  DriverRequest cp1 = driver.get_console_cp();
  ASSERT_EQ(653, cp1->integer_value());

  DriverRequest cp2 = driver.set_console_output_cp(443);
  ASSERT_TRUE(cp2->bool_value());
  ASSERT_EQ(443, backend.output.value());

  DriverRequest cp3 = driver.get_console_output_cp();
  ASSERT_EQ(443, cp3->integer_value());
  DriverRequest cp4 = driver.get_console_cp();
  ASSERT_EQ(653, cp4->integer_value());


  ASSERT_F_TRUE(driver.join(NULL));
}
