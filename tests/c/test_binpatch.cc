//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Code patching.

#include "agent/binpatch.hh"
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
  PatchRequest patch(FUNCAST(add), FUNCAST(new_add), 0);
  ASSERT_EQ(PatchRequest::NOT_APPLIED, patch.status());
  ASSERT_TRUE(patch.is_tentatively_possible());
  MemoryManager &memman = MemoryManager::get();
  ASSERT_TRUE(memman.ensure_initialized());
  ASSERT_TRUE(patch.apply(memman));
  ASSERT_EQ(PatchRequest::APPLIED, patch.status());
  ASSERT_EQ(9, add(3, 5));
  if (false) {
    int (*old_add)(int, int) = patch.get_trampoline(add);
    ASSERT_EQ(8, old_add(3, 5));
  }
  ASSERT_TRUE(patch.revert(memman));
  ASSERT_EQ(PatchRequest::NOT_APPLIED, patch.status());
  ASSERT_EQ(8, add(3, 5));
}

TEST(binpatch, address_range) {
  PatchSet p0(MemoryManager::get(), Vector<PatchRequest>());
  Vector<byte_t> r0 = p0.determine_address_range();
  ASSERT_PTREQ(NULL, r0.start());

  PatchRequest p1s[1] = {
    PatchRequest(FUNCAST(7), NULL),
  };
  PatchSet p1(MemoryManager::get(), Vector<PatchRequest>(p1s, 1));
  Vector<byte_t> r1 = p1.determine_address_range();
  ASSERT_PTREQ(reinterpret_cast<address_t>(7), r1.start());
  ASSERT_PTREQ(reinterpret_cast<address_t>(8), r1.end());

  PatchRequest p3s[3] = {
    PatchRequest(FUNCAST(15), NULL),
    PatchRequest(FUNCAST(10), NULL),
    PatchRequest(FUNCAST(36), NULL)
  };
  PatchSet p3(MemoryManager::get(), Vector<PatchRequest>(p3s, 3));
  Vector<byte_t> r3 = p3.determine_address_range();
  ASSERT_PTREQ(reinterpret_cast<address_t>(10), r3.start());
  ASSERT_PTREQ(reinterpret_cast<address_t>(37), r3.end());
}

TEST(binpatch, fits_in_32_bits) {
  ASSERT_TRUE(PatchSet::offsets_fits_in_32_bits(0));
  ASSERT_TRUE(PatchSet::offsets_fits_in_32_bits(65536));
  ASSERT_TRUE(PatchSet::offsets_fits_in_32_bits(0x7FFFFFFE));
  ASSERT_TRUE(PatchSet::offsets_fits_in_32_bits(0x7FFFFFFF));
  ASSERT_FALSE(PatchSet::offsets_fits_in_32_bits(0x80000000));
  ASSERT_FALSE(PatchSet::offsets_fits_in_32_bits(0x80000001));
}
