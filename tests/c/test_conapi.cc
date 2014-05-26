//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test/asserts.hh"
#include "test/unittest.hh"
#include "agent/conapi.hh"

TEST(conapi, datatypes) {
  size_t word_size = IF_64_BIT(8, 4);
  ASSERT_EQ(word_size, sizeof(dword_t));
  ASSERT_EQ(sizeof(void*), sizeof(handle_t));
}
