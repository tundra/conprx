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
  bool make_trampoline = IF_MSVC(false, IF_64_BIT(true, false));
  ASSERT_EQ(8, add(3, 5));
  uint32_t flags = make_trampoline ? BinaryPatch::MAKE_TRAMPOLINE : BinaryPatch::NONE;
  BinaryPatch patch(FUNCAST(add), FUNCAST(new_add), flags);
  ASSERT_EQ(BinaryPatch::NOT_APPLIED, patch.status());
  ASSERT_TRUE(patch.is_tentatively_possible());
  PatchEngine &engine = PatchEngine::get();
  ASSERT_TRUE(engine.ensure_initialized());
  ASSERT_TRUE(patch.apply(engine));
  ASSERT_EQ(BinaryPatch::APPLIED, patch.status());
  ASSERT_EQ(9, add(3, 5));
  if (make_trampoline) {
    int (*old_add)(int, int) = patch.get_trampoline(add);
    ASSERT_EQ(8, old_add(3, 5));
  }
  ASSERT_TRUE(patch.revert(engine));
  ASSERT_EQ(BinaryPatch::NOT_APPLIED, patch.status());
  ASSERT_EQ(8, add(3, 5));
}
