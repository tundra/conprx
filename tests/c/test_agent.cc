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
