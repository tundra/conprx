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
  ASSERT_TRUE(driver.start());
  ASSERT_TRUE(driver.connect());

  DriverRequest echo0 = driver.echo(5436);
  ASSERT_EQ(5436, echo0->integer_value());

  ASSERT_TRUE(driver.join(NULL));
}

TEST(agent, inject_fail) {
  if (!DriverManager::kSupportsRealAgent)
    SKIP_TEST("requires dll injection");
  DriverManager driver;
  driver.set_agent_type(DriverManager::atReal);
  driver.set_agent_path(new_c_string("flip-flap-foobeliboo"));
  log_o *old_log = silence_global_log();
  ASSERT_FALSE(driver.start());
  set_global_log(old_log);
  ASSERT_TRUE(driver->ensure_process_resumed());
  int exit_code = 0;
  ASSERT_TRUE(driver.join(&exit_code));
  ASSERT_EQ(1, exit_code);
}
