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

TEST(driver, simple) {
  DriverManager driver;
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
  NativeVariant out_var(&out);
  Handle *in = call2.echo(out_var).native_as(Handle::seed_type());
  ASSERT_TRUE(in != NULL);
  ASSERT_EQ(54234, in->id());

  DriverRequest call3 = driver.new_request();
  ASSERT_TRUE(call3.is_handle(out_var) == Variant::yes());

  DriverRequest call4 = driver.is_handle(100);
  ASSERT_TRUE(*call4 == Variant::no());

  ASSERT_TRUE(driver.join(NULL));
}

TEST(driver, get_std_handle) {
  DriverManager driver;
  ASSERT_TRUE(driver.start());
  ASSERT_TRUE(driver.connect());

  DriverRequest gsh10 = driver.get_std_handle(kStdInputHandle);
  ASSERT_TRUE(gsh10->native_as<Handle>()->is_valid());
  ASSERT_TRUE(gsh10->native_as<Handle>()->is_console());

  DriverRequest gsh11 = driver.get_std_handle(kStdOutputHandle);
  ASSERT_TRUE(gsh11->native_as<Handle>()->is_valid());
  ASSERT_TRUE(gsh11->native_as<Handle>()->is_console());

  DriverRequest gsh12 = driver.get_std_handle(kStdErrorHandle);
  ASSERT_TRUE(gsh12->native_as<Handle>()->is_valid());
  ASSERT_TRUE(gsh12->native_as<Handle>()->is_console());

  DriverRequest gsh1000 = driver.get_std_handle(1000);
  ASSERT_FALSE(gsh1000->native_as<Handle>()->is_valid());

  ASSERT_TRUE(driver.join(NULL));
}

TEST(driver, get_console_title) {
  DriverManager driver;
  ASSERT_TRUE(driver.start());
  ASSERT_TRUE(driver.connect());

  DriverRequest tit0 = driver.get_console_title_a(1024);
  ASSERT_TRUE(tit0->is_blob());

  DriverRequest settit0 = driver.set_console_title_a("Looky here!");
  ASSERT_TRUE(*settit0 == Variant::yes());

  DriverRequest tit1 = driver.get_console_title_a(1024);
  ASSERT_BLOBEQ(tclib::Blob("Looky here!", 12),
      tclib::Blob(tit1->blob_data(), tit1->blob_size()));

  ASSERT_TRUE(driver.join(NULL));
}

TEST(driver, errors) {
  DriverManager driver;
  ASSERT_TRUE(driver.start());
  ASSERT_TRUE(driver.connect());

  DriverRequest err = driver.raise_error(NtStatus::from(NtStatus::nsError,
      NtStatus::npCustomer, NtStatus::nfTerminal, 1090));
  ASSERT_TRUE(err.is_rejected());
  ConsoleError *result = err.error().native_as<ConsoleError>();
  ASSERT_EQ(1090, result->code());
  ASSERT_EQ(NtStatus::nsError, result->last_error().severity());
  ASSERT_EQ(NtStatus::npCustomer, result->last_error().provider());
  ASSERT_EQ(NtStatus::nfTerminal, result->last_error().facility());

  ASSERT_TRUE(driver.join(NULL));
}
