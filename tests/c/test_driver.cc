//- Copyright 2016 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "io/iop.hh"
#include "rpc.hh"
#include "sync/process.hh"
#include "test.hh"

BEGIN_C_INCLUDES
#include "utils/string-inl.h"
END_C_INCLUDES

using namespace tclib;
using namespace plankton;

class Driver {
public:
  bool start();
  bool join();

private:
  NativeProcess process_;
  plankton::rpc::MessageSocket socket_;

  static utf8_t executable_path();
};

bool Driver::start() {
  return process_.start(executable_path(), 0, NULL);
}

bool Driver::join() {
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
  ASSERT_TRUE(driver.join());
}
