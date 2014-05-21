//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Code patching.

#include "binpatch.hh"
#include "test.hh"

TEST(binpatch, datatypes) {
  ASSERT_EQ(1, sizeof(byte_t));
}

int add_impl(int a, int b) {
  return a + b;
}

// Alias for add which hopefully eager compilers won't inline, unlike add_impl.
int (*add)(int, int) = add_impl;

int new_add(int a, int b) {
  return a + b + 1;
}

TEST(binpatch, simple_patching) {
  ASSERT_EQ(8, add(3, 5));
  BinaryPatch patch(FUNCAST(add), FUNCAST(new_add));
  ASSERT_EQ(BinaryPatch::NOT_APPLIED, patch.status());
  ASSERT_TRUE(patch.is_tentatively_possible());
  PatchEngine &engine = PatchEngine::get();
  ASSERT_TRUE(engine.ensure_initialized());
  ASSERT_TRUE(patch.apply(engine));
  ASSERT_EQ(BinaryPatch::APPLIED, patch.status());
  ASSERT_EQ(9, add(3, 5));
  ASSERT_TRUE(patch.revert(engine));
  ASSERT_EQ(BinaryPatch::NOT_APPLIED, patch.status());
  ASSERT_EQ(8, add(3, 5));
}
