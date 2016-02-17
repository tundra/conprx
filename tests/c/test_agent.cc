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

TEST(agent, simple) {
  DriverManager driver;
  ASSERT_TRUE(driver.enable_agent());
  ASSERT_FALSE(driver.agent_is_ready());
  ASSERT_TRUE(driver.start());
  ASSERT_FALSE(driver.agent_is_ready());
  ASSERT_TRUE(driver.connect());
  ASSERT_TRUE(driver.agent_is_ready());

  DriverRequest echo0 = driver.echo(5436);
  ASSERT_EQ(5436, echo0->integer_value());

  ASSERT_TRUE(driver.join());
}
